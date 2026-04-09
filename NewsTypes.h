#pragma once
#ifndef MACRO_TYPES_H
#define MACRO_TYPES_H

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

struct NewsRaw {
    QString url_hash;
    QString url;
    QDateTime updated_at;
    QString post;
    QString priority;
    bool in_process = false;
    bool processed = false;
};

struct PostContent {
    QStringList tags;
    QString title;
    QString author;
    QString content;
    QString time_ago;
    QDateTime publish_date;

    static PostContent fromJson(const QString& jsonStr) {
        PostContent pc;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            
            pc.title = obj["title"].toString();
            pc.author = obj["author"].toString();
            pc.content = obj["content"].toString();
            pc.time_ago = obj["time_ago"].toString();
            
            pc.publish_date = QDateTime::fromString(obj["publish_date"].toString(), Qt::ISODate);
            
            QJsonArray tagsArray = obj["tags"].toArray();
            for (const QJsonValue &value : tagsArray) {
                pc.tags << value.toString();
            }
        }
        return pc;
    }
};

struct NewsScored
{
    QString url_hash; 
    float snmt_score;
    QDateTime publish_date;
    QStringList tags;
};


Q_DECLARE_METATYPE(NewsRaw)
Q_DECLARE_METATYPE(NewsScored)

#endif // MACRO_TYPES_H
