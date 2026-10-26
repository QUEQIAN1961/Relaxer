#include "lyrics-translation-classification.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringDecoder>

#include <algorithm>

namespace relaxer {
namespace {

TranslationClassification result(TranslationStatus status, bool needs, bool completed, bool unsaved,
                                 const QString &reason, const QString &badge) {
    TranslationClassification value;
    value.status = status;
    value.needsTranslation = needs;
    value.completed = completed;
    value.unsavedResult = unsaved;
    value.reason = reason;
    value.badge = badge;
    return value;
}

} // namespace

bool LyricsTranslationClassification::verifiedMarkedBilingual(const QByteArray &bytes, const QString &title,
                                                              const QString &artist) {
    // A merge marker alone is insufficient: every source line must have a
    // matching saved partner. Reuse the production response validator.
    static const QRegularExpression marker(QStringLiteral("^\\s*\\[re:relaxer-bilingual\\]\\s*$"),
                                           QRegularExpression::MultilineOption |
                                               QRegularExpression::CaseInsensitiveOption);
    QStringDecoder decoder(QStringConverter::Utf8);
    QString text = decoder(bytes);
    if (decoder.hasError()) return false;
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    if (!marker.match(text).hasMatch()) return false;
    auto parsed = ai::lyrics::prepare(bytes, title, artist);
    if (parsed.lines.isEmpty() || parsed.lines.size() % 2) return false;
    if (parsed.timed) {
        static const QRegularExpression stamp(QStringLiteral("\\[\\d+:\\d{1,2}(?:[.:]\\d{1,3})?\\]"));
        static const QRegularExpression meta(QStringLiteral("^\\[(?:ar|al|ti|by|re|ve|length|offset):"),
                                             QRegularExpression::CaseInsensitiveOption);
        for (const auto &raw : text.split('\n')) {
            const auto line = raw.trimmed();
            if (!line.isEmpty() && !meta.match(line).hasMatch() && !stamp.match(line).hasMatch() &&
                !ai::lyrics::prepare(line.toUtf8(), title, artist).lines.isEmpty())
                return false;
        }
        std::stable_sort(parsed.lines.begin(), parsed.lines.end(),
                         [](const auto &a, const auto &b) { return a.timestampMs < b.timestampMs; });
    }
    ai::lyrics::PreparedLyrics prepared;
    QJsonArray translations;
    for (int i = 0; i < parsed.lines.size(); i += 2) {
        const auto &original = parsed.lines[i], &translation = parsed.lines[i + 1];
        if (original.timestampMs != translation.timestampMs || original.text.trimmed().isEmpty()) return false;
        ai::lyrics::SourceLine line;
        line.id = prepared.lines.size();
        line.text = original.text;
        prepared.lines.append(line);
        translations.append(QJsonObject{{"id", line.id}, {"text", translation.text}});
    }
    return ai::lyrics::validateResponse(
               QString::fromUtf8(QJsonDocument(QJsonObject{{"translations", translations}}).toJson(QJsonDocument::Compact)),
               prepared)
        .ok();
}

QString LyricsTranslationClassification::statusKey(TranslationStatus status) {
    switch (status) {
    case TranslationStatus::Instrumental: return QStringLiteral("instrumental");
    case TranslationStatus::NoLyrics: return QStringLiteral("no-lyrics");
    case TranslationStatus::Unreadable: return QStringLiteral("unreadable");
    case TranslationStatus::PendingSave: return QStringLiteral("pending-save");
    case TranslationStatus::InProgress: return QStringLiteral("in-progress");
    case TranslationStatus::NotNeeded: return QStringLiteral("not-needed");
    case TranslationStatus::Translated: return QStringLiteral("translated");
    case TranslationStatus::NeedsTranslation: return QStringLiteral("needs-translation");
    case TranslationStatus::Review: return QStringLiteral("review");
    case TranslationStatus::NativeChinese: return QStringLiteral("native-chinese");
    case TranslationStatus::NoSource: return QStringLiteral("no-source");
    }
    return QStringLiteral("unknown");
}

TranslationClassification LyricsTranslationClassification::decide(const TranslationClassificationInput &input) {
    using Need = ai::lyrics::TranslationNeed;
    const bool noNeeded = input.judgment == TranslationJudgment::NoTranslationNeeded;
    const bool needsJudged = input.judgment == TranslationJudgment::NeedsTranslation;
    const bool unsaved = input.taskRunning || input.taskUnsaved;

    // Lifecycle outranks everything: a paid preview or a running request must
    // never be reported as "already translated / not needed" while it is still
    // waiting, and it is always worth showing in the missing-translation list.
    if (input.taskUnsaved)
        return result(TranslationStatus::PendingSave, true, false, true,
                      QStringLiteral("译文已生成但尚未保存"), QStringLiteral("翻译未保存"));
    if (input.taskRunning)
        return result(TranslationStatus::InProgress, true, false, true,
                      QStringLiteral("整首翻译仍在进行中"), QStringLiteral("正在翻译"));

    // Pure music is excluded from translation entirely; the manual "无需翻译"
    // completes it just like any other row with no pending work.
    if (input.instrumental || input.content.instrumental)
        return result(TranslationStatus::Instrumental, false, noNeeded, false,
                      QStringLiteral("纯音乐，不参与翻译判断"), QString());

    // The manual "无需翻译" completes the row regardless of file state.
    if (noNeeded)
        return result(TranslationStatus::NotNeeded, false, true, false,
                      QStringLiteral("人工标记为无需翻译"), QStringLiteral("无需翻译"));

    // The manual "需要翻译" is an independent signal: it keeps the row in the
    // missing list even when the file is gone or unreadable, so a user can find
    // it again to fix the lyric. This matches the historical Widgets/QML rules.
    const bool translateJudged = needsJudged;
    if (!input.lyricsConfigured || !input.fileExists)
        return result(TranslationStatus::NoLyrics, translateJudged, false, false,
                      QStringLiteral("没有歌词，或本地歌词文件不存在"),
                      translateJudged ? QStringLiteral("需要翻译") : QString());
    if (!input.fileReadable)
        return result(TranslationStatus::Unreadable, translateJudged, false, false,
                      QStringLiteral("歌词文件存在但无法完整读取"),
                      translateJudged ? QStringLiteral("需要翻译") : QString());

    const bool translated = input.verifiedBilingual || input.content.need == Need::Translated;
    if (translated)
        return result(TranslationStatus::Translated, false, true, false,
                      input.verifiedBilingual ? QStringLiteral("已校验保存的完整双语对应行")
                                              : input.content.reason,
                      QStringLiteral("翻译完成"));
    if (input.content.need == Need::Missing)
        return result(TranslationStatus::NeedsTranslation, true, false, false, input.content.reason,
                      needsJudged ? QStringLiteral("需要翻译") : QStringLiteral("待判断"));
    if (input.content.need == Need::Review)
        return result(TranslationStatus::Review, true, false, false, input.content.reason,
                      needsJudged ? QStringLiteral("需要翻译") : QStringLiteral("待判断"));
    if (input.content.need == Need::Chinese)
        // Already Chinese: only an explicit manual "needs" keeps it in the
        // missing list. Widgets' badge used to say 需要翻译 here; keep that.
        return result(TranslationStatus::NativeChinese, needsJudged, false, false, input.content.reason,
                      needsJudged ? QStringLiteral("需要翻译") : QString());
    return result(TranslationStatus::NoSource, false, noNeeded, false,
                  input.content.reason.isEmpty() ? QStringLiteral("没有可翻译的歌词正文") : input.content.reason,
                  QString());
}

}
