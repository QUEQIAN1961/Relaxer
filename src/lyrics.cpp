#include "lyrics.h"
#include <QRegularExpression>
#include <QStringConverter>
#include <algorithm>
namespace relaxer {
Lyrics Lyrics::parse(const QByteArray &bytes) {
    Lyrics out;
    QString text;
    if (bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff")) {
        auto decoder = QStringDecoder(QStringDecoder::Utf16);
        text = decoder(bytes);
    } else {
        auto utf = QStringDecoder(QStringDecoder::Utf8);
        text = utf(bytes);
        if (utf.hasError())
            text = QString::fromLocal8Bit(bytes);
    }
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    out.bilingual = text.contains("relaxer-bilingual");
    if(text.trimmed()==QString("纯音乐，请欣赏")){out.instrumental=true;out.lines.push_back({-1,"纯音乐，请欣赏"});return out;}
    QRegularExpression stamp("\\[(\\d+):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]");
    QRegularExpression meta("^\\[(?:ar|al|ti|by|re|ve|length):", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression off("\\[offset:([+-]?\\d+)\\]", QRegularExpression::CaseInsensitiveOption);
    auto om = off.match(text);
    qint64 offset = om.hasMatch() ? om.captured(1).toLongLong() : 0;
    QStringList plain;
    for (auto line : text.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || meta.match(line).hasMatch() || off.match(line).hasMatch())
            continue;
        auto it = stamp.globalMatch(line);
        QVector<qint64> times;
        while (it.hasNext()) {
            auto m = it.next();
            auto frac = m.captured(3);
            while (frac.size() < 3)
                frac += '0';
            times << m.captured(1).toLongLong() * 60000 + m.captured(2).toLongLong() * 1000 + frac.toInt() +
                         offset;
        }
        line.remove(stamp);
        if (times.isEmpty())
            plain << line;
        else
            for (auto time : times)
                out.lines.push_back({qMax<qint64>(0, time), line});
    }
    if (out.lines.isEmpty()) {
        for (const auto &l : plain)
            out.lines.push_back({-1, l});
    } else {
        out.timed = true;
        std::stable_sort(out.lines.begin(), out.lines.end(),
                         [](const auto &a, const auto &b) { return a.time < b.time; });
    }
    if(out.lines.size()==1&&out.lines.first().text.trimmed()==QString("纯音乐，请欣赏")){out.instrumental=true;out.timed=false;out.lines.first().time=-1;}
    return out;
}
int Lyrics::indexAt(qint64 ms) const {
    if (!timed || lines.isEmpty())
        return -1;
    auto it = std::upper_bound(lines.begin(), lines.end(), ms,
                               [](qint64 t, const LyricLine &l) { return t < l.time; });
    return int(it - lines.begin()) - 1;
}
} // namespace relaxer
