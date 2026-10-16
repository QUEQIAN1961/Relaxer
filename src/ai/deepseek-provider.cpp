#include "deepseek-provider.h"

#include "ai-model.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QNetworkProxy>
#include <utility>

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
    } else if (status >= 300 && status < 400) {
        error.code = "redirect";
        error.message = "DeepSeek 返回了重定向，未向其他端点发送凭据";
    } else if (networkError) {
        error.code = "network";
        error.message = "网络连接失败";
    } else {
        error.code = "http";
        error.message = QString("DeepSeek 返回了异常 HTTP 状态（%1）").arg(status);
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
        const auto &effort = request.reasoningEffort;
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
    if (m_baseUrl.host() == "127.0.0.1" || m_baseUrl.host() == "::1")
        m_network.setProxy(QNetworkProxy::NoProxy);
}

DeepSeekProvider::DeepSeekProvider(MockEndpoint endpoint, QObject *parent)
    : DeepSeekProvider(std::move(endpoint.url), parent) {
    m_mockEndpoint = true;
}

DeepSeekProvider::~DeepSeekProvider() {
    // Disconnect before member destruction: QNetworkAccessManager aborts replies
    // while it is being destroyed, after m_pending would otherwise be gone.
    for (const auto &pending : std::as_const(m_pending)) {
        if (pending.timer) pending.timer->stop();
        if (pending.reply) {
            disconnect(pending.reply, nullptr, this, nullptr);
            pending.reply->abort();
        }
    }
    m_pending.clear();
}

QString DeepSeekProvider::endpointError() const {
    if (!m_baseUrl.isValid() || m_baseUrl.isRelative() || m_baseUrl.host().isEmpty() ||
        m_baseUrl.authority().contains('@') || m_baseUrl.hasQuery() || m_baseUrl.hasFragment())
        return QStringLiteral("AI 请求端点无效");
    if (m_mockEndpoint) {
        if (m_baseUrl.scheme() == "http" &&
            (m_baseUrl.host() == "127.0.0.1" || m_baseUrl.host() == "::1") &&
            m_baseUrl.path().isEmpty() && m_baseUrl.port() > 0)
            return {};
        return QStringLiteral("测试端点必须是显式回环 HTTP 地址");
    }
    if (m_baseUrl.scheme() != "https" || m_baseUrl.host() != "api.deepseek.com" ||
        (m_baseUrl.port() != -1 && m_baseUrl.port() != 443) ||
        (!m_baseUrl.path().isEmpty() && m_baseUrl.path() != "/v1"))
        return QStringLiteral("DeepSeek 凭据只能发送到官方 HTTPS 端点");
    return {};
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

void DeepSeekProvider::setTimeoutMs(int timeoutMs) {
    setTimeoutMs(RequestOperation::Models, timeoutMs);
    setTimeoutMs(RequestOperation::Completion, timeoutMs);
}
void DeepSeekProvider::setTimeoutMs(RequestOperation operation, int timeoutMs) {
    auto &duration = operation == RequestOperation::Models ? m_modelsTimeoutMs : m_completionTimeoutMs;
    duration = timeoutBudgetFor(operation, timeoutMs).providerMs;
}
TimeoutBudget DeepSeekProvider::timeoutBudget(RequestOperation operation) const {
    return timeoutBudgetFor(operation, operation == RequestOperation::Models ? m_modelsTimeoutMs : m_completionTimeoutMs);
}

quint64 DeepSeekProvider::listModels(const QString &apiKey) {
    return send(Kind::Models, apiKey);
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
    return send(Kind::Completion, apiKey, QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

quint64 DeepSeekProvider::send(Kind kind, const QString &apiKey, const QByteArray &body) {
    const quint64 id = ++m_nextRequestId;
    const auto invalidEndpoint = endpointError();
    if (!invalidEndpoint.isEmpty()) {
        const ProviderError error{"unsafe_endpoint", invalidEndpoint};
        QTimer::singleShot(0, this, [this, id, kind, error] {
            if (kind == Kind::Models)
                emit modelsFinished(id, {}, error);
            else
                emit completionFinished(id, ChatCompletionResult{{}, {}, {}, error});
        });
        return id;
    }
    QNetworkRequest request(endpoint(kind == Kind::Models ? "/models" : "/chat/completions"));
    auto authorization = QByteArray("Bearer ") + apiKey.toUtf8();
    request.setRawHeader("Authorization", authorization);
    authorization.fill('\0');
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(0);
    if (kind == Kind::Completion)
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply = kind == Kind::Models ? m_network.get(request) : m_network.post(request, body);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    Pending pending{kind, reply, timer, {}, timeoutBudget(kind == Kind::Models ? RequestOperation::Models : RequestOperation::Completion)};
    pending.elapsed.start();
    m_pending.insert(id, pending);
    connect(timer, &QTimer::timeout, reply, [reply] {
        if (!reply->isFinished()) {
            reply->setProperty("relaxerTimedOut", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, id] { finish(id); });
    timer->start(pending.budget.providerMs);
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
    const bool timedOut = reply->property("relaxerTimedOut").toBool();
    const bool networkError = reply->error() != QNetworkReply::NoError;
    const auto body = !cancelled && reply->isOpen() ? reply->readAll() : QByteArray();
    RequestDiagnostics diagnostics;
    diagnostics.elapsedMs = pending.elapsed.elapsed();
    diagnostics.responseBytes = body.size();
    diagnostics.httpStatus = status;
    diagnostics.networkError = int(reply->error());
    diagnostics.providerTimeoutMs = pending.budget.providerMs;
    diagnostics.coordinatorTimeoutMs = pending.budget.coordinatorMs;
    if (timedOut) diagnostics.timeoutSource = QStringLiteral("provider");
    auto complete = [this, requestId, diagnostics](ChatCompletionResult result) {
        result.diagnostics = diagnostics;
        if (!result.ok()) result.content.clear();
        emit completionFinished(requestId, result);
    };
    reply->deleteLater();

    if (cancelled) {
        ProviderError error;
        error.code = "cancelled";
        error.message = "本地请求已取消";
        error.cancelled = true;
        if (pending.kind == Kind::Models)
            emit modelsFinished(requestId, {}, error);
        else
            complete(ChatCompletionResult{{}, {}, {}, error});
        return;
    }
    if (networkError || status < 200 || status >= 300) {
        auto error = timedOut ? ProviderError{"timeout", QStringLiteral("AI 请求超时，请手动重试"), status}
                              : httpError(status, networkError);
        if (pending.kind == Kind::Models)
            emit modelsFinished(requestId, {}, error);
        else
            complete(ChatCompletionResult{{}, {}, {}, error});
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
        complete(ChatCompletionResult{{}, {}, {}, invalidResponse("DeepSeek 返回了无效 JSON")});
        return;
    }
    const auto root = document.object();
    const auto usage = root.value("usage");
    TokenUsage tokenUsage;
    if (usage.isObject()) {
        const auto usageObject = usage.toObject();
        bool inputValid = false, outputValid = false, totalValid = false;
        tokenUsage.inputTokens = nonNegativeJsonInteger(usageObject.value("prompt_tokens"), &inputValid);
        tokenUsage.outputTokens = nonNegativeJsonInteger(usageObject.value("completion_tokens"), &outputValid);
        tokenUsage.totalTokens = nonNegativeJsonInteger(usageObject.value("total_tokens"), &totalValid);
        tokenUsage.available = inputValid && outputValid && totalValid;
    }
    ChatCompletionResult result;
    result.usage = tokenUsage;
    const auto choicesValue = root.value("choices");
    if (!choicesValue.isArray() || choicesValue.toArray().size() != 1 || !choicesValue.toArray().first().isObject()) {
        result.error = invalidResponse("DeepSeek 响应缺少唯一结果");
        complete(result);
        return;
    }
    const auto choice = choicesValue.toArray().first().toObject();
    result.finishReason = choice.value("finish_reason").toString();
    if (result.finishReason != "stop") {
        if (result.finishReason == "length")
            result.error = {"truncated", QStringLiteral("模型输出达到长度上限，歌词未保存")};
        else if (result.finishReason == "content_filter")
            result.error = {"content_filter", QStringLiteral("供应商过滤了本次内容，未保存歌词；请检查原文后手动处理")};
        else if (result.finishReason == "insufficient_system_resource")
            result.error = {"insufficient_system_resource", QStringLiteral("供应商推理资源不足，请稍后手动重试")};
        else if (result.finishReason == "aborted")
            result.error = {"aborted", QStringLiteral("供应商中断了生成，请稍后手动重试")};
        else if (result.finishReason == "tool_calls")
            result.error = {"unexpected_tool_calls", QStringLiteral("供应商返回了意外工具调用，歌词未保存")};
        else {
            result.finishReason = QStringLiteral("unknown");
            result.error = {"incomplete", QStringLiteral("模型未正常完成响应")};
        }
    } else {
        const auto message = choice.value("message");
        if (!message.isObject() || !message.toObject().value("content").isString())
            result.error = invalidResponse("DeepSeek 响应正文为空或类型错误");
        else {
            result.content = message.toObject().value("content").toString();
            if (result.content.trimmed().isEmpty())
                result.error = {"empty_content", QStringLiteral("DeepSeek 返回了空内容，未保存歌词")};
        }
    }
    complete(result);
}
} // namespace relaxer::ai
