#include "lyrics-replacement-policy.h"
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QVector>

namespace relaxer {
namespace {
struct Row {
    QString raw, text, body, prefix, metadata;
    QVector<qint64> times;
    bool marker = false;
};
struct Pair { int source = -1; QVector<int> translations; };
struct Document {
    QVector<Row> rows;
    QVector<Pair> pairs;
    bool marked = false, translation = false, safe = true;
};
QString decode(const QByteArray &bytes) {
    QStringDecoder decoder(bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff")
                               ? QStringDecoder::Utf16 : QStringDecoder::Utf8);
    auto text = QString(decoder(bytes));
    if (decoder.hasError()) text = QString::fromLocal8Bit(bytes);
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    return text.replace("\r\n", "\n").replace('\r', '\n');
}
Document parse(const QByteArray &bytes) {
    static const QRegularExpression metadata(R"(^\s*\[([A-Za-z][A-Za-z0-9_-]*):([^\[\]]*)\]\s*$)");
    static const QRegularExpression stamp(R"(\[(\d+):(\d{1,2})(?:[.:](\d{1,3}))?\])");
    Document doc;
    qint64 offset = 0;
    int offsets = 0;
    const auto text = decode(bytes);
    for (const auto &raw : text.split('\n')) {
        Row row; row.raw = raw;
        const auto meta = metadata.match(raw);
        if (meta.hasMatch()) {
            row.metadata = meta.captured(1).toLower();
            row.marker = row.metadata == "re" && meta.captured(2).compare("relaxer-bilingual", Qt::CaseInsensitive) == 0;
            doc.marked |= row.marker;
            if (row.metadata == "offset") {
                bool ok = false; offset = meta.captured(2).toLongLong(&ok);
                // Keep arithmetic bounded and ambiguous offsets reviewable.
                doc.safe &= ok && offset >= -86400000 && offset <= 86400000 && ++offsets == 1;
            }
        } else {
            auto matches = stamp.globalMatch(raw);
            int end = 0;
            while (matches.hasNext()) {
                const auto m = matches.next();
                if (!raw.mid(end, m.capturedStart() - end).trimmed().isEmpty()) doc.safe = false;
                auto fraction = m.captured(3); while (fraction.size() < 3) fraction += '0';
                bool ok = false; const auto minutes = m.captured(1).toLongLong(&ok);
                if (!ok || minutes > 1000000 || m.captured(2).toInt() >= 60) { doc.safe = false; continue; }
                row.times.append(minutes * 60000 + m.captured(2).toInt() * 1000 + fraction.toInt());
                end = int(m.capturedEnd());
            }
            row.prefix = raw.left(end).trimmed();
            row.body = raw.mid(end).trimmed();
            row.text = row.body.normalized(QString::NormalizationForm_C);
        }
        doc.rows.append(row);
    }
    QVector<int> body;
    for (int i = 0; i < doc.rows.size(); ++i) {
        auto &row = doc.rows[i];
        for (auto &time : row.times) time += offset;
        if (row.metadata.isEmpty() && !row.text.isEmpty()) body.append(i);
    }
    if (doc.marked) {
        doc.translation = true;
        if (body.size() % 2) doc.safe = false;
        for (int i = 0; i < body.size(); i += 2) {
            Pair pair{body[i], {}};
            if (i + 1 < body.size()) {
                pair.translations.append(body[i + 1]);
                if (doc.rows[body[i]].times != doc.rows[body[i + 1]].times) doc.safe = false;
            }
            doc.pairs.append(pair);
        }
    } else {
        // Legacy human translations use adjacent equal timestamps. Never infer
        // alignment from row counts or language; unmatched structures need review.
        QSet<QVector<qint64>> seen;
        for (int index : body) {
            const auto &times = doc.rows[index].times;
            if (!times.isEmpty() && !doc.pairs.isEmpty() && doc.rows[doc.pairs.last().source].times == times) {
                doc.pairs.last().translations.append(index); doc.translation = true;
            } else {
                if (!times.isEmpty() && seen.contains(times)) doc.safe = false;
                seen.insert(times); doc.pairs.append(Pair{index, {}});
            }
        }
    }
    return doc;
}
bool equivalent(const Document &a, const Document &b) {
    if (!a.safe || !b.safe || a.pairs.isEmpty() || a.pairs.size() != b.pairs.size()) return false;
    // A new unmarked equal-time row could be a simultaneous original rather
    // than a translation. Never remove candidate rows on that inference.
    if (b.translation && !b.marked) return false;
    for (int i = 0; i < a.pairs.size(); ++i) {
        const auto &x = a.rows[a.pairs[i].source], &y = b.rows[b.pairs[i].source];
        if (x.text != y.text || x.times != y.times) return false;
    }
    QVector<QVector<qint64>> blankA, blankB;
    for (const auto &row : a.rows) if (row.text.isEmpty() && !row.times.isEmpty()) blankA.append(row.times);
    for (const auto &row : b.rows) if (row.text.isEmpty() && !row.times.isEmpty()) blankB.append(row.times);
    if (blankA != blankB) return false;
    return true;
}
QByteArray preserve(const Document &old, const Document &candidate) {
    QStringList result;
    QSet<QString> candidateMetadata;
    for (const auto &row : candidate.rows) if (!row.metadata.isEmpty() && !row.marker) candidateMetadata.insert(row.metadata);
    bool completePairs = true;
    for (int i = 0; i < old.pairs.size(); ++i)
        completePairs &= (old.pairs[i].translations.isEmpty() ? candidate.pairs[i].translations : old.pairs[i].translations).size() == 1;
    if ((old.marked || candidate.marked) && completePairs) result.append("[re:relaxer-bilingual]");
    // Explicit candidate metadata wins; retain old tags missing from the new
    // version, except offset, whose value belongs to the candidate's timeline.
    for (const auto &row : old.rows)
        if (!row.metadata.isEmpty() && !row.marker && row.metadata != "offset" && !candidateMetadata.contains(row.metadata)) result.append(row.raw);
    QSet<int> translatedRows;
    for (const auto &pair : candidate.pairs) for (int i : pair.translations) translatedRows.insert(i);
    int pairIndex = 0;
    for (int i = 0; i < candidate.rows.size(); ++i) {
        const auto &row = candidate.rows[i];
        if (row.marker || translatedRows.contains(i)) continue;
        result.append(row.raw);
        if (pairIndex < candidate.pairs.size() && candidate.pairs[pairIndex].source == i) {
            const bool saved = !old.pairs[pairIndex].translations.isEmpty();
            const auto &translations = saved ? old.pairs[pairIndex].translations : candidate.pairs[pairIndex].translations;
            for (int translated : translations)
                result.append(row.prefix + (saved ? old.rows[translated].body : candidate.rows[translated].body));
            ++pairIndex;
        }
    }
    return result.join('\n').toUtf8();
}
}
LyricsReplacementDecision LyricsReplacementPolicy::analyze(const QByteArray &existing, const QByteArray &candidate) {
    LyricsReplacementDecision result; result.content = candidate;
    const auto next = parse(candidate), old = parse(existing);
    result.valid = !candidate.isEmpty() && candidate.size() <= 4 * 1024 * 1024 && !next.pairs.isEmpty();
    if (!result.valid) { result.reason = QStringLiteral("歌词为空、格式无效或超过 4 MB。"); return result; }
    result.existingTranslation = old.translation;
    result.equivalent = equivalent(old, next);
    if (existing == candidate || existing.trimmed().isEmpty()) return result;
    if (result.equivalent && old.translation) {
        result.content = preserve(old, next); result.preservesTranslation = true;
        result.reason = QStringLiteral("原文与时间轴一致；保留已有译文并采用候选元数据。");
    } else if (!result.equivalent || next.translation) {
        result.confirmationRequired = true;
        result.reason = old.translation
            ? QStringLiteral("候选原文、时间轴或双语结构与本地版本不同，无法安全套用已有译文。替换将覆盖已有译文。")
            : QStringLiteral("候选内容与已有歌词不同，替换将覆盖本地版本。");
    }
    return result;
}
}
