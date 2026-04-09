#pragma once
#ifndef DATA_STORE_H
#define DATA_STORE_H

#include "NewsTypes.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <string>
#include <vector>

// Writes scored news to the SQL Server Docker container (news table)
class DataStore {
public:
    // conn_string: SQL Server ODBC connection string
    // password: SQL Server password (from PASSWORD_SQL env var)
    DataStore(const std::string& server,
              const std::string& database,
              const std::string& user,
              const std::string& password);
    ~DataStore();

    bool isConnected() const;

    // Ensure the news table exists
    bool ensureTable();

    // Insert or update a scored news item
    bool saveScored(const NewsScored& scored);

    // Batch insert multiple scored items
    int saveScoredBatch(const std::vector<NewsScored>& items);

private:
    QSqlDatabase db;
    QString m_connectionName;
};

#endif // DATA_STORE_H
