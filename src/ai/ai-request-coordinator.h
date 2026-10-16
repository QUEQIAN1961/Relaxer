#pragma once

#include "ai-provider.h"
#include "credential-store.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>

namespace relaxer::ai {

class ProviderRegistry final {
  public:
    bool add(AiProvider *provider);
    AiProvider *provider(const QString &providerId) const;
    QStringList providerIds() const;
  private:
    QHash<QString, QPointer<AiProvider>> m_providers;
};

// Owns task identity and the only credential/model-checked network dispatch boundary.
// Provider implementations report asynchronously; queued connections also protect
// against a test/provider emitting synchronously from its dispatch function.
class AiRequestCoordinator final : public QObject {
    Q_OBJECT
  public:
    explicit AiRequestCoordinator(ProviderRegistry *providers, CredentialStore *credentials,
                                  const ModelRegistry *models = nullptr, QObject *parent = nullptr);
    ~AiRequestCoordinator() override;
    quint64 fetchModels(const QString &providerId);
    quint64 complete(const QString &providerId, const ChatCompletionRequest &request);
    // Returns whether a local task was terminated, not whether the remote request was revoked.
    bool cancel(quint64 taskId);
    bool isPending(quint64 taskId) const;
    quint64 latestModelTask(const QString &providerId) const;
    void setTimeoutMs(int timeoutMs);

  signals:
    void modelsFinished(quint64 taskId, const QString &providerId, const QList<relaxer::ai::ModelInfo> &models,
                        const relaxer::ai::ProviderError &error);
    void completionFinished(quint64 taskId, const QString &providerId,
                            const relaxer::ai::ChatCompletionResult &result);
  private:
    enum class Kind { Models, Completion };
    struct Pending {
        QString providerId;
        QPointer<AiProvider> provider;
        quint64 providerGeneration = 0;
        quint64 providerRequestId = 0;
        Kind kind = Kind::Models;
        QPointer<QTimer> timer;
        QElapsedTimer elapsed;
        qint64 startedAtMs = 0;
        TimeoutBudget budget{0, 0};
        QString modelId, reasoningEffort;
        bool thinkingEnabled = false;
    };
    ProviderRegistry *m_providers;
    CredentialStore *m_credentials;
    const ModelRegistry *m_models;
    QHash<quint64, Pending> m_pending;
    QHash<QString, quint64> m_providerRequests;
    QHash<QString, quint64> m_latestModelTask;
    QHash<AiProvider *, quint64> m_watched;
    quint64 m_nextProviderGeneration = 0;
    quint64 m_nextTaskId = 0;
    int m_timeoutMs = 0; // Explicit watchdog override for fault-injection tests only.
    QString providerRequestKey(quint64 providerGeneration, quint64 requestId) const;
    quint64 begin(const QString &providerId, Kind kind);
    bool readKey(quint64 taskId, QString *key);
    bool takePending(quint64 taskId, Pending *pending);
    void reject(quint64 taskId, const ProviderError &error);
    void finishError(quint64 taskId, const ProviderError &error);
    void dispatch(quint64 taskId, const QString &key, const ChatCompletionRequest *request = nullptr);
    void watch(AiProvider *provider);
    void finishCompletion(quint64 taskId, const Pending &pending, ChatCompletionResult result);
};

} // namespace relaxer::ai
