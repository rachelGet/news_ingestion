#include "SentimentScorer.h"
#include "logs.h"

extern Logger* globalLogger;

SentimentScorer::SentimentScorer() {
    m_positiveWords = {
        "bull", "bullish", "rally", "surge", "gain", "gains", "profit",
        "growth", "recovery", "upbeat", "optimism", "optimistic", "rise",
        "rising", "strong", "strength", "boost", "positive", "upgrade",
        "outperform", "beat", "exceed", "record", "high", "buy",
        "accumulate", "breakout", "momentum", "expansion", "rebound"
    };

    m_negativeWords = {
        "bear", "bearish", "crash", "plunge", "loss", "losses", "decline",
        "recession", "downturn", "pessimism", "pessimistic", "fall",
        "falling", "weak", "weakness", "drop", "negative", "downgrade",
        "underperform", "miss", "default", "risk", "sell", "fear",
        "correction", "collapse", "contraction", "inflation", "crisis",
        "bankruptcy", "layoff", "layoffs", "warning", "debt"
    };
}

NewsScored SentimentScorer::score(const NewsRaw& raw) {
    NewsScored scored;
    scored.url_hash = raw.url_hash;

    // Parse the post JSON to extract content and metadata
    PostContent content = PostContent::fromJson(raw.post);

    scored.publish_date = content.publish_date.isValid()
                          ? content.publish_date
                          : raw.updated_at;
    scored.tags = content.tags;

    // Score based on title + content combined
    QString fullText = content.title + " " + content.content;
    scored.snmt_score = computeScore(fullText);

    if (globalLogger) {
        globalLogger->log("Scored [" + raw.url_hash.toStdString() + "]: "
                         + std::to_string(scored.snmt_score)
                         + " title=\"" + content.title.left(60).toStdString() + "\"");
    }

    return scored;
}

float SentimentScorer::computeScore(const QString& text) {
    QString lower = text.toLower();
    int positive = 0;
    int negative = 0;

    for (const auto& word : m_positiveWords) {
        if (lower.contains(word)) positive++;
    }
    for (const auto& word : m_negativeWords) {
        if (lower.contains(word)) negative++;
    }

    int total = positive + negative;
    if (total == 0) return 0.0f;

    // Normalize to [-1.0, 1.0]
    return static_cast<float>(positive - negative) / static_cast<float>(total);
}
