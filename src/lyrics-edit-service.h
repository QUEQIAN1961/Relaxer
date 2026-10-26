#pragma once
#include "lyrics-replacement-service.h"
#include <QVariantMap>

namespace relaxer {
class LyricsEditService;
// A restricted capability: callers can inspect a preview, never supply its bytes.
class LyricsOffsetPlan final {
public:
    bool valid() const { return m_guard.decision.valid; }
    QString error() const { return m_error; }
    QByteArray content() const { return m_guard.decision.content; }
    int existingOffset() const { return m_existing; }
    qint64 trackId() const { return m_guard.snapshot.id; }
    bool confirmationRequired() const { return m_changed && m_guard.decision.confirmationRequired; }
    bool changed() const { return m_changed; }
private:
    friend class LyricsEditService;
    LyricsReplacementPlan m_guard;
    QString m_error;
    int m_existing = 0;
    bool m_changed = false, m_consumed = false;
};

class LyricsInstrumentalBatch final {
public:
    bool valid() const { return m_error.isEmpty() && !m_entries.isEmpty(); }
    bool finished() const { return m_next >= m_entries.size(); }
    QVariantMap overview() const;
    QVariantMap result() const;
    QString error() const { return m_error; }
private:
    friend class LyricsEditService;
    struct Entry { qint64 id = 0; QString title, artist, state, reason; LyricsReplacementPlan plan; };
    QVector<Entry> m_entries;
    QString m_error;
    QStringList m_details;
    int m_next = 0, m_succeeded = 0, m_skipped = 0, m_failed = 0, m_cancelledCount = 0, m_omitted = 0;
    bool m_cancelled = false;
    void detail(const Entry &entry);
};

// Owner-thread, UI-independent commands; all writes use Library's checked save.
class LyricsEditService final {
public:
    static constexpr int MaximumBatch = 1000;
    static constexpr int MaximumOffset = 5000; // Audited Widgets file-edit contract.
    static QByteArray withOffset(const QByteArray &data, int milliseconds);
    static LyricsOffsetPlan prepareOffset(Library &library, const LyricsSaveSnapshot &snapshot,
                                          int milliseconds, TranslationCoordinator *translation = nullptr);
    static bool commitOffset(Library &library, LyricsOffsetPlan &plan, qint64 currentTarget,
                             bool discardTranslation, TranslationCoordinator *translation, QString *error = nullptr);
    static LyricsInstrumentalBatch prepareInstrumental(Library &library, const QVector<qint64> &ids,
        TranslationCoordinator *translation = nullptr, const QHash<qint64,LyricsSaveSnapshot> &snapshots = {});
    // One atomic-per-track step. Cancel is observed between steps, never mid-write.
    static bool stepInstrumental(Library &library, LyricsInstrumentalBatch &batch, bool replaceRisks,
                                 TranslationCoordinator *translation = nullptr);
    static void cancelInstrumental(LyricsInstrumentalBatch &batch);
};
}
