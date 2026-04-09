#include "DataExtraction.h"
#include "logs.h"

#include <QUuid>
#include <QDateTime>
#include <QUrl>

extern Logger* globalLogger;

DataExtractor::DataExtractor(const std::string& conn_string, QDateTime actual_date)
    : m_actual_date(actual_date) {

    m_connectionName = QUuid::createUuid().toString();
    db = QSqlDatabase::addDatabase("QPSQL", m_connectionName);

    // Parse postgres:// connection string into QSqlDatabase fields
    QUrl connUrl(QString::fromStdString(conn_string));
    db.setHostName(connUrl.host());
    db.setPort(connUrl.port(5432));
    db.setDatabaseName(connUrl.path().mid(1)); // remove leading '/'
    db.setUserName(connUrl.userName());
    db.setPassword(connUrl.password());

    if (!db.open()) {
        if (globalLogger) {
            globalLogger->log("Database Error: " + db.lastError().text().toStdString());
        }
    } else {
        if (globalLogger) {
            globalLogger->log("Database connected successfully to " + connUrl.host().toStdString());
        }
    }
}

DataExtractor::~DataExtractor() {
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

std::vector<NewsRaw> DataExtractor::fetchUnprocessedNews(int limit) {
    std::vector<NewsRaw> results;
    QSqlQuery query(db);

    query.prepare("SELECT url_hash, url, updated_at, post, priority, in_process, processed "
                  "FROM news_table "
                  "WHERE processed = false AND in_process = false "
                  "LIMIT :limit");
    query.bindValue(":limit", limit);

    if (query.exec()) {
        while (query.next()) {
            NewsRaw row;
            row.url_hash   = query.value("url_hash").toString();
            row.url        = query.value("url").toString();
            row.updated_at = query.value("updated_at").toDateTime();
            row.post       = query.value("post").toString();
            row.priority   = query.value("priority").toString();
            row.in_process = query.value("in_process").toBool();
            row.processed  = query.value("processed").toBool();
            results.push_back(row);
        }
        if (globalLogger) {
            globalLogger->log("Fetched " + std::to_string(results.size()) + " unprocessed news items");
        }
    } else {
        if (globalLogger) {
            globalLogger->log("Query error: " + query.lastError().text().toStdString());
        }
    }
    return results;
}
