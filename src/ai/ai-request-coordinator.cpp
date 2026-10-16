#include "ai-request-coordinator.h"

#include <QTimer>

namespace relaxer::ai {

bool ProviderRegistry::add(AiProvider *provider) {
    if (!provider || provider->providerId().trimmed().isEmpty() || m_providers.contains(provider->providerId()))
        return false;
    m_providers.insert(provider->providerId(), provider);
    return true;
}

AiProvider *ProviderRegistry::provider(const QString &providerId) const {
    return m_providers.value(providerId, nullptr);
}

QStringList ProviderRegistry::providerIds() const {
    auto ids = m_providers.keys();
    ids.sort();
    return ids;
}

AiRequestCoordinator::AiRequestCoordinator(ProviderRegistry *providers, QObject *parent)
    : QObject(parent), m_providers(providers) {
    if (!m_providers)
        return;
    for (const auto &id : m_providers->providerIds())
        watch(m_providers->provider(id));
}

QString AiRequestCoordinator::providerRequestKey(const QString &providerId, quint64 requestId) const {
    return providerId + QLatin1Char('/') + QString::number(requestId);
}

void AiRequestCoordinator::watch(AiProvider *provider) {
    if (!provider)
        return;
    connect(provider, &AiProvider::modelsFinished, this,
            [this, provider](quint64 providerRequestId, const QList<ModelInfo> &models, const ProviderError &error) {
        const auto key = providerRequestKey(provider->providerId(), providerRequestId);
        const auto taskId = m_providerRequests.take(key);
        const auto pending = m_pending.take(taskId);
        if (!taskId || pending.kind != Kind::Models)
            return;
        emit modelsFinished(taskId, pending.providerId, models, error);
    });
    connect(provider, &AiProvider::completionFinished, this,
            [this, provider](quint64 providerRequestId, const ChatCompletionResult &result) {
        const auto key = providerRequestKey(provider->providerId(), providerRequestId);
        const auto taskId = m_providerRequests.take(key);
        const auto pending = m_pending.take(taskId);
        if (!taskId || pending.kind != Kind::Completion)
            return;
        emit completionFinished(taskId, pending.providerId, result);
    });
}

quint64 AiRequestCoordinator::fetchModels(const QString &providerId, const QString &apiKey) {
    const auto taskId = ++m_nextTaskId;
    auto *provider = m_providers ? m_providers->provider(providerId) : nullptr;
    if (!provider) {
        ProviderError error{"provider_unavailable", "所选 AI 供应商不可用"};
        QTimer::singleShot(0, this, [this, taskId, providerId, error] { emit modelsFinished(taskId, providerId, {}, error); });
        return taskId;
    }
    const auto requestId = provider->listModels(apiKey);
    m_pending.insert(taskId, Pending{providerId, requestId, Kind::Models});
    m_providerRequests.insert(providerRequestKey(providerId, requestId), taskId);
    return taskId;
}

quint64 AiRequestCoordinator::complete(const QString &providerId, const QString &apiKey,
                                       const ChatCompletionRequest &request) {
    const auto taskId = ++m_nextTaskId;
    auto *provider = m_providers ? m_providers->provider(providerId) : nullptr;
    if (!provider) {
        ProviderError error{"provider_unavailable", "所选 AI 供应商不可用"};
        QTimer::singleShot(0, this, [this, taskId, providerId, error] {
            emit completionFinished(taskId, providerId, ChatCompletionResult{{}, {}, {}, error});
        });
        return taskId;
    }
    const auto requestId = provider->complete(apiKey, request);
    m_pending.insert(taskId, Pending{providerId, requestId, Kind::Completion});
    m_providerRequests.insert(providerRequestKey(providerId, requestId), taskId);
    return taskId;
}

bool AiRequestCoordinator::cancel(quint64 taskId) {
    const auto found = m_pending.constFind(taskId);
    if (found == m_pending.cend())
        return false;
    auto *provider = m_providers ? m_providers->provider(found->providerId) : nullptr;
    return provider && provider->cancel(found->providerRequestId);
}

bool AiRequestCoordinator::isPending(quint64 taskId) const { return m_pending.contains(taskId); }
} // namespace relaxer::ai
