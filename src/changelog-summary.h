#pragma once
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

namespace relaxer {
// Presentation only: retain the complete, versioned resource and GA entries.
inline QJsonArray summarizedChangelog(const QJsonArray &history) {
    QJsonArray result;
    const QRegularExpression rc(QStringLiteral("^(?:Relaxer )?V(0\\.2\\.[02])-RC[0-9]+$"),
                                QRegularExpression::CaseInsensitiveOption);
    QStringList emitted;
    for (const auto &value : history) {
        const auto entry = value.toArray();
        if (entry.size() < 3) { result.append(value); continue; }
        const auto match = rc.match(entry.at(0).toString());
        if (!match.hasMatch()) { result.append(value); continue; }
        const auto series = match.captured(1);
        if (emitted.contains(series)) continue;
        emitted.append(series);
        QStringList details;
        for (const auto &source : history) {
            const auto item = source.toArray();
            if (item.size() < 3) continue;
            const auto candidate = rc.match(item.at(0).toString());
            if (candidate.hasMatch() && candidate.captured(1) == series)
                details.append(item.at(2).toString());
        }
        result.append(QJsonArray{QStringLiteral("V") + series + "-RC", entry.at(1), details.join("\n\n")});
    }
    return result;
}
}
