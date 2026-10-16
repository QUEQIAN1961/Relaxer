#include "ai-request-coordinator.h"

namespace relaxer::ai {

bool ProviderRegistry::add(AiProvider *provider) {
    if (!provider || provider->providerId().trimmed().isEmpty() || this->provider(provider->providerId()))
        return false;
    m_providers.insert(provider->providerId(), provider);
    return true;
}

AiProvider *ProviderRegistry::provider(const QString &providerId) const {
    return m_providers.value(providerId).data();
}

QStringList ProviderRegistry::providerIds() const {
    QStringList ids;
    for (auto it = m_providers.cbegin(); it != m_providers.cend(); ++it)
        if (it.value()) ids.append(it.key());
    ids.sort();
    return ids;
}

AiRequestCoordinator::AiRequestCoordinator(ProviderRegistry *providers, CredentialStore *credentials,
                                         const ModelRegistry *models, QObject *parent)
    : QObject(parent), m_providers(providers), m_credentials(credentials), m_models(models) {}

AiRequestCoordinator::~AiRequestCoordinator() {
    while (!m_pending.isEmpty())
        cancel(m_pending.constBegin().key());
}

QString AiRequestCoordinator::providerRequestKey(quint64 providerGeneration, quint64 requestId) const {
    return QString::number(providerGeneration) + QLatin1Char('/') + QString::number(requestId);
}

void AiRequestCoordinator::watch(AiProvider *provider) {
    if (!provider || m_watched.contains(provider)) return;
    const auto generation = ++m_nextProviderGeneration;
    m_watched.insert(provider, generation);
    connect(provider, &AiProvider::modelsFinished, this,
            [this, generation](quint64 requestId, const QList<ModelInfo> &models, const ProviderError &error) {
        const auto taskId = m_providerRequests.value(providerRequestKey(generation, requestId));
        Pending pending;
        if (!taskId || m_pending.value(taskId).kind != Kind::Models || !takePending(taskId, &pending)) return;
        emit modelsFinished(taskId, pending.providerId, models, error);
    }, Qt::QueuedConnection);
    connect(provider, &AiProvider::completionFinished, this,
            [this, generation](quint64 requestId, const ChatCompletionResult &result) {
        const auto taskId = m_providerRequests.value(providerRequestKey(generation, requestId));
        Pending pending;
        if (!taskId || m_pending.value(taskId).kind != Kind::Completion || !takePending(taskId, &pending)) return;
        emit completionFinished(taskId, pending.providerId, result);
    }, Qt::QueuedConnection);
    connect(provider, &QObject::destroyed, this, [this, provider, generation] {
        m_watched.remove(provider);
        const auto ids = m_pending.keys();
        for (auto id : ids)
            if (m_pending.value(id).providerGeneration == generation)
                finishError(id, {"provider_unavailable", QStringLiteral("AI 供应商已关闭")});
    });
}

quint64 AiRequestCoordinator::begin(const QString &providerId, Kind kind) {
    const auto taskId = ++m_nextTaskId;
    auto *provider = m_providers ? m_providers->provider(providerId) : nullptr;
    watch(provider);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    m_pending.insert(taskId, Pending{providerId, provider, m_watched.value(provider), 0, kind, timer});
    connect(timer, &QTimer::timeout, this, [this, taskId] {
        const auto pending = m_pending.value(taskId);
        finishError(taskId, {"timeout", QStringLiteral("AI 请求超时，请手动重试")});
        if (pending.provider && pending.providerRequestId)
            pending.provider->cancel(pending.providerRequestId);
    });
    timer->start(m_timeoutMs);
    return taskId;
}

bool AiRequestCoordinator::readKey(quint64 taskId, QString *key) {
    const auto pending = m_pending.value(taskId);
    if (!pending.provider) {
        reject(taskId, {"provider_unavailable", QStringLiteral("所选 AI 供应商不可用")});
        return false;
    }
    // This check precedes read(): a mock endpoint cannot even read a production store.
    if (!m_credentials || (pending.provider->usesMockEndpoint() && !m_credentials->allowsMockRequests())) {
        reject(taskId, {"credentials_unavailable", QStringLiteral("该请求不能使用当前凭据存储")});
        return false;
    }
    auto result = m_credentials->read(pending.providerId);
    QString error;
    if (result.ok && result.found)
        *key = normalizedApiKey(result.secret, &error);
    result.secret.fill(QChar());
    if (!result.ok || !result.found || key->isEmpty()) {
        reject(taskId, {"invalid_credential", QStringLiteral("请先安全保存有效的供应商 API Key")});
        return false;
    }
    return true;
}

void AiRequestCoordinator::dispatch(quint64 taskId, const QString &key, const ChatCompletionRequest *request) {
    const auto pending = m_pending.value(taskId);
    if (!m_pending.contains(taskId)) return;
    if (!pending.provider) {
        reject(taskId, {"provider_unavailable", QStringLiteral("AI 供应商已关闭")});
        return;
    }
    const auto requestId = request ? pending.provider->complete(key, *request) : pending.provider->listModels(key);
    if (!requestId) {
        reject(taskId, {"invalid_request", QStringLiteral("AI 供应商未能启动请求")});
        return;
    }
    m_pending[taskId].providerRequestId = requestId;
    m_providerRequests.insert(providerRequestKey(pending.providerGeneration, requestId), taskId);
}

quint64 AiRequestCoordinator::fetchModels(const QString &providerId) {
    const auto previous = m_latestModelTask.value(providerId);
    if (previous) cancel(previous);
    const auto taskId = begin(providerId, Kind::Models);
    m_latestModelTask.insert(providerId, taskId);
    QString key;
    if (readKey(taskId, &key)) dispatch(taskId, key);
    key.fill(QChar());
    return taskId;
}

quint64 AiRequestCoordinator::complete(const QString &providerId, const ChatCompletionRequest &request) {
    const auto taskId = begin(providerId, Kind::Completion);
    const auto *model = m_models ? m_models->find(providerId, request.modelId) : nullptr;
    const auto error = validateModelSelection(model, providerId, request.modelId,
                                               request.thinkingEnabled, request.reasoningEffort);
    if (!error.isEmpty() || request.messages.isEmpty() || request.maxTokens <= 0 ||
        (model && request.maxTokens > model->capabilities.maxOutputTokens)) {
        reject(taskId, {"invalid_configuration", error.isEmpty() ? QStringLiteral("AI 请求消息或输出上限无效") : error});
        return taskId;
    }
    QString key;
    if (readKey(taskId, &key)) dispatch(taskId, key, &request);
    key.fill(QChar());
    return taskId;
}

bool AiRequestCoordinator::takePending(quint64 taskId, Pending *pending) {
    auto it = m_pending.find(taskId);
    if (it == m_pending.end()) return false;
    *pending = it.value();
    m_pending.erase(it);
    m_providerRequests.remove(providerRequestKey(pending->providerGeneration, pending->providerRequestId));
    if (pending->timer) {
        pending->timer->stop();
        pending->timer->deleteLater();
    }
    return true;
}

void AiRequestCoordinator::reject(quint64 taskId, const ProviderError &error) {
    QTimer::singleShot(0, this, [this, taskId, error] { finishError(taskId, error); });
}

void AiRequestCoordinator::finishError(quint64 taskId, const ProviderError &error) {
    Pending pending;
    if (!takePending(taskId, &pending)) return;
    if (pending.kind == Kind::Models)
        emit modelsFinished(taskId, pending.providerId, {}, error);
    else
        emit completionFinished(taskId, pending.providerId, ChatCompletionResult{{}, {}, {}, error});
}

bool AiRequestCoordinator::cancel(quint64 taskId) {
    Pending pending;
    if (!takePending(taskId, &pending)) return false;
    if (m_latestModelTask.value(pending.providerId) == taskId)
        m_latestModelTask.remove(pending.providerId);
    if (pending.provider && pending.providerRequestId)
        pending.provider->cancel(pending.providerRequestId);
    const ProviderError error{"cancelled", QStringLiteral("本地请求已取消"), 0, true};
    QTimer::singleShot(0, this, [this, taskId, pending, error] {
        if (pending.kind == Kind::Models)
            emit modelsFinished(taskId, pending.providerId, {}, error);
        else
            emit completionFinished(taskId, pending.providerId, ChatCompletionResult{{}, {}, {}, error});
    });
    return true;
}

bool AiRequestCoordinator::isPending(quint64 taskId) const { return m_pending.contains(taskId); }
quint64 AiRequestCoordinator::latestModelTask(const QString &providerId) const {
    return m_latestModelTask.value(providerId);
}
void AiRequestCoordinator::setTimeoutMs(int timeoutMs) { m_timeoutMs = qBound(10, timeoutMs, 300000); }

} // namespace relaxer::ai
