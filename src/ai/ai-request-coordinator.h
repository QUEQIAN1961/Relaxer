#pragma once

#include "ai-provider.h"

#include <QObject>
#include <QHash>

namespace relaxer::ai {

class ProviderRegistry final {
  public:
    bool add(AiProvider *provider);
    AiProvider *provider(const QString &providerId) const;
    QStringList providerIds() const;
  private:
    QHash<QString, AiProvider *> m_providers;
};

class AiRequestCoordinator final : public QObject {
    Q_OBJECT
  public:
    explicit AiRequestCoordinator(ProviderRegistry *providers, QObject *parent = nullptr);
    quint64 fetchModels(const QString &providerId, const QString &apiKey);
    quint64 complete(const QString &providerId, const QString &apiKey, const ChatCompletionRequest &request);
    bool cancel(quint64 taskId);
    bool isPending(quint64 taskId) const;
  signals:
    void modelsFinished(quint64 taskId, const QString &providerId,
                        const QList<relaxer::ai::ModelInfo> &models,
                        const relaxer::ai::ProviderError &error);
    void completionFinished(quint64 taskId, const QString &providerId,
                            const relaxer::ai::ChatCompletionResult &result);
  private:
    enum class Kind { Models, Completion };
    struct Pending { QString providerId; quint64 providerRequestId = 0; Kind kind = Kind::Models; };
    ProviderRegistry *m_providers;
    QHash<quint64, Pending> m_pending;
    QHash<QString, quint64> m_providerRequests;
    quint64 m_nextTaskId = 0;
    QString providerRequestKey(const QString &providerId, quint64 requestId) const;
    void watch(AiProvider *provider);
};

} // namespace relaxer::ai
