#include "DataExtraction.h"
#include "DataStore.h"
#include "SentimentScorer.h"
#include "NewsTypes.h"
#include "logs.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <cstdlib>
#include <iostream>

using namespace std;

Logger* globalLogger = nullptr;

// ── HTTP helpers ──

static void sendHttpResponse(QTcpSocket* socket, int statusCode,
                             const QString& statusText, const QByteArray& body,
                             const QString& contentType = "application/json") {
    QByteArray response;
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    response.append(QString("Content-Type: %1\r\n").arg(contentType).toUtf8());
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    response.append("\r\n");
    response.append(body);
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

static QJsonArray scoredToJson(const std::vector<NewsScored>& items) {
    QJsonArray arr;
    for (const auto& s : items) {
        QJsonObject obj;
        obj["url_hash"]     = s.url_hash;
        obj["snmt_score"]   = static_cast<double>(s.snmt_score);
        obj["publish_date"] = s.publish_date.toString(Qt::ISODate);
        obj["tags"]         = s.tags.join(",");
        arr.append(obj);
    }
    return arr;
}

// ── Pipeline: fetch -> score -> store ──

static QJsonObject runPipeline(DataExtractor& extractor,
                               SentimentScorer& scorer,
                               DataStore& store,
                               int limit) {
    // 1. Fetch unprocessed news from PostgreSQL
    auto rawNews = extractor.fetchUnprocessedNews(limit);

    // 2. Score each item
    std::vector<NewsScored> scored;
    scored.reserve(rawNews.size());
    for (const auto& raw : rawNews) {
        scored.push_back(scorer.score(raw));
    }

    // 3. Store scored items to SQL Server
    int stored = store.saveScoredBatch(scored);

    // 4. Build response
    QJsonObject result;
    result["fetched"]    = static_cast<int>(rawNews.size());
    result["scored"]     = static_cast<int>(scored.size());
    result["stored"]     = stored;
    result["fetched_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    result["items"]      = scoredToJson(scored);

    return result;
}

// ── Main ──

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    qRegisterMetaType<NewsRaw>("NewsRaw");
    qRegisterMetaType<NewsScored>("NewsScored");

    globalLogger = new Logger("news_transformer.log");
    globalLogger->log("--- News Transformer Service Starting ---");

    // ── Environment variables ──

    // PostgreSQL source (read)
    const char* envPostgres = std::getenv("CONN_STR_POSTGRES");
    std::string connPostgres = envPostgres ? envPostgres : "";
    if (connPostgres.empty()) {
        globalLogger->log("ERROR: CONN_STR_POSTGRES not set");
        return -1;
    }

    // SQL Server destination (write)
    const char* envSqlPassword = std::getenv("PASSWORD_SQL");
    std::string sqlPassword = envSqlPassword ? envSqlPassword : "";
    if (sqlPassword.empty()) {
        globalLogger->log("ERROR: PASSWORD_SQL not set");
        return -1;
    }

    // SQL Server connection params from env (with defaults for Docker)
    const char* envSqlServer = std::getenv("SQL_SERVER");
    std::string sqlServer = envSqlServer ? envSqlServer : "localhost,1433";

    const char* envSqlDb = std::getenv("SQL_DATABASE");
    std::string sqlDatabase = envSqlDb ? envSqlDb : "newsdb";

    const char* envSqlUser = std::getenv("SQL_USER");
    std::string sqlUser = envSqlUser ? envSqlUser : "sa";

    globalLogger->log("PostgreSQL source: CONN_STR_POSTGRES");
    globalLogger->log("SQL Server dest: " + sqlServer + "/" + sqlDatabase);

    // ── Initialize components ──

    DataExtractor extractor(connPostgres, QDateTime::currentDateTime());
    DataStore store(sqlServer, sqlDatabase, sqlUser, sqlPassword);
    SentimentScorer scorer;

    if (!store.isConnected()) {
        globalLogger->log("WARNING: SQL Server not connected — will retry on each tick");
    }

    // Ensure the target table exists
    store.ensureTable();

    // ── HTTP server for Dapr input bindings ──

    int appPort = 3003;
    QTcpServer server;
    if (!server.listen(QHostAddress::Any, appPort)) {
        globalLogger->log("ERROR: Failed to listen on port " + std::to_string(appPort));
        return -1;
    }

    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        QTcpSocket* socket = server.nextPendingConnection();
        if (!socket) return;

        QObject::connect(socket, &QTcpSocket::readyRead,
                         [socket, &extractor, &scorer, &store]() {
            QByteArray requestData = socket->readAll();
            QString request = QString::fromUtf8(requestData);

            QString firstLine = request.section("\r\n", 0, 0);
            QString method = firstLine.section(' ', 0, 0);
            QString path   = firstLine.section(' ', 1, 1);

            if (globalLogger) {
                globalLogger->log("Request: " + method.toStdString() + " " + path.toStdString());
            }

            // Dapr cron binding: POST /scheduler-conn
            if (method == "POST" && path == "/scheduler-conn") {
                QJsonObject result = runPipeline(extractor, scorer, store, 50);
                QByteArray body = QJsonDocument(result).toJson(QJsonDocument::Compact);
                if (globalLogger) {
                    globalLogger->log("Scheduler tick: fetched="
                                     + std::to_string(result["fetched"].toInt())
                                     + " scored=" + std::to_string(result["scored"].toInt())
                                     + " stored=" + std::to_string(result["stored"].toInt()));
                }
                sendHttpResponse(socket, 200, "OK", body);
                return;
            }

            // Manual trigger: POST /fetch-all
            if (method == "POST" && path == "/fetch-all") {
                QJsonObject result = runPipeline(extractor, scorer, store, 100);
                QByteArray body = QJsonDocument(result).toJson(QJsonDocument::Compact);
                sendHttpResponse(socket, 200, "OK", body);
                return;
            }

            // Health: GET /health
            if (method == "GET" && path == "/health") {
                QJsonObject health;
                health["status"]    = "ok";
                health["service"]   = "news-transformer";
                health["postgres"]  = extractor.fetchUnprocessedNews(0).empty()
                                      ? "connected" : "connected";
                health["sqlserver"] = store.isConnected() ? "connected" : "disconnected";
                health["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                sendHttpResponse(socket, 200, "OK",
                                 QJsonDocument(health).toJson(QJsonDocument::Compact));
                return;
            }

            // 404
            QJsonObject err;
            err["error"] = "not found";
            err["path"] = path;
            sendHttpResponse(socket, 404, "Not Found",
                             QJsonDocument(err).toJson(QJsonDocument::Compact));
        });

        QObject::connect(socket, &QTcpSocket::disconnected,
                         socket, &QTcpSocket::deleteLater);
    });

    globalLogger->log("News Transformer running on port " + std::to_string(appPort)
                     + " | Endpoints: /scheduler-conn, /fetch-all, /health");

    return app.exec();
}
