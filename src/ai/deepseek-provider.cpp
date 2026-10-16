#include "deepseek-provider.h"

#include "ai-model.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QNetworkProxy>

namespace relaxer::ai {
namespace {
ProviderError httpError(int status, bool networkError) {
    ProviderError error;
    error.httpStatus = status;
    if (status == 401) {
        error.code = "unauthorized";
        error.message = "API Key 无效或已失效";
    } else if (status == 403) {
        error.code = "forbidden";
        error.message = "供应商拒绝了该请求";
    } else if (status == 402) {
        error.code = "quota";
        error.message = "账户余额或额度不足";
    } else if (status == 429) {
        error.code = "rate_limited";
        error.message = "请求过于频繁，请稍后手动重试";
    } else if (status >= 500) {
        error.code = "server";
        error.message = "DeepSeek 服务暂时不可用";
    } else if (status >= 400) {
        error.code = "http";
        error.message = QString("DeepSeek 请求失败（HTTP %1）").arg(status);
    } else if (networkError) {
        error.code = "network";
        error.message = "网络连接失败或请求超时";
    }
    return error;
}

ProviderError invalidResponse(const QString &message) {
    ProviderError error;
    error.code = "invalid_response";
    error.message = message;
    return error;
}
} // namespace

QJsonObject deepSeekChatPayload(const ChatCompletionRequest &request, QString *error) {
    if (request.modelId.trimmed().isEmpty() || request.messages.isEmpty()) {
        if (error)
            *error = "请求缺少模型或消息";
        return {};
    }
    if (request.maxTokens <= 0) {
        if (error)
            *error = "请求输出上限无效";
        return {};
    }
    QJsonObject body{{"model", request.modelId},
                     {"messages", request.messages},
                     {"stream", false},
                     {"max_tokens", double(request.maxTokens)},
                     {"response_format", QJsonObject{{"type", "json_object"}}}};
    if (request.thinkingEnabled) {
        const auto effort = request.reasoningEffort.trimmed().toLower();
        if (effort != "low" && effort != "high" && effort != "max") {
            if (error)
                *error = "所选模型不支持当前思考强度";
            return {};
        }
        body["thinking"] = QJsonObject{{"type", "enabled"}};
        body["reasoning_effort"] = effort;
    } else {
        body["thinking"] = QJsonObject{{"type", "disabled"}};
    }
    if (error)
        error->clear();
    return body;
}

DeepSeekProvider::DeepSeekProvider(QUrl baseUrl, QObject *parent)
    : AiProvider(parent), m_network(this), m_baseUrl(std::move(baseUrl)) {
    while (m_baseUrl.path().endsWith('/') && m_baseUrl.path().size() > 1)
        m_baseUrl.setPath(m_baseUrl.path().left(m_baseUrl.path().size() - 1));
    if (m_baseUrl.path() == "/")
        m_baseUrl.setPath({});
    if (m_baseUrl.host() == "127.0.0.1" || m_baseUrl.host() == "localhost")
        m_network.setProxy(QNetworkProxy::NoProxy);
}

QString DeepSeekProvider::providerId() const { return QString::fromLatin1(DeepSeekProviderId); }

QUrl DeepSeekProvider::endpoint(const QString &path) const {
    auto url = m_baseUrl;
    auto suffix = path;
    if (!suffix.startsWith('/'))
        suffix.prepend('/');
    url.setPath(m_baseUrl.path() + suffix);
    url.setQuery({});
    url.setFragment({});
    return url;
}

void DeepSeekProvider::setTimeoutMs(int timeoutMs) { m_timeoutMs = qBound(1000, timeoutMs, 300000); }

quint64 DeepSeekProvider::listModels(const QString &apiKey) {
    QNetworkRequest request(endpoint("/models"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(0);
    return send(Kind::Models, request);
}

quint64 DeepSeekProvider::complete(const QString &apiKey, const ChatCompletionRequest &chat) {
    QString payloadError;
    const auto payload = deepSeekChatPayload(chat, &payloadError);
    if (payload.isEmpty()) {
        const quint64 id = ++m_nextRequestId;
        ProviderError error;
        error.code = "invalid_request";
        error.message = payloadError;
        QTimer::singleShot(0, this, [this, id, error] {
            emit completionFinished(id, ChatCompletionResult{{}, {}, {}, error});
        });
        return id;
    }
    QNetworkRequest request(endpoint("/chat/completions"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(0);
    return send(Kind::Completion, request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

quint64 DeepSeekProvider::send(Kind kind, QNetworkRequest request, const QByteArray &body) {
    const quint64 id = ++m_nextRequestId;
    auto *reply = body.isEmpty() ? m_network.get(request) : m_network.post(request, body);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    m_pending.insert(id, Pending{kind, reply, timer});
    connect(timer, &QTimer::timeout, reply, [reply] {
        if (reply && !reply->isFinished())
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, id] { finish(id); });
    timer->start(m_timeoutMs);
    return id;
}

bool DeepSeekProvider::cancel(quint64 requestId) {
    const auto found = m_pending.find(requestId);
    if (found == m_pending.end() || !found->reply)
        return false;
    found->reply->setProperty("relaxerCancelled", true);
    found->reply->abort();
    return true;
}

void DeepSeekProvider::finish(quint64 requestId) {
    auto it = m_pending.find(requestId);
    if (it == m_pending.end())
        return;
    const auto pending = it.value();
    m_pending.erase(it);
    if (pending.timer) {
        pending.timer->stop();
        pending.timer->deleteLater();
    }
    if (!pending.reply)
        return;
    auto *reply = pending.reply.data();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool cancelled = reply->property("relaxerCancelled").toBool();
    const bool networkError = reply->error() != QNetworkReply::NoError;
    const auto body = !cancelled && reply->isOpen() ? reply->readAll() : QByteArray();
    reply->deleteLater();

    if (cancelled) {
        ProviderError error;
        error.code = "cancelled";
        error.message = "本地请求已取消";
        error.cancelled = true;
        if (pending.kind == Kind::Models)
            emit modelsFinished(requestId, {}, error);
        else
            emit completionFinished(requestId, ChatCompletionResult{{}, {}, {}, error});
        return;
    }
    if (networkError || status < 200 || status >= 300) {
        auto error = httpError(status, networkError);
        if (pending.kind == Kind::Models)
            emit modelsFinished(requestId, {}, error);
        else
            emit completionFinished(requestId, ChatCompletionResult{{}, {}, {}, error});
        return;
    }

    if (pending.kind == Kind::Models) {
        const auto parsed = parseDeepSeekModelCatalog(body);
        if (parsed.ok)
            emit modelsFinished(requestId, parsed.models, {});
        else
            emit modelsFinished(requestId, {}, invalidResponse(parsed.error));
        return;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit completionFinished(requestId, ChatCompletionResult{{}, {}, {}, invalidResponse("DeepSeek 返回了无效 JSON")});
        return;
    }
    const auto root = document.object();
    const auto choicesValue = root.value("choices");
    if (!choicesValue.isArray() || choicesValue.toArray().size() != 1 || !choicesValue.toArray().first().isObject()) {
        emit completionFinished(requestId, ChatCompletionResult{{}, {}, {}, invalidResponse("DeepSeek 响应缺少唯一结果")});
        return;
    }
    const auto choice = choicesValue.toArray().first().toObject();
    const auto message = choice.value("message");
    if (!message.isObject() || !message.toObject().value("content").isString()) {
        emit completionFinished(requestId, ChatCompletionResult{{}, {}, {}, invalidResponse("DeepSeek 响应正文为空或类型错误")});
        return;
    }
    const auto usage = root.value("usage");
    TokenUsage tokenUsage;
    if (usage.isObject()) {
        const auto usageObject = usage.toObject();
        tokenUsage.inputTokens = qMax<qint64>(0, qint64(usageObject.value("prompt_tokens").toDouble()));
        tokenUsage.outputTokens = qMax<qint64>(0, qint64(usageObject.value("completion_tokens").toDouble()));
        tokenUsage.totalTokens = qMax<qint64>(0, qint64(usageObject.value("total_tokens").toDouble()));
        tokenUsage.available = usageObject.contains("prompt_tokens") || usageObject.contains("completion_tokens") ||
                               usageObject.contains("total_tokens");
    }
    ChatCompletionResult result;
    result.content = message.toObject().value("content").toString();
    result.finishReason = choice.value("finish_reason").toString();
    result.usage = tokenUsage;
    if (result.finishReason != "stop") {
        ProviderError error;
        error.code = result.finishReason == "length" ? "truncated" : "incomplete";
        error.message = result.finishReason == "length"
            ? QStringLiteral("模型输出达到长度上限，歌词未保存")
            : QStringLiteral("模型未正常完成响应");
        result.error = error;
    } else if (result.content.trimmed().isEmpty()) {
        result.error = invalidResponse("DeepSeek 返回了空内容");
    }
    emit completionFinished(requestId, result);
}
} // namespace relaxer::ai
