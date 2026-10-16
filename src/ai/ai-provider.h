#pragma once

#include "ai-model.h"

#include <QJsonArray>
#include <QObject>
#include <QUrl>

namespace relaxer::ai {

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

struct ChatCompletionResult {
    QString content;
    QString finishReason;
    TokenUsage usage;
    ProviderError error;
    bool ok() const { return error.isEmpty(); }
};

QJsonObject deepSeekChatPayload(const ChatCompletionRequest &request, QString *error = nullptr);

class AiProvider : public QObject {
    Q_OBJECT
  public:
    explicit AiProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~AiProvider() override = default;
    virtual QString providerId() const = 0;
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
