#pragma once
// Shared fixture table for D2-06: one synthetic lyric state per row, with the
// business answer both frontends must reproduce. Widgets and QML tests consume
// the same rows, so a divergence fails one of them instead of going unnoticed.
#include "library.h"
#include <QByteArray>
#include <QFile>
#include <QVector>

namespace translation_cases {
using relaxer::Library;
using relaxer::TranslationJudgment;

struct Case {
    const char *id;
    const char *name;
    QByteArray body;
    bool configured = true;  // track.lyrics points at a file
    bool hasFile = true;     // the file is still on disk
    bool instrumental = false;  // marked through Library
    TranslationJudgment judgment = TranslationJudgment::Unknown;
    bool missing = false;     // belongs to 缺失翻译
    bool completed = false;   // green / 翻译完成
    const char *status = "";
    const char *badge = "";
};

inline QString title() { return QStringLiteral("Case Song"); }
inline QString artist() { return QStringLiteral("Case Artist"); }

inline QVector<Case> all() {
    const QByteArray mono =
        "[00:01.00]Hello my friend\n[00:02.00]This is a synthetic song\n[00:03.00]We walk together tonight\n";
    const QByteArray markedPairs = "[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好\n"
                                   "[00:02.00]Goodbye friend\n[00:02.00]朋友再见\n";
    const QByteArray legacyPairs = "[00:01.00]Hello friend\n[00:01.00]朋友你好\n"
                                   "[00:02.00]Goodbye friend\n[00:02.00]朋友再见\n";
    const QByteArray aiPairs = "[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好\n"
                               "[00:02.00]Goodbye friend\n[00:02.00]朋友再见\n"
                               "[00:03.00]We walk together\n[00:03.00]我们一起走\n";
    const QByteArray partial = "[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]朋友你好\n"
                               "[00:02.00]Goodbye friend\n";
    const QByteArray emptyTranslation = "[re:relaxer-bilingual]\n[00:01.00]Hello friend\n[00:01.00]\n";
    const QByteArray duplicateStamps = "[00:01.00]Hello friend\n[00:01.00]你好朋友\n"
                                       "[00:01.00]Goodbye friend\n[00:01.00]再见朋友\n";
    const QByteArray multiStamp = "[00:01.00][00:05.00]Hello my friend\n";
    const QByteArray instrumentalBody = "[00:00.00]纯音乐，请欣赏\n";
    const QByteArray untimed = "Hello my friend\nThis is a synthetic song\nWe walk together tonight\n";
    const QByteArray metadataOnly = "[ti:Case Song]\n[ar:Case Artist]\n[al:Case Album]\n[by:Case Writer]\n";
    const QByteArray unmarkedSinglePair = "[00:00.00]作词：测试作者\n[00:01.00]Hello friend\n[00:01.00]朋友你好\n";
    const QByteArray chinese = "[00:01.00]今天的风很温柔\n[00:03.00]我们一起走回家\n[00:05.00]夜色落在窗前\n";

    return {
        // T01 没有歌词：既不是"缺少译文"，也不是"已完成"
        {"T01", "no lyric file", {}, false, false, false, TranslationJudgment::Unknown, false, false,
         "no-lyrics", ""},
        {"T02", "mono foreign body", mono, true, true, false, TranslationJudgment::Unknown, true, false,
         "needs-translation", "待判断"},
        {"T02-needs", "mono + manual needs", mono, true, true, false, TranslationJudgment::NeedsTranslation, true,
         false, "needs-translation", "需要翻译"},
        {"T02-no", "mono + manual not-needed", mono, true, true, false,
         TranslationJudgment::NoTranslationNeeded, false, true, "not-needed", "无需翻译"},
        // T03 标准双语（带标记）
        {"T03", "marked bilingual", markedPairs, true, true, false, TranslationJudgment::Unknown, false, true,
         "translated", "翻译完成"},
        {"T03-needs", "marked bilingual + stale manual needs", markedPairs, true, true, false,
         TranslationJudgment::NeedsTranslation, false, true, "translated", "翻译完成"},
        // T04 旧式人工双语（无标记，完整配对）
        {"T04", "legacy unmarked bilingual", legacyPairs, true, true, false, TranslationJudgment::Unknown, false,
         true, "translated", "翻译完成"},
        {"T05", "ai bilingual three pairs", aiPairs, true, true, false, TranslationJudgment::Unknown, false, true,
         "translated", "翻译完成"},
        // T06 部分译文
        {"T06", "partial translation", partial, true, true, false, TranslationJudgment::Unknown, true, false,
         "review", "待判断"},
        {"T07", "empty translation row", emptyTranslation, true, true, false, TranslationJudgment::Unknown, true,
         false, "review", "待判断"},
        {"T08", "duplicate timestamps paired", duplicateStamps, true, true, false, TranslationJudgment::Unknown,
         false, true, "translated", "翻译完成"},
        {"T09", "multi timestamp row", multiStamp, true, true, false, TranslationJudgment::Unknown, true, false,
         "needs-translation", "待判断"},
        // T10 空/不可解析文件：安全降级，不当作缺少译文
        {"T10", "empty file", {}, true, true, false, TranslationJudgment::Unknown, false, false, "no-source", ""},
        {"T11", "instrumental body", instrumentalBody, true, true, false, TranslationJudgment::Unknown, false,
         false, "instrumental", ""},
        {"T11-flag", "instrumental flagged", mono, true, true, true, TranslationJudgment::Unknown, false, false,
         "instrumental", ""},
        {"T12", "untimed foreign body", untimed, true, true, false, TranslationJudgment::Unknown, true, false,
         "needs-translation", "待判断"},
        {"T13", "metadata only", metadataOnly, true, true, false, TranslationJudgment::Unknown, false, false,
         "no-source", ""},
        // T14 无双语标记的单对译文：保守判为待确认，不自动当成完整译文
        {"T14", "unmarked single pair", unmarkedSinglePair, true, true, false, TranslationJudgment::Unknown, true,
         false, "review", "待判断"},
        {"T15", "file disappears", mono, true, false, false, TranslationJudgment::Unknown, false, false,
         "no-lyrics", ""},
        {"T15-needs", "file disappears + manual needs", mono, true, false, false,
         TranslationJudgment::NeedsTranslation, true, false, "no-lyrics", "需要翻译"},
        {"T16", "chinese body", chinese, true, true, false, TranslationJudgment::Unknown, false, false,
         "native-chinese", ""},
        {"T16-needs", "chinese body + manual needs", chinese, true, true, false,
         TranslationJudgment::NeedsTranslation, true, false, "native-chinese", "需要翻译"},
    };
}

// Writes one case into an already seeded track. Returns false when the fixture
// itself could not be stored.
inline bool apply(Library &lib, qint64 id, const Case &c) {
    if (!c.configured) return true;
    if (c.instrumental) return lib.markInstrumental(id);
    if (!lib.saveLyrics(id, c.body)) return false;
    if (!c.hasFile) QFile::remove(lib.absolute(lib.track(id).lyrics));
    return true;
}
}
