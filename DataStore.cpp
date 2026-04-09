#include "DataStore.h"
#include "logs.h"

#include <QUuid>
#include <QSqlDriver>

extern Logger* globalLogger;

DataStore::DataStore(const std::string& server,
                     const std::string& database,
                     const std::string& user,
                     const std::string& password) {
    m_connectionName = "sqlserver_" + QUuid::createUuid().toString();
    db = QSqlDatabase::addDatabase("QODBC", m_connectionName);

    // Build ODBC connection string for SQL Server
    QString connStr = QString(
        "DRIVER={ODBC Driver 17 for SQL Server};"
        "SERVER=%1;"
        "DATABASE=%2;"
        "UID=%3;"
        "PWD=%4;"
        "TrustServerCertificate=yes;"
    ).arg(QString::fromStdString(server),
          QString::fromStdString(database),
          QString::fromStdString(user),
          QString::fromStdString(password));

    db.setDatabaseName(connStr);

    if (!db.open()) {
        if (globalLogger) {
            globalLogger->log("SQL Server Error: " + db.lastError().text().toStdString());
        }
    } else {
        if (globalLogger) {
            globalLogger->log("SQL Server connected to " + server + "/" + database);
        }
    }
}

DataStore::~DataStore() {
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DataStore::isConnected() const {
    return db.isOpen();
}

bool DataStore::ensureTable() {
    QSqlQuery query(db);

    bool ok = query.exec(
        "IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='news' AND xtype='U') "
        "CREATE TABLE news ("
        "    [url_hash]      NVARCHAR(512) NOT NULL PRIMARY KEY, "
        "    [snmt_score]    FLOAT NOT NULL, "
        "    [publish_date]  DATETIME2 NULL, "
        "    [tags]          NVARCHAR(MAX) NOT NULL "
        ")"
    );

    if (!ok) {
        if (globalLogger) {
            globalLogger->log("ensureTable error: " + query.lastError().text().toStdString());
        }
    } else {
        if (globalLogger) {
            globalLogger->log("ensureTable: news table ready");
        }
    }
    return ok;
}

bool DataStore::saveScored(const NewsScored& scored) {
    QSqlQuery query(db);

    // MERGE (upsert) — insert or update if url_hash already exists
    query.prepare(
        "MERGE INTO news AS target "
        "USING (SELECT :url_hash AS url_hash) AS source "
        "ON target.url_hash = source.url_hash "
        "WHEN MATCHED THEN "
        "    UPDATE SET snmt_score = :score_upd, "
        "               publish_date = :date_upd, "
        "               tags = :tags_upd "
        "WHEN NOT MATCHED THEN "
        "    INSERT (url_hash, snmt_score, publish_date, tags) "
        "    VALUES (:url_hash_ins, :score_ins, :date_ins, :tags_ins);"
    );

    QString tagsStr = scored.tags.join(",");

    query.bindValue(":url_hash", scored.url_hash);
    query.bindValue(":score_upd", static_cast<double>(scored.snmt_score));
    query.bindValue(":date_upd", scored.publish_date);
    query.bindValue(":tags_upd", tagsStr);
    query.bindValue(":url_hash_ins", scored.url_hash);
    query.bindValue(":score_ins", static_cast<double>(scored.snmt_score));
    query.bindValue(":date_ins", scored.publish_date);
    query.bindValue(":tags_ins", tagsStr);

    if (!query.exec()) {
        if (globalLogger) {
            globalLogger->log("saveScored error [" + scored.url_hash.toStdString() + "]: "
                             + query.lastError().text().toStdString());
        }
        return false;
    }
    return true;
}

int DataStore::saveScoredBatch(const std::vector<NewsScored>& items) {
    int saved = 0;
    for (const auto& item : items) {
        if (saveScored(item)) {
            saved++;
        }
    }
    if (globalLogger) {
        globalLogger->log("Batch save: " + std::to_string(saved) + "/"
                         + std::to_string(items.size()) + " items stored to SQL Server");
    }
    return saved;
}
