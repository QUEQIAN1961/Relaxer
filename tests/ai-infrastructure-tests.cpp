#include "ai/ai-model.h"
#include "ai/ai-request-coordinator.h"
#include "ai/ai-runtime.h"
#include "ai/deepseek-provider.h"
#include "library.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

using namespace relaxer;
using namespace relaxer::ai;

class AiInfrastructureTests final : public QObject {
    Q_OBJECT
  private slots:
    void modelCatalogUsesIdsAndCapabilities();
    void modelCatalogHandlesEmptyAndMalformedResponses();
    void chatPayloadMapsThinkingWithoutFakeEffort();
    void deepSeekProviderUsesOfficialEndpointsAndParsesUsage();
    void providerClassifiesHttpErrorsAndCancellation();
    void runtimePersistsConfigurationAndKeepsCredentialsSeparate();
};

namespace {
class HttpMock final : public QObject {
  public:
    explicit HttpMock(QObject *parent = nullptr) : QObject(parent) {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (m_server.hasPendingConnections()) {
                auto *socket = m_server.nextPendingConnection();
                m_buffers.insert(socket, {});
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { consume(socket); });
                connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                    m_buffers.remove(socket);
                    m_held.removeAll(socket);
                    socket->deleteLater();
                });
            }
        });
    }
    ~HttpMock() override {
        for (auto *socket : m_buffers.keys()) {
            if (!socket)
                continue;
            QObject::disconnect(socket, nullptr, this, nullptr);
            socket->abort();
        }
    }
    bool listen() { return m_server.listen(QHostAddress::LocalHost); }
    QUrl baseUrl() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }
    void enqueue(int status, QByteArray body) { m_responses.append({status, std::move(body)}); }
    void setHold(bool hold) { m_hold = hold; }
    int requestCount() const { return m_requests.size(); }
    QByteArray request(int index) const { return m_requests.value(index); }
    bool releaseHeld() {
        if (m_held.isEmpty() || m_responses.isEmpty())
            return false;
        auto *socket = m_held.takeFirst();
        respond(socket);
        return true;
    }
  private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<QTcpSocket *> m_held;
    QList<QByteArray> m_requests;
    QList<QPair<int, QByteArray>> m_responses;
    bool m_hold = false;

    void consume(QTcpSocket *socket) {
        auto &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const auto headersEnd = buffer.indexOf("\r\n\r\n");
        if (headersEnd < 0)
            return;
        const auto headers = buffer.left(headersEnd + 4);
        qint64 length = 0;
        const auto lines = headers.split('\n');
        for (const auto &line : lines) {
            if (line.toLower().startsWith("content-length:"))
                length = line.mid(line.indexOf(':') + 1).trimmed().toLongLong();
        }
        if (buffer.size() < headersEnd + 4 + length)
            return;
        m_requests.append(buffer.left(headersEnd + 4 + length));
        if (m_hold) {
            m_held.append(socket);
            return;
        }
        respond(socket);
    }
    void respond(QTcpSocket *socket) {
        if (!socket || m_responses.isEmpty())
            return;
        const auto response = m_responses.takeFirst();
        const auto reason = response.first >= 200 && response.first < 300 ? "OK" : "Error";
        QByteArray bytes = "HTTP/1.1 " + QByteArray::number(response.first) + " " + reason + "\r\n";
        bytes += "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(response.second.size()) +
                 "\r\nConnection: close\r\n\r\n" + response.second;
        socket->write(bytes);
        socket->flush();
        socket->disconnectFromHost();
    }
};

class FakeCredentials final : public CredentialStore {
  public:
    bool store(const QString &providerId, const QString &secret, QString *error) override {
        if (providerId.isEmpty() || secret.isEmpty()) {
            if (error) *error = "invalid";
            return false;
        }
        m_values.insert(providerId, secret);
        if (error) error->clear();
        return true;
    }
    CredentialReadResult read(const QString &providerId) override {
        CredentialReadResult result;
        result.ok = true;
        result.found = m_values.contains(providerId);
        result.secret = m_values.value(providerId);
        return result;
    }
    bool remove(const QString &providerId, QString *error) override {
        m_values.remove(providerId);
        if (error) error->clear();
        return true;
    }
    bool supported() const override { return true; }
  private:
    QHash<QString, QString> m_values;
};

ModelInfo model(const QString &id = "deepseek-flash") {
    ModelInfo result;
    result.providerId = "deepseek";
    result.id = id;
    result.displayName = "DeepSeek Flash long model name";
    result.availability = ModelAvailability::Available;
    result.capabilities.inputModalities = {"text"};
    result.capabilities.outputModalities = {"text"};
    result.capabilities.reasoningMetadataKnown = true;
    result.capabilities.reasoningEfforts = {"low", "high", "max"};
    result.capabilities.defaultReasoningEffort = "high";
    result.capabilities.contextWindow = 1048576;
    result.capabilities.maxOutputTokens = 393216;
    if (id == "deepseek-flash" || id == "deepseek-v4-pro") {
        result.protocol.chatCompletions = true;
        result.protocol.jsonObjectOutput = true;
        result.protocol.maxTokens = true;
        result.protocol.explicitThinkingControl = true;
    }
    return result;
}

QByteArray modelList() {
    return R"json({"object":"list","data":[{"id":"deepseek-flash","object":"model","name":"DeepSeek Flash Display","context_window":1048576,"max_output_tokens":393216,"input_modalities":["text","image"],"output_modalities":["text"],"effort":{"supported_levels":["low","high","max"],"default_level":"high"}},{"id":"deepseek-v4-pro","object":"model","name":"DeepSeek Pro","context_window":1048576,"max_output_tokens":393216,"input_modalities":["text"],"output_modalities":["text"],"effort":{"supported_levels":["low","high","max"],"default_level":"high"}}]})json";
}
} // namespace

class AiInfrastructureTestCases final {
  public:
    void modelCatalogUsesIdsAndCapabilities() {
        const auto parsed = parseDeepSeekModelCatalog(modelList());
        QVERIFY(parsed.ok);
        QCOMPARE(parsed.models.size(), 2);
        QCOMPARE(parsed.models[0].id, QString("deepseek-flash"));
        QCOMPARE(parsed.models[0].displayName, QString("DeepSeek Flash Display"));
        QCOMPARE(parsed.models[0].availability, ModelAvailability::Available);
        QCOMPARE(parsed.models[0].capabilities.contextWindow, qint64(1048576));
        QCOMPARE(parsed.models[0].capabilities.reasoningEfforts, QStringList({"low", "high", "max"}));
        QVERIFY(parsed.models[0].protocol.supportsStructuredChat());

        const auto withUnknown = parseDeepSeekModelCatalog(
            R"json({"object":"list","data":[{"id":"future-model","effort":{"supported_levels":["medium","ultra","low"]}}]})json");
        QVERIFY(withUnknown.ok);
        QCOMPARE(withUnknown.models[0].displayName, QString("future-model"));
        QCOMPARE(withUnknown.models[0].capabilities.reasoningEfforts, QStringList({"low"}));
        QVERIFY(withUnknown.models[0].capabilities.reasoningMetadataKnown);
        QVERIFY(!withUnknown.models[0].protocol.supportsStructuredChat());
    }

    void modelCatalogHandlesEmptyAndMalformedResponses() {
        QVERIFY(parseDeepSeekModelCatalog(R"json({"object":"list","data":[]})json").ok);
        QVERIFY(!parseDeepSeekModelCatalog(R"json({"object":"list","data":null})json").ok);
        QVERIFY(!parseDeepSeekModelCatalog("not json").ok);
        const auto duplicate = parseDeepSeekModelCatalog(
            R"json({"object":"list","data":[{"id":"same"},{"id":"same","name":"other"}]})json");
        QVERIFY(duplicate.ok);
        QCOMPARE(duplicate.models.size(), 1);
    }

    void chatPayloadMapsThinkingWithoutFakeEffort() {
        ChatCompletionRequest request;
        request.modelId = "deepseek-flash";
        request.messages = QJsonArray{QJsonObject{{"role", "user"}, {"content", "json"}}};
        request.maxTokens = 2048;
        auto disabled = deepSeekChatPayload(request);
        QCOMPARE(disabled.value("thinking").toObject().value("type").toString(), QString("disabled"));
        QVERIFY(!disabled.contains("reasoning_effort"));
        QCOMPARE(disabled.value("response_format").toObject().value("type").toString(), QString("json_object"));

        request.thinkingEnabled = true;
        request.reasoningEffort = "high";
        auto enabled = deepSeekChatPayload(request);
        QCOMPARE(enabled.value("thinking").toObject().value("type").toString(), QString("enabled"));
        QCOMPARE(enabled.value("reasoning_effort").toString(), QString("high"));
        request.reasoningEffort = "medium";
        QString error;
        QVERIFY(deepSeekChatPayload(request, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void deepSeekProviderUsesOfficialEndpointsAndParsesUsage() {
        HttpMock server;
        QVERIFY(server.listen());
        DeepSeekProvider provider(server.baseUrl());
        provider.setTimeoutMs(3000);
        QSignalSpy modelsSpy(&provider, &AiProvider::modelsFinished);
        server.enqueue(200, modelList());
        const auto modelsId = provider.listModels("fake-deepseek-key");
        QTRY_COMPARE_WITH_TIMEOUT(modelsSpy.size(), 1, 3000);
        QCOMPARE(modelsSpy[0][0].toULongLong(), modelsId);
        QVERIFY(qvariant_cast<ProviderError>(modelsSpy[0][2]).isEmpty());
        QVERIFY(server.request(0).startsWith("GET /models "));
        QVERIFY(server.request(0).contains("Authorization: Bearer fake-deepseek-key"));

        server.enqueue(200, R"json({"choices":[{"index":0,"finish_reason":"stop","message":{"role":"assistant","content":"{\"translations\":[]}","reasoning_content":"not lyric text"}}],"usage":{"prompt_tokens":17,"completion_tokens":23,"total_tokens":40}})json");
        ChatCompletionRequest request;
        request.modelId = "deepseek-flash";
        request.messages = QJsonArray{QJsonObject{{"role", "user"}, {"content", "json"}}};
        request.maxTokens = 2048;
        QSignalSpy completionSpy(&provider, &AiProvider::completionFinished);
        const auto completionId = provider.complete("fake-deepseek-key", request);
        QTRY_COMPARE_WITH_TIMEOUT(completionSpy.size(), 1, 3000);
        QCOMPARE(completionSpy[0][0].toULongLong(), completionId);
        const auto completion = qvariant_cast<ChatCompletionResult>(completionSpy[0][1]);
        QVERIFY(completion.ok());
        QCOMPARE(completion.content, QString("{\"translations\":[]}"));
        QCOMPARE(completion.finishReason, QString("stop"));
        QVERIFY(completion.usage.available);
        QCOMPARE(completion.usage.totalTokens, qint64(40));
        QVERIFY(server.request(1).startsWith("POST /chat/completions "));
        QVERIFY(server.request(1).contains("Authorization: Bearer fake-deepseek-key"));
        QVERIFY(server.request(1).contains("\"thinking\":{\"type\":\"disabled\"}"));
    }

    void providerClassifiesHttpErrorsAndCancellation() {
        HttpMock server;
        QVERIFY(server.listen());
        DeepSeekProvider provider(server.baseUrl());
        provider.setTimeoutMs(3000);
        QSignalSpy modelsSpy(&provider, &AiProvider::modelsFinished);
        server.enqueue(429, "{}");
        provider.listModels("fake-key");
        QTRY_COMPARE_WITH_TIMEOUT(modelsSpy.size(), 1, 3000);
        QCOMPARE(qvariant_cast<ProviderError>(modelsSpy[0][2]).code, QString("rate_limited"));

        ProviderRegistry registry;
        QVERIFY(registry.add(&provider));
        AiRequestCoordinator coordinator(&registry);
        server.setHold(true);
        QSignalSpy coordinated(&coordinator, &AiRequestCoordinator::modelsFinished);
        const auto taskId = coordinator.fetchModels("deepseek", "fake-key");
        QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 2, 3000);
        QVERIFY(coordinator.isPending(taskId));
        QVERIFY(coordinator.cancel(taskId));
        QTRY_COMPARE_WITH_TIMEOUT(coordinated.size(), 1, 3000);
        const auto cancelled = qvariant_cast<ProviderError>(coordinated[0][3]);
        QVERIFY(cancelled.cancelled);
        QVERIFY(!coordinator.isPending(taskId));
    }

    void runtimePersistsConfigurationAndKeepsCredentialsSeparate() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        const QJsonObject unknown{{"nested", QJsonObject{{"keep", true}}}};
        QVERIFY(library.setSetting("b5.1UnknownSetting", unknown));

        auto fake = std::make_unique<FakeCredentials>();
        auto *credentials = fake.get();
        AiRuntime runtime(&library, std::move(fake));
        AiServiceConfiguration configuration;
        configuration.defaults = {"deepseek", "deepseek-flash", false, {}};
        configuration.translationUsesDefaults = false;
        configuration.translationOverride = {"deepseek", "deepseek-v4-pro", true, "max"};
        configuration.translationStylePrompt = "保留意象";
        QVERIFY(runtime.saveConfiguration(configuration));
        QVERIFY(credentials->store("deepseek", "fake-only-key", nullptr));
        QVERIFY(runtime.hasCredential("deepseek"));
        QCOMPARE(runtime.readCredential("deepseek"), QString("fake-only-key"));
        QVERIFY(!library.setting("aiServiceConfigV1").toString().contains("fake-only-key"));

        QCOMPARE(runtime.validateConfiguration(runtime.translationConfiguration()),
                 QString("请先刷新模型列表并确认模型可用"));
        runtime.acceptModelCatalog("deepseek", {model(), model("deepseek-v4-pro")});
        QVERIFY(runtime.validateConfiguration(runtime.translationConfiguration()).isEmpty());
        auto unconfirmed = model("future-model");
        runtime.acceptModelCatalog("deepseek", {model(), model("deepseek-v4-pro"), unconfirmed});
        auto futureConfiguration = runtime.translationConfiguration();
        futureConfiguration.modelId = "future-model";
        QVERIFY(runtime.validateConfiguration(futureConfiguration).contains("协议尚未确认"));
        runtime.acceptModelCatalog("deepseek", {});
        QCOMPARE(runtime.validateConfiguration(runtime.translationConfiguration()),
                 QString("所选模型尚未验证可用，请刷新模型列表"));
        QCOMPARE(runtime.configuration().translationOverride.modelId, QString("deepseek-v4-pro"));
        QCOMPARE(library.setting("b5.1UnknownSetting").toObject(), unknown);

        QVERIFY(credentials->remove("deepseek", nullptr));
        QVERIFY(!runtime.hasCredential("deepseek"));
        AiRuntime reopened(&library, std::make_unique<FakeCredentials>());
        QCOMPARE(reopened.configuration().translationStylePrompt, QString("保留意象"));
        QCOMPARE(reopened.translationConfiguration().reasoningEffort, QString("max"));
        QCOMPARE(reopened.models("deepseek").size(), 3);
        QCOMPARE(reopened.models("deepseek").first().availability, ModelAvailability::Unverified);
        QVERIFY(reopened.models("deepseek").first().protocol.supportsStructuredChat());
        QVERIFY(!reopened.models("deepseek").last().protocol.supportsStructuredChat());
    }
};

void AiInfrastructureTests::modelCatalogUsesIdsAndCapabilities() { AiInfrastructureTestCases{}.modelCatalogUsesIdsAndCapabilities(); }
void AiInfrastructureTests::modelCatalogHandlesEmptyAndMalformedResponses() { AiInfrastructureTestCases{}.modelCatalogHandlesEmptyAndMalformedResponses(); }
void AiInfrastructureTests::chatPayloadMapsThinkingWithoutFakeEffort() { AiInfrastructureTestCases{}.chatPayloadMapsThinkingWithoutFakeEffort(); }
void AiInfrastructureTests::deepSeekProviderUsesOfficialEndpointsAndParsesUsage() { AiInfrastructureTestCases{}.deepSeekProviderUsesOfficialEndpointsAndParsesUsage(); }
void AiInfrastructureTests::providerClassifiesHttpErrorsAndCancellation() { AiInfrastructureTestCases{}.providerClassifiesHttpErrorsAndCancellation(); }
void AiInfrastructureTests::runtimePersistsConfigurationAndKeepsCredentialsSeparate() { AiInfrastructureTestCases{}.runtimePersistsConfigurationAndKeepsCredentialsSeparate(); }

QTEST_GUILESS_MAIN(AiInfrastructureTests)
#include "ai-infrastructure-tests.moc"
