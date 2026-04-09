#pragma once
#ifndef SENTIMENT_SCORER_H
#define SENTIMENT_SCORER_H

#include "NewsTypes.h"
#include <QStringList>
#include <QMap>

class SentimentScorer {
public:
    SentimentScorer();

    // Score a single news item: parses post JSON, computes sentiment [-1.0, 1.0]
    NewsScored score(const NewsRaw& raw);

private:
    float computeScore(const QString& text);

    QStringList m_positiveWords;
    QStringList m_negativeWords;
};

#endif // SENTIMENT_SCORER_H
