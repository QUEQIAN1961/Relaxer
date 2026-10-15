#include "lyric-text.h"
#include "lyrics.h"
#include <QHash>
#include <QRegularExpression>
namespace relaxer {
namespace {
const QHash<char16_t, char16_t> &traditionalMap() {
    static const QHash<char16_t, char16_t> map = [] {
        QHash<char16_t, char16_t> out;
#include "opencc-st.inc"
        const int count = int(sizeof(kTradMap) / sizeof(kTradMap[0]));
        for (int i = 0; i + 1 < count; i += 2)
            if (kTradMap[i][0] && kTradMap[i + 1][0])
                out.insert(kTradMap[i + 1][0], kTradMap[i][0]);
        return out;
    }();
    return map;
}
QString lyricBody(const QByteArray &data) {
    QString text;
    for (const auto &line : Lyrics::parse(data).lines)
        text += line.text + QChar('\n');
    return text;
}
} // namespace
QString toSimplified(const QString &text) {
    const auto &map = traditionalMap();
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (c.isHighSurrogate() || c.isLowSurrogate()) {
            out += c;
            continue;
        }
        const auto it = map.constFind(c.unicode());
        out += it == map.cend() ? c : QChar(it.value());
    }
    return out;
}
bool containsTraditional(const QString &text) {
    const auto &map = traditionalMap();
    for (const QChar c : text)
        if (!c.isHighSurrogate() && !c.isLowSurrogate() && map.contains(c.unicode()))
            return true;
    return false;
}
bool lyricsAreChinese(const QString &text) {
    int han = 0, kana = 0, hangul = 0, letters = 0;
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (c.script() == QChar::Script_Han) {
            ++han;
            ++letters;
        } else if ((u >= 0x3040 && u <= 0x30ff) || (u >= 0x31f0 && u <= 0x31ff)) {
            ++kana;
            ++letters;
        } else if (u >= 0xac00 && u <= 0xd7af) {
            ++hangul;
            ++letters;
        } else if (c.isLetter())
            ++letters;
    }
    if (kana > 2 || hangul > 2)
        return false;
    return letters > 0 && han * 2 >= letters;
}
OriginalCredit originalCredit(const QString &title) {
    OriginalCredit credit;
    const QString source = title.normalized(QString::NormalizationForm_KC);
    QRegularExpression marked(
        QString("[\\[\\(（【]\\s*(?:原唱|原曲|原版|original)\\s*[:：]\\s*([^\\]\\)）】]+)\\s*[\\]\\)）】]"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpression cover(
        QString("[\\[\\(（【]\\s*(?:cover(?:ed)?(?:\\s+by)?|翻唱)\\s*[:：]?\\s*([^\\]\\)）】]+)\\s*[\\]\\)）】]"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpression from(QString("(?:翻自|原唱)\\s*[:：]?\\s*([^\\[\\]()（）【】]+)$"),
                            QRegularExpression::CaseInsensitiveOption);
    QString artist;
    QRegularExpressionMatch match = marked.match(source);
    if (!match.hasMatch())
        match = cover.match(source);
    if (match.hasMatch())
        artist = match.captured(1).trimmed();
    else if ((match = from.match(source)).hasMatch())
        artist = match.captured(1).trimmed();
    if (artist.isEmpty())
        return credit;
    QString clean = source;
    clean.remove(marked);
    clean.remove(cover);
    clean.remove(from);
    clean = clean.simplified();
    if (clean.isEmpty())
        return credit;
    credit.found = true;
    credit.artist = artist;
    credit.title = clean;
    return credit;
}
QByteArray simplifyTraditionalLyrics(const QByteArray &data) {
    const auto lyric = Lyrics::parse(data);
    const QString body = lyricBody(data);
    if (!lyricsAreChinese(body) || !containsTraditional(body))
        return data;
    QString text = QString::fromUtf8(data);
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    QStringList output;
    const QRegularExpression stamp("\\[(\\d+):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]");
    for (const auto &raw : text.split('\n')) {
        QString line = raw;
        if (!line.contains(QChar('[')) || !stamp.match(line).hasMatch()) {
            output << toSimplified(line);
            continue;
        }
        const int content = line.lastIndexOf(']') + 1;
        output << line.left(content) + toSimplified(line.mid(content));
    }
    return output.join("\n").toUtf8();
}
bool lyricsIncludeTranslation(const QByteArray &data) {
    if (QString::fromUtf8(data).contains("relaxer-bilingual"))
        return true;
    const auto lyric = Lyrics::parse(data);
    if (!lyric.timed)
        return false;
    QHash<qint64, int> seen;
    for (const auto &line : lyric.lines)
        if (++seen[line.time] > 1)
            return true;
    return false;
}
QByteArray sourceLyricsForTranslation(const QByteArray &data) {
    const QString text = QString::fromUtf8(data);
    if (text.contains("relaxer-bilingual")) {
        QStringList rows;
        bool body = false, original = true;
        for (const auto &raw : text.split('\n')) {
            if (!body) {
                if (raw.contains("relaxer-bilingual")) {
                    body = true;
                    continue;
                }
                rows << raw;
                continue;
            }
            if (original)
                rows << raw;
            original = !original;
        }
        return rows.join('\n').toUtf8();
    }
    const QRegularExpression stamp("^((?:\\[\\d+:\\d{1,2}(?:[.:]\\d{1,3})?\\])+)");
    QHash<QString, int> seen;
    QStringList rows;
    for (const auto &raw : text.split('\n')) {
        const auto matched = stamp.match(raw.trimmed());
        if (!matched.hasMatch()) {
            rows << raw;
            continue;
        }
        if (++seen[matched.captured(1)] > 1)
            continue;
        rows << raw;
    }
    return rows.join('\n').toUtf8();
}
QStringList lyricTexts(const QByteArray &data) {
    QStringList lines;
    for (const auto &line : Lyrics::parse(data).lines)
        if (!line.text.trimmed().isEmpty())
            lines << line.text.trimmed();
    return lines;
}
QByteArray attachTranslations(const QByteArray &data, const QStringList &translations) {
    if (translations.isEmpty())
        return data;
    const auto lyric = Lyrics::parse(data);
    if (!lyric.timed) {
        QStringList rows{QString("[re:relaxer-bilingual]")};
        for (int i = 0; i < lyric.lines.size(); ++i) {
            rows << lyric.lines[i].text;
            if (i < translations.size() && !translations[i].trimmed().isEmpty())
                rows << translations[i].trimmed();
        }
        return rows.join("\n").toUtf8();
    }
    QStringList rows;
    int index = 0;
    QString previous;
    const QRegularExpression stamp("\\[(\\d+):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]");
    for (const auto &raw : QString::fromUtf8(data).split('\n')) {
        rows << raw;
        if (!stamp.match(raw).hasMatch())
            continue;
        const QString body = raw.mid(raw.lastIndexOf(']') + 1).trimmed();
        if (body.isEmpty())
            continue;
        if (index < translations.size() && !translations[index].trimmed().isEmpty() &&
            translations[index].trimmed() != body) {
            const QString prefix = raw.left(raw.lastIndexOf(']') + 1);
            if (previous != prefix)
                rows << prefix + translations[index].trimmed();
            previous = prefix;
        }
        ++index;
    }
    return rows.join("\n").toUtf8();
}
QString translationLanguagePair(const QString &sample) {
    int kana = 0, hangul = 0, cyrillic = 0, han = 0;
    for (const QChar c : sample) {
        const ushort u = c.unicode();
        if ((u >= 0x3040 && u <= 0x30ff))
            ++kana;
        else if (u >= 0xac00 && u <= 0xd7af)
            ++hangul;
        else if (u >= 0x0400 && u <= 0x04ff)
            ++cyrillic;
        else if (c.script() == QChar::Script_Han)
            ++han;
    }
    if (kana > 0)
        return "ja|zh-CN";
    if (hangul > 0)
        return "ko|zh-CN";
    if (cyrillic > han)
        return "ru|zh-CN";
    return "en|zh-CN";
}
bool isMidiSuffix(const QString &suffix) {
    const auto s = suffix.toLower();
    return s == "mid" || s == "midi" || s == "kar";
}
bool playsWithoutFfmpeg(const QString &suffix) {
    const auto s = suffix.toLower();
    return QStringList{"mp3", "flac", "wav", "m4a", "aac", "aiff", "aif", "wma", "mp2", "m4b"}.contains(s) ||
           isMidiSuffix(s);
}
bool needsTranscodedPlayback(const QString &suffix) {
    const auto s = suffix.toLower();
    return QStringList{"ogg", "opus", "oga", "ape", "wv", "tta", "mpc", "mp+", "ac3", "amr", "caf", "mka"}.contains(s);
}
QString storedAudioSuffix(const QString &suffix) {
    const auto s = suffix.toLower();
    return needsTranscodedPlayback(s) ? QString("flac") : s;
}
} // namespace relaxer
