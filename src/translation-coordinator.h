#pragma once
#include "ai/ai-runtime.h"
#include "ai/whole-song-lyrics-translation.h"
#include "library.h"
#include "translation-browser-backend.h"
#include <QQueue>
#include <memory>

namespace relaxer {
enum class TranslationMethod { DeepSeek, Browser };
enum class TranslationState { WaitingForConsent,Queued,Running,NeedsAttention,CompletedPendingSave,Saved,Cancelled,Failed,Invalidated };
// Local value snapshot. Contains lyrics, never credentials. Do not log its text.
struct TranslationSummary {
    quint64 taskId=0,generation=0;
    qint64 trackId=0;
    TranslationMethod method=TranslationMethod::DeepSeek;
    TranslationState state=TranslationState::WaitingForConsent;
    QString trackHash,lyricsPath,targetLanguage,providerId;
    QByteArray sourceDigest,configurationDigest;
    int completedLines=0,totalLines=0;
    QString status,details,errorCode;
    ai::lyrics::PreparedLyrics prepared;
    QStringList translations;
    bool terminal() const;
    bool pending() const;
    bool hasPreview() const {return state==TranslationState::CompletedPendingSave;}
};

// All calls and Library access stay on the owning event-loop thread. Network
// dispatch/lifetime is delegated exclusively to AiRequestCoordinator.
class TranslationCoordinator final : public QObject {
    Q_OBJECT
public:
    TranslationCoordinator(Library *library,ai::AiRuntime *runtime,QObject *parent=nullptr,
                           std::unique_ptr<TranslationBrowserBackend> browser={});
    ~TranslationCoordinator() override;
    // Preparation never dispatches. start() is the explicit consent boundary.
    quint64 prepareAi(qint64 trackId,QString *error=nullptr);
    quint64 prepareBrowser(qint64 trackId,const QString &provider,const QString &language,QString *error=nullptr);
    bool start(quint64 taskId,QString *error=nullptr);
    bool cancel(quint64 taskId); // Never discards an already completed preview.
    bool discard(quint64 taskId); // Explicit navigation/replacement/resend/close.
    bool resume(quint64 taskId);
    bool save(quint64 taskId,QString *error=nullptr);
    bool forget(quint64 taskId); // Only terminal tasks; bounded storage (32).
    TranslationSummary summary(quint64 taskId) const;
    void shutdown();
signals:
    void taskChanged(const relaxer::TranslationSummary &summary);
    // Exactly once per task; CompletedPendingSave is intentionally not terminal.
    void finished(const relaxer::TranslationSummary &summary);
private:
    struct Task {
        TranslationSummary value;
        Track track;
        LyricsSaveSnapshot saveSnapshot;
        QByteArray source,merged;
        ai::AiTaskConfiguration configuration;
        ai::ChatCompletionRequest request;
        QString sourceLanguage;
        quint64 requestId=0,segmentToken=0;
    };
    using TaskPtr=std::shared_ptr<Task>;
    QPointer<Library> m_library;
    QPointer<ai::AiRuntime> m_runtime;
    std::unique_ptr<TranslationBrowserBackend> m_browser;
    QHash<quint64,TaskPtr> m_tasks;
    QQueue<quint64> m_queue;
    quint64 m_active=0,m_generation=0;
    bool m_stopping=false,m_pumpScheduled=false;
    bool ownerThread() const;
    QByteArray configurationDigest() const;
    quint64 prepare(qint64 id,TranslationMethod method,const QString &provider,const QString &language,QString *error);
    bool current(const TaskPtr &task,QString *error) const;
    void publish(const TaskPtr &task);
    void finish(const TaskPtr &task,TranslationState state,const QString &status,const QString &details,const QString &code={});
    void schedule();void pump();void dispatchSegment(const TaskPtr &task);
    void completeAi(quint64 requestId,const ai::ChatCompletionResult &result);
    void completeBrowser(quint64 token,const QString &text,const QString &error);
    void ready(const TaskPtr &task,const QStringList &translations,const QString &usage={});
};
}
Q_DECLARE_METATYPE(relaxer::TranslationSummary)
