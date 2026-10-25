#pragma once
#include "lyrics-replacement-policy.h"
#include "translation-coordinator.h"

namespace relaxer {
struct LyricsReplacementPlan {
    LyricsSaveSnapshot snapshot;
    QString candidateIdentity;
    QByteArray candidate, translationContext;
    QVector<quint64> translationTasks;
    LyricsReplacementDecision decision;
    bool consumed = false;
};
// Owner-thread command boundary shared by the two frontends. A plan is a local
// capability bound to one target, candidate revision, task state and file digest.
class LyricsReplacementService final {
public:
    static LyricsReplacementPlan prepare(Library &library, const LyricsSaveSnapshot &snapshot,
        const QByteArray &candidate, const QString &identity, TranslationCoordinator *translation = nullptr);
    static bool commit(Library &library, LyricsReplacementPlan &plan, LyricsReplacementChoice choice,
        qint64 currentTarget, const QString &currentIdentity, TranslationCoordinator *translation,
        QString *error = nullptr);
};
}
