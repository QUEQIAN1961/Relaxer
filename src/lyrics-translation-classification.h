#pragma once
#include "ai/whole-song-lyrics-translation.h"
#include "library.h"
#include <QByteArray>
#include <QString>

namespace relaxer {

// One business answer for "does this track still need a translation?".
// Both frontends consume this enum; neither may invent its own combination of
// the manual judgment, the file read result, the content assessment and the
// translation task snapshot.
enum class TranslationStatus {
    Instrumental,      // library flag or LRC body says 纯音乐
    NoLyrics,          // no lyric path configured, or the file is gone
    Unreadable,        // file exists but cannot be read completely
    PendingSave,       //译文已生成但尚未写入
    InProgress,        // translation task still running
    NotNeeded,         // user marked 无需翻译
    Translated,        // complete saved translation (incl. verified marked pairs)
    NeedsTranslation,  // foreign body without a Chinese counterpart
    Review,            // cannot be decided locally, needs a human look
    NativeChinese,     // body is already Chinese without foreign lines
    NoSource           // readable file without a translatable body
};

// Local facts only. Callers gather them; this layer touches no file, database,
// task, or widget.
struct TranslationClassificationInput {
    TranslationJudgment judgment = TranslationJudgment::Unknown;
    bool instrumental = false;                  // Library::isInstrumental
    bool lyricsConfigured = false;              // track.lyrics non-empty
    bool fileExists = false;
    bool fileReadable = false;                  // opened && complete && !tooLarge
    ai::lyrics::TranslationAssessment content;  // assessTranslation result
    bool verifiedBilingual = false;             // marker + complete pair validation
    bool taskRunning = false;
    bool taskUnsaved = false;
};

struct TranslationClassification {
    TranslationStatus status = TranslationStatus::NoLyrics;
    bool needsTranslation = false;  // belongs to the "缺失翻译" filter
    bool completed = false;         // safe to treat as done (green / 翻译完成)
    bool unsavedResult = false;     // holds a running or unsaved translation
    QString reason;
    QString badge;
};

class LyricsTranslationClassification final {
public:
    static TranslationClassification decide(const TranslationClassificationInput &input);
    // The merge marker alone is not proof: every source line must keep a
    // matching saved partner. Pure function over bytes.
    static bool verifiedMarkedBilingual(const QByteArray &bytes, const QString &title = {},
                                        const QString &artist = {});
    // Stable identifier for frontend rows and logs; never localised here.
    static QString statusKey(TranslationStatus status);
};

}
