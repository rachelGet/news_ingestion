#pragma once
#ifndef DATA_EXTRACTOR_H
#define DATA_EXTRACTOR_H

#include "NewsTypes.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QString>
#include <string>
#include <vector>

class DataExtractor {
public:
    DataExtractor(const std::string& conn_string, QDateTime actual_date);
    ~DataExtractor();

    std::vector<NewsRaw> fetchUnprocessedNews(int limit = 50);

private:
    QSqlDatabase db;
    QString m_connectionName;
    QDateTime m_actual_date;
};

#endif // DATA_EXTRACTOR_H
