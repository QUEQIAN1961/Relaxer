#pragma once

#include "ai-model.h"

#include <QJsonArray>
#include <QObject>
#include <QUrl>

namespace relaxer::ai {

enum class RequestOperation { Models, Completion };
struct TimeoutBudget {
    int providerMs;
    int coordinatorMs;
};
// One policy for both layers; reserve room under the 300 s watchdog ceiling.
inline TimeoutBudget timeoutBudgetFor(RequestOperation operation, int requestedMs = 0) {
    const bool completion = operation == RequestOperation::Completion;
    const int grace = completion ? 5000 : 1000;
    const int duration = qBound(1000, requestedMs > 0 ? requestedMs : (completion ? 180000 : 30000), 300000 - grace);
    return {duration, duration + grace};
}

struct ProviderError {
    QString code;
    QString message;
    int httpStatus = 0;
    bool cancelled = false;
    bool isEmpty() const { return code.isEmpty(); }
};

struct TokenUsage {
    qint64 inputTokens = 0;
    qint64 outputTokens = 0;
    qint64 totalTokens = 0;
    bool available = false;
};

struct ChatCompletionRequest {
    QString modelId;
    QJsonArray messages;
    bool thinkingEnabled = false;
    QString reasoningEffort;
    qint64 maxTokens = 0;
};

// Metadata only: never store request/response text, credentials or reasoning here.
struct RequestDiagnostics {
    qint64 elapsedMs = 0;
    qint64 responseBytes = 0;
    int httpStatus = 0;
    int networkError = 0;
    int providerTimeoutMs = 0;
    int coordinatorTimeoutMs = 0;
    QString timeoutSource;
};

struct ChatCompletionResult {
    QString content;
    QString finishReason;
    TokenUsage usage;
    ProviderError error;
    RequestDiagnostics diagnostics{};
    bool ok() const { return error.isEmpty(); }
};

QJsonObject deepSeekChatPayload(const ChatCompletionRequest &request, QString *error = nullptr);

class AiProvider : public QObject {
    Q_OBJECT
  public:
    explicit AiProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~AiProvider() override = default;
    virtual QString providerId() const = 0;
    virtual bool usesMockEndpoint() const { return false; }
    virtual TimeoutBudget timeoutBudget(RequestOperation operation) const { return timeoutBudgetFor(operation); }
  protected:
    friend class AiRequestCoordinator;
    // Only the coordinator dispatches requests after credentials/capabilities are checked.
    virtual quint64 listModels(const QString &apiKey) = 0;
    virtual quint64 complete(const QString &apiKey, const ChatCompletionRequest &request) = 0;
    virtual bool cancel(quint64 requestId) = 0;

  signals:
    void modelsFinished(quint64 requestId, const QList<relaxer::ai::ModelInfo> &models,
                        const relaxer::ai::ProviderError &error);
    void completionFinished(quint64 requestId, const relaxer::ai::ChatCompletionResult &result);
};

} // namespace relaxer::ai

Q_DECLARE_METATYPE(relaxer::ai::ProviderError)
Q_DECLARE_METATYPE(relaxer::ai::ChatCompletionResult)
Q_DECLARE_METATYPE(QList<relaxer::ai::ModelInfo>)
