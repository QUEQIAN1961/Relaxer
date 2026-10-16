#include "ai/ai-model.h"
#include "ai/ai-request-coordinator.h"
#include "ai/ai-runtime.h"
#include "ai/deepseek-provider.h"
#include "library.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
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
    void redirectsAreFailuresRatherThanSuccessfulEmptyCatalogs();
    void httpStatusMatrixAndCatalogFailurePreserveState();
    void endpointPolicyRejectsUnsafeRoutesAndProductionStoreMocks();
    void completionBoundaryRejectsUnconfirmedModelsAndBadCredentials();
    void cancellationTimeoutAndLateProviderRegistrationAreTerminal();
    void newestModelRequestOwnsCatalogPersistence();
    void networkFailureAndProviderTimeoutAreTerminal();
    void catalogPersistenceFailureKeepsPreviousState();
    void apiKeysAndNumericMetadataAreValidated();
    void completionFinishReasonsPreserveUsage_data();
    void completionFinishReasonsPreserveUsage();
    void operationBudgetsAllowSlowTranslationWithoutExtendingModelLookup();
    void completionTimeoutsAreSingleTerminalAndIdentifyTheirLayer();

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
        if (response.first >= 100 && response.first < 200) {
            // Informational responses are not final HTTP responses. Keep the
            // connection open without a final status to exercise our timeout.
            socket->write("HTTP/1.1 " + QByteArray::number(response.first) + " Informational\r\n\r\n");
            socket->flush();
            return;
        }
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
    bool allowsMockRequests() const override { return true; }
  private:
    QHash<QString, QString> m_values;
};

class GuardedCredentials final : public CredentialStore {
  public:
    int reads = 0;
    bool store(const QString &, const QString &, QString *) override { return false; }
    CredentialReadResult read(const QString &) override {
        ++reads;
        return {true, true, "fake-production-store-sentinel", {}};
    }
    bool remove(const QString &, QString *) override { return false; }
    bool supported() const override { return true; }
};

class ManualProvider final : public AiProvider {
  public:
    explicit ManualProvider(QString id = "deepseek") : m_id(std::move(id)) {}
    QString providerId() const override { return m_id; }
    bool cancel(quint64) override { ++cancellations; return false; }
    void deliver(quint64 id, const QList<ModelInfo> &models) { emit modelsFinished(id, models, {}); }
    quint64 serial = 0;
    int cancellations = 0;
  protected:
    quint64 listModels(const QString &) override { return ++serial; }
    quint64 complete(const QString &, const ChatCompletionRequest &) override { return ++serial; }
  private:
    QString m_id;
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
        DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
        FakeCredentials credentials;
        QVERIFY(credentials.store("deepseek", "fake-deepseek-key", nullptr));
        ProviderRegistry registry;
        QVERIFY(registry.add(&provider));
        ModelRegistry models;
        models.applyLiveCatalog("deepseek", {model()});
        AiRequestCoordinator coordinator(&registry, &credentials, &models);
        QSignalSpy modelsSpy(&coordinator, &AiRequestCoordinator::modelsFinished);
        server.enqueue(200, modelList());
        const auto modelsId = coordinator.fetchModels("deepseek");
        QTRY_COMPARE_WITH_TIMEOUT(modelsSpy.size(), 1, 3000);
        QCOMPARE(modelsSpy[0][0].toULongLong(), modelsId);
        QVERIFY(qvariant_cast<ProviderError>(modelsSpy[0][3]).isEmpty());
        QVERIFY(server.request(0).startsWith("GET /models "));
        QVERIFY(server.request(0).contains("Authorization: Bearer fake-deepseek-key"));

        server.enqueue(200, R"json({"choices":[{"index":0,"finish_reason":"stop","message":{"role":"assistant","content":"{\"translations\":[]}","reasoning_content":"not lyric text"}}],"usage":{"prompt_tokens":17,"completion_tokens":23,"total_tokens":40}})json");
        ChatCompletionRequest request;
        request.modelId = "deepseek-flash";
        request.messages = QJsonArray{QJsonObject{{"role", "user"}, {"content", "json"}}};
        request.maxTokens = 2048;
        QSignalSpy completionSpy(&coordinator, &AiRequestCoordinator::completionFinished);
        const auto completionId = coordinator.complete("deepseek", request);
        QTRY_COMPARE_WITH_TIMEOUT(completionSpy.size(), 1, 3000);
        QCOMPARE(completionSpy[0][0].toULongLong(), completionId);
        const auto completion = qvariant_cast<ChatCompletionResult>(completionSpy[0][2]);
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
        DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
        provider.setTimeoutMs(3000);
        FakeCredentials credentials;
        QVERIFY(credentials.store("deepseek", "fake-key", nullptr));
        ProviderRegistry registry;
        QVERIFY(registry.add(&provider));
        AiRequestCoordinator coordinator(&registry, &credentials);
        QSignalSpy finished(&coordinator, &AiRequestCoordinator::modelsFinished);
        server.enqueue(429, "{}");
        coordinator.fetchModels("deepseek");
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
        QCOMPARE(qvariant_cast<ProviderError>(finished[0][3]).code, QString("rate_limited"));
        server.setHold(true);
        const auto taskId = coordinator.fetchModels("deepseek");
        QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 2, 3000);
        QVERIFY(coordinator.isPending(taskId));
        QVERIFY(coordinator.cancel(taskId));
        QVERIFY(!coordinator.isPending(taskId));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 3000);
        QVERIFY(qvariant_cast<ProviderError>(finished[1][3]).cancelled);
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
        QCOMPARE(runtime.credentials()->read("deepseek").secret, QString("fake-only-key"));
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

void AiInfrastructureTests::redirectsAreFailuresRatherThanSuccessfulEmptyCatalogs() {
    HttpMock server;
    QVERIFY(server.listen());
    DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
    FakeCredentials credentials;
    QVERIFY(credentials.store("deepseek", "fake-key", nullptr));
    ProviderRegistry registry;
    QVERIFY(registry.add(&provider));
    AiRequestCoordinator coordinator(&registry, &credentials);
    QSignalSpy finished(&coordinator, &AiRequestCoordinator::modelsFinished);
    server.enqueue(302, "{}");
    coordinator.fetchModels("deepseek");
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    const auto error = qvariant_cast<ProviderError>(finished[0][3]);
    QCOMPARE(error.httpStatus, 302);
    QVERIFY2(!error.isEmpty(), "HTTP redirect was reported as success");
    QCOMPARE(server.requestCount(), 1);
}


void AiInfrastructureTests::httpStatusMatrixAndCatalogFailurePreserveState() {
    QTemporaryDir directory;
    Library library(directory.filePath("library"));
    QVERIFY(library.open());
    HttpMock server;
    QVERIFY(server.listen());
    AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{server.baseUrl()}, std::make_unique<FakeCredentials>());
    runtime.deepSeekProvider()->setTimeoutMs(1000);
    QVERIFY(runtime.credentials()->store("deepseek", "fake-status-key", nullptr));
    runtime.acceptModelCatalog("deepseek", {model()});
    auto config = runtime.configuration();
    config.defaults = {"deepseek", "deepseek-flash", false, {}};
    QVERIFY(runtime.saveConfiguration(config));
    const auto persisted = library.setting("aiServiceConfigV1");
    QSignalSpy finished(runtime.requests(), &AiRequestCoordinator::modelsFinished);
    const QList<int> statuses{199, 301, 302, 304, 400, 401, 402, 403, 404, 429, 500, 503, 204};
    for (const auto status : statuses) {
        server.enqueue(status, status == 204 ? QByteArray() : modelList());
        const auto task = runtime.requests()->fetchModels("deepseek");
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3500);
        const auto error = qvariant_cast<ProviderError>(finished[0][3]);
        QVERIFY2(!error.isEmpty(), qPrintable(QString("HTTP %1 reported success").arg(status)));
        if (status >= 300 && status < 400) QCOMPARE(error.code, QString("redirect"));
        QVERIFY(!runtime.requests()->isPending(task));
        QCOMPARE(runtime.model("deepseek", "deepseek-flash")->availability, ModelAvailability::Available);
        QCOMPARE(library.setting("aiServiceConfigV1"), persisted);
        finished.clear();
    }
    QCOMPARE(server.requestCount(), statuses.size());
    for (const auto body : {QByteArray(), QByteArray("{}"), QByteArray("{")}) {
        server.enqueue(200, body);
        runtime.requests()->fetchModels("deepseek");
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(qvariant_cast<ProviderError>(finished[0][3]).code, QString("invalid_response"));
        QCOMPARE(library.setting("aiServiceConfigV1"), persisted);
        finished.clear();
    }
    for (const int status : {200, 201}) {
        server.enqueue(status, modelList());
        runtime.requests()->fetchModels("deepseek");
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(qvariant_cast<ProviderError>(finished[0][3]).isEmpty());
        finished.clear();
    }
    QCOMPARE(runtime.models("deepseek").size(), 2);
}

void AiInfrastructureTests::endpointPolicyRejectsUnsafeRoutesAndProductionStoreMocks() {
    HttpMock server;
    QVERIFY(server.listen());
    FakeCredentials credentials;
    QVERIFY(credentials.store("deepseek", "fake-route-key", nullptr));
    const QList<QUrl> invalid{
        server.baseUrl(), QUrl("https://example.invalid"), QUrl("https://api.deepseek.com.evil.invalid"),
        QUrl("https://user@api.deepseek.com"), QUrl("https://api.deepseek.com:8443"),
        QUrl("file:///models"), QUrl("/relative"), QUrl("https://api.deepseek.com/other"),
        QUrl("https://api.deepseek.com?query"), QUrl("https://api.deepseek.com/#fragment")
    };
    for (const auto &url : invalid) {
        DeepSeekProvider provider(url); // production policy, including a loopback URL
        ProviderRegistry registry;
        QVERIFY(registry.add(&provider));
        AiRequestCoordinator coordinator(&registry, &credentials);
        QSignalSpy finished(&coordinator, &AiRequestCoordinator::modelsFinished);
        coordinator.fetchModels("deepseek");
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(qvariant_cast<ProviderError>(finished[0][3]).code, QString("unsafe_endpoint"));
    }
    QCOMPARE(server.requestCount(), 0);
    DeepSeekProvider mock(DeepSeekProvider::MockEndpoint{server.baseUrl()});
    ProviderRegistry registry;
    QVERIFY(registry.add(&mock));
    GuardedCredentials guarded;
    AiRequestCoordinator coordinator(&registry, &guarded);
    QSignalSpy finished(&coordinator, &AiRequestCoordinator::modelsFinished);
    coordinator.fetchModels("deepseek");
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(qvariant_cast<ProviderError>(finished[0][3]).code, QString("credentials_unavailable"));
    QCOMPARE(guarded.reads, 0); // never touch a real credential store in test mode
    QCOMPARE(server.requestCount(), 0);
}

void AiInfrastructureTests::completionBoundaryRejectsUnconfirmedModelsAndBadCredentials() {
    HttpMock server;
    QVERIFY(server.listen());
    DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
    ProviderRegistry providers;
    QVERIFY(providers.add(&provider));
    FakeCredentials credentials;
    ModelRegistry models;
    AiRequestCoordinator coordinator(&providers, &credentials, &models);
    QSignalSpy finished(&coordinator, &AiRequestCoordinator::completionFinished);
    ChatCompletionRequest base{"deepseek-flash", QJsonArray{QJsonObject{{"role","user"},{"content","json"}}}, false, {}, 2048};
    auto reject = [&](const ChatCompletionRequest &request) {
        const auto task = coordinator.complete("deepseek", request);
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(!qvariant_cast<ChatCompletionResult>(finished[0][2]).ok());
        QVERIFY(!coordinator.isPending(task));
        QCOMPARE(server.requestCount(), 0);
        finished.clear();
    };
    reject(base); // no verified catalog
    auto known = model();
    models.restoreUnverified({known});
    reject(base);
    models.applyLiveCatalog("deepseek", {known});
    reject(base); // no credential
    for (const auto &key : {QString(" "), QString("fake\r\nInjected: key"), QString("fake\tkey")}) {
        QVERIFY(credentials.store("deepseek", key, nullptr));
        reject(base);
    }
    QVERIFY(credentials.store("deepseek", " fake-boundary-key ", nullptr));
    auto request = base; request.modelId = "unknown";
    reject(request);
    request = base; request.thinkingEnabled = true; request.reasoningEffort = "medium";
    reject(request);
    request.reasoningEffort = " High ";
    reject(request); // canonical casing is consistent at every layer; no silent coercion
    request = base; request.maxTokens = known.capabilities.maxOutputTokens + 1;
    reject(request);
    for (int missing = 0; missing < 4; ++missing) {
        auto incomplete = known;
        if (missing == 0) incomplete.protocol.jsonObjectOutput = false;
        if (missing == 1) incomplete.capabilities.outputModalities.clear();
        if (missing == 2) incomplete.capabilities.contextWindow = 0;
        if (missing == 3) incomplete.capabilities.reasoningMetadataKnown = false;
        models.applyLiveCatalog("deepseek", {incomplete});
        request = base;
        if (missing == 3) { request.thinkingEnabled = true; request.reasoningEffort = "high"; }
        reject(request);
    }
    models.applyLiveCatalog("deepseek", {known});
    server.enqueue(200, R"({"choices":[{"finish_reason":"stop","message":{"content":"{}"}}]})");
    coordinator.complete("deepseek", base);
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(qvariant_cast<ChatCompletionResult>(finished[0][2]).ok());
    QCOMPARE(server.requestCount(), 1);
    QVERIFY(server.request(0).contains("Authorization: Bearer fake-boundary-key"));
}

void AiInfrastructureTests::cancellationTimeoutAndLateProviderRegistrationAreTerminal() {
    FakeCredentials credentials;
    QVERIFY(credentials.store("deepseek", "fake-task-key", nullptr));
    ProviderRegistry registry;
    AiRequestCoordinator coordinator(&registry, &credentials);
    QSignalSpy finished(&coordinator, &AiRequestCoordinator::modelsFinished);
    auto provider = std::make_unique<ManualProvider>();
    QVERIFY(registry.add(provider.get())); // added after coordinator construction
    auto task = coordinator.fetchModels("deepseek");
    QVERIFY(coordinator.isPending(task));
    QVERIFY(coordinator.cancel(task)); // provider deliberately returns false
    QVERIFY(!coordinator.isPending(task));
    QVERIFY(!coordinator.cancel(task));
    provider->deliver(1, {model()});
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(qvariant_cast<ProviderError>(finished[0][3]).cancelled);
    QTest::qWait(30);
    QCOMPARE(finished.size(), 1);
    coordinator.setTimeoutMs(20);
    task = coordinator.fetchModels("deepseek");
    QTRY_COMPARE(finished.size(), 2);
    QCOMPARE(qvariant_cast<ProviderError>(finished[1][3]).code, QString("timeout"));
    QVERIFY(!coordinator.isPending(task));
    provider->deliver(2, {model()});
    QTest::qWait(30);
    QCOMPARE(finished.size(), 2);
    task = coordinator.fetchModels("deepseek");
    provider.reset();
    QTRY_COMPARE(finished.size(), 3);
    QVERIFY(!coordinator.isPending(task));
    QVERIFY(!registry.provider("deepseek"));
    ManualProvider replacement;
    QVERIFY(registry.add(&replacement));
    coordinator.fetchModels("deepseek");
    replacement.deliver(1, {model()});
    QTRY_COMPARE(finished.size(), 4);
    QVERIFY(qvariant_cast<ProviderError>(finished[3][3]).isEmpty());
}

void AiInfrastructureTests::newestModelRequestOwnsCatalogPersistence() {
    QTemporaryDir directory;
    Library library(directory.filePath("library"));
    QVERIFY(library.open());
    auto credentials = std::make_unique<FakeCredentials>();
    QVERIFY(credentials->store("catalog-test", "fake-catalog-key", nullptr));
    AiRuntime runtime(&library, std::move(credentials));
    ManualProvider provider("catalog-test");
    QVERIFY(runtime.providers()->add(&provider));
    const auto oldTask = runtime.requests()->fetchModels("catalog-test");
    const auto latest = runtime.requests()->fetchModels("catalog-test");
    QVERIFY(!runtime.requests()->isPending(oldTask));
    auto newModel = model("new-model"); newModel.providerId = "catalog-test";
    auto oldModel = model("old-model"); oldModel.providerId = "catalog-test";
    provider.deliver(2, {newModel});
    QTRY_VERIFY(runtime.model("catalog-test", "new-model"));
    const auto persisted = library.setting("aiServiceConfigV1");
    provider.deliver(1, {oldModel});
    QTest::qWait(30);
    QVERIFY(!runtime.model("catalog-test", "old-model"));
    QCOMPARE(library.setting("aiServiceConfigV1"), persisted);
    QCOMPARE(runtime.requests()->latestModelTask("catalog-test"), latest);
}


void AiInfrastructureTests::networkFailureAndProviderTimeoutAreTerminal() {
    HttpMock server;
    QVERIFY(server.listen());
    server.setHold(true);
    DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
    provider.setTimeoutMs(1000);
    FakeCredentials credentials;
    QVERIFY(credentials.store("deepseek", "fake-timeout-key", nullptr));
    ProviderRegistry providers;
    QVERIFY(providers.add(&provider));
    AiRequestCoordinator coordinator(&providers, &credentials);
    QSignalSpy finished(&coordinator, &AiRequestCoordinator::modelsFinished);
    const auto task = coordinator.fetchModels("deepseek");
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    QCOMPARE(qvariant_cast<ProviderError>(finished[0][3]).code, QString("timeout"));
    QVERIFY(!coordinator.isPending(task));
    QCOMPARE(server.requestCount(), 1);
    QTest::qWait(30);
    QCOMPARE(finished.size(), 1);
    QTcpServer closed;
    QVERIFY(closed.listen(QHostAddress::LocalHost));
    const auto port = closed.serverPort();
    closed.close();
    DeepSeekProvider offline(DeepSeekProvider::MockEndpoint{QUrl(QString("http://127.0.0.1:%1").arg(port))});
    ProviderRegistry offlineRegistry;
    QVERIFY(offlineRegistry.add(&offline));
    AiRequestCoordinator offlineRequests(&offlineRegistry, &credentials);
    QSignalSpy offlineFinished(&offlineRequests, &AiRequestCoordinator::modelsFinished);
    const auto offlineTask = offlineRequests.fetchModels("deepseek");
    QTRY_COMPARE_WITH_TIMEOUT(offlineFinished.size(), 1, 8000);
    QCOMPARE(qvariant_cast<ProviderError>(offlineFinished[0][3]).code, QString("network"));
    QVERIFY(!offlineRequests.isPending(offlineTask));
}


void AiInfrastructureTests::catalogPersistenceFailureKeepsPreviousState() {
    QTemporaryDir directory;
    Library library(directory.filePath("library"));
    QVERIFY(library.open());
    AiRuntime runtime(&library, std::make_unique<FakeCredentials>());
    runtime.acceptModelCatalog("deepseek", {model()});
    const auto persisted = library.setting("aiServiceConfigV1");
    QSqlDatabase database;
    for (const auto &name : QSqlDatabase::connectionNames()) {
        auto candidate = QSqlDatabase::database(name);
        if (QFileInfo(candidate.databaseName()).absoluteFilePath() ==
            QFileInfo(library.absolute("relaxer.sqlite")).absoluteFilePath()) database = candidate;
    }
    QVERIFY(database.isValid());
    QSqlQuery query(database);
    QVERIFY(query.exec("PRAGMA query_only = ON"));
    runtime.acceptModelCatalog("deepseek", {model("deepseek-v4-pro")});
    QVERIFY2(!runtime.model("deepseek", "deepseek-v4-pro"), "Failed persistence changed the live model registry");
    QCOMPARE(runtime.model("deepseek", "deepseek-flash")->availability, ModelAvailability::Available);
    QCOMPARE(library.setting("aiServiceConfigV1"), persisted);
    QVERIFY(query.exec("PRAGMA query_only = OFF"));
}


void AiInfrastructureTests::apiKeysAndNumericMetadataAreValidated() {
    QString error;
    QCOMPARE(normalizedApiKey(" fake-key ", &error), QString("fake-key"));
    QVERIFY(error.isEmpty());
    for (const auto &key : {QString(), QString("   "), QString("fake key"), QString("fake\r\nkey"),
                           QString("fake\tkey"), QString("fake") + QChar(0), QString("fake") + QChar(0x2028)}) {
        QVERIFY(normalizedApiKey(key, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(!error.contains("fake"));
    }
    const auto malformed = parseDeepSeekModelCatalog(
        R"({"object":"list","data":[{"id":"deepseek-flash","context_window":1e300,"max_output_tokens":1.5}]})");
    QVERIFY(malformed.ok);
    QCOMPARE(malformed.models.first().capabilities.contextWindow, qint64(0));
    QCOMPARE(malformed.models.first().capabilities.maxOutputTokens, qint64(0));
    bool valid = true;
    QCOMPARE(nonNegativeJsonInteger(QJsonValue(1e300), &valid), qint64(0));
    QVERIFY(!valid);
    QCOMPARE(nonNegativeJsonInteger(QJsonValue(42), &valid), qint64(42));
    QVERIFY(valid);
    HttpMock server;
    QVERIFY(server.listen());
    QTemporaryDir directory;
    Library library(directory.filePath("library"));
    QVERIFY(library.open());
    AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{server.baseUrl()}, std::make_unique<FakeCredentials>());
    QVERIFY(runtime.credentials()->store("deepseek", "fake-numeric-key", nullptr));
    QVERIFY(runtime.acceptModelCatalog("deepseek", {model()}));
    QSignalSpy finished(runtime.requests(), &AiRequestCoordinator::completionFinished);
    const QList<QByteArray> usages{R"({"prompt_tokens":"secret","completion_tokens":1,"total_tokens":1})",
        R"({"prompt_tokens":1.5,"completion_tokens":1,"total_tokens":2})",
        R"({"prompt_tokens":1e300,"completion_tokens":1,"total_tokens":1e300})"};
    for (const auto &usage : usages) {
        server.enqueue(200, R"({"choices":[{"finish_reason":"stop","message":{"content":"{}"}}],"usage":)" + usage + "}");
        ChatCompletionRequest request{"deepseek-flash", QJsonArray{QJsonObject{{"role","user"},{"content","json"}}}, false, {}, 1024};
        runtime.requests()->complete("deepseek", request);
        QTRY_COMPARE(finished.size(), 1);
        const auto result = qvariant_cast<ChatCompletionResult>(finished[0][2]);
        QVERIFY(result.ok());
        QVERIFY(!result.usage.available);
        finished.clear();
    }
}

void AiInfrastructureTests::completionFinishReasonsPreserveUsage_data() {
    QTest::addColumn<QString>("reason");
    QTest::addColumn<QJsonValue>("content");
    QTest::addColumn<QString>("error");
    QTest::newRow("filter-null") << QString("content_filter") << QJsonValue(QJsonValue::Null) << QString("content_filter");
    QTest::newRow("filter-empty") << QString("content_filter") << QJsonValue("") << QString("content_filter");
    QTest::newRow("filter-text") << QString("content_filter") << QJsonValue("private-response-sentinel") << QString("content_filter");
    QTest::newRow("resource-null") << QString("insufficient_system_resource") << QJsonValue(QJsonValue::Null) << QString("insufficient_system_resource");
    QTest::newRow("aborted-null") << QString("aborted") << QJsonValue(QJsonValue::Null) << QString("aborted");
    QTest::newRow("length-null") << QString("length") << QJsonValue(QJsonValue::Null) << QString("truncated");
    QTest::newRow("tools-null") << QString("tool_calls") << QJsonValue(QJsonValue::Null) << QString("unexpected_tool_calls");
    QTest::newRow("unknown") << QString("private-unknown-sentinel") << QJsonValue(QJsonValue::Null) << QString("incomplete");
    QTest::newRow("stop-null") << QString("stop") << QJsonValue(QJsonValue::Null) << QString("invalid_response");
    QTest::newRow("stop-blank") << QString("stop") << QJsonValue("  ") << QString("empty_content");
    QTest::newRow("stop-ok") << QString("stop") << QJsonValue("{}") << QString();
}

void AiInfrastructureTests::completionFinishReasonsPreserveUsage() {
    QFETCH(QString, reason); QFETCH(QJsonValue, content); QFETCH(QString, error);
    HttpMock server; QVERIFY(server.listen());
    DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
    FakeCredentials credentials; QVERIFY(credentials.store("deepseek", "fake-finish-key", nullptr));
    ProviderRegistry providers; QVERIFY(providers.add(&provider));
    ModelRegistry models; models.applyLiveCatalog("deepseek", {model()});
    AiRequestCoordinator coordinator(&providers, &credentials, &models);
    QSignalSpy finished(&coordinator, &AiRequestCoordinator::completionFinished);
    QJsonObject response{{"choices", QJsonArray{QJsonObject{{"finish_reason",reason},
        {"message",QJsonObject{{"content",content},{"reasoning_content","private-reasoning-sentinel"}}}}}},
        {"usage",QJsonObject{{"prompt_tokens",17},{"completion_tokens",23},{"total_tokens",40}}}};
    server.enqueue(200, QJsonDocument(response).toJson(QJsonDocument::Compact));
    ChatCompletionRequest request{"deepseek-flash", QJsonArray{QJsonObject{{"role","user"},{"content","private-request-sentinel json"}}},false,{},1024};
    const auto task = coordinator.complete("deepseek",request);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,3000);
    const auto result = qvariant_cast<ChatCompletionResult>(finished[0][2]);
    QCOMPARE(result.error.code,error);
    QVERIFY(result.usage.available); QCOMPARE(result.usage.totalTokens,qint64(40));
    QCOMPARE(result.diagnostics.httpStatus,200);
    QCOMPARE(result.diagnostics.providerTimeoutMs,180000);
    QCOMPARE(result.diagnostics.coordinatorTimeoutMs,185000);
    QVERIFY(result.diagnostics.responseBytes>0);
    if (!error.isEmpty()) QVERIFY(result.content.isEmpty());
    QVERIFY(!result.content.contains("private-reasoning-sentinel"));
    QVERIFY(!result.error.message.contains("private-response-sentinel"));
    QVERIFY(!coordinator.isPending(task));
    QTest::qWait(40); QCOMPARE(finished.size(),1); QCOMPARE(server.requestCount(),1);
}

void AiInfrastructureTests::operationBudgetsAllowSlowTranslationWithoutExtendingModelLookup() {
    HttpMock server; QVERIFY(server.listen()); server.setHold(true);
    DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
    QCOMPARE(provider.timeoutBudget(RequestOperation::Models).coordinatorMs,31000);
    QCOMPARE(provider.timeoutBudget(RequestOperation::Completion).coordinatorMs,185000);
    provider.setTimeoutMs(RequestOperation::Models,1000);
    provider.setTimeoutMs(RequestOperation::Completion,3000);
    FakeCredentials credentials; QVERIFY(credentials.store("deepseek","fake-slow-key",nullptr));
    ProviderRegistry providers; QVERIFY(providers.add(&provider));
    ModelRegistry models; models.applyLiveCatalog("deepseek",{model()});
    AiRequestCoordinator coordinator(&providers,&credentials,&models);
    QSignalSpy lookup(&coordinator,&AiRequestCoordinator::modelsFinished);
    QSignalSpy translated(&coordinator,&AiRequestCoordinator::completionFinished);
    const auto listTask=coordinator.fetchModels("deepseek");
    ChatCompletionRequest request{"deepseek-flash",QJsonArray{QJsonObject{{"role","user"},{"content","json"}}},false,{},1024};
    const auto translationTask=coordinator.complete("deepseek",request);
    provider.setTimeoutMs(RequestOperation::Completion,1000); // pending task keeps its captured 3 s budget
    // Both start together. The old shared 1 s duration would kill both.
    QTimer::singleShot(1600,&server,[&server]{
        server.enqueue(200,R"({"choices":[{"finish_reason":"stop","message":{"content":"{}"}}]})");
        server.releaseHeld();
    });
    QTRY_COMPARE_WITH_TIMEOUT(lookup.size(),1,3000);
    QCOMPARE(qvariant_cast<ProviderError>(lookup[0][3]).code,QString("timeout"));
    QTRY_COMPARE_WITH_TIMEOUT(translated.size(),1,4000);
    const auto result=qvariant_cast<ChatCompletionResult>(translated[0][2]);
    QVERIFY(result.ok()); QVERIFY(result.diagnostics.elapsedMs>=1000);
    QCOMPARE(result.diagnostics.providerTimeoutMs,3000);
    QCOMPARE(result.diagnostics.coordinatorTimeoutMs,8000);
    QVERIFY(!coordinator.isPending(listTask)); QVERIFY(!coordinator.isPending(translationTask));
    QCOMPARE(server.requestCount(),2);
    // A new budget cannot mutate an already dispatched request's captured deadline.
    const auto maximum=timeoutBudgetFor(RequestOperation::Completion,300000);
    QVERIFY(maximum.coordinatorMs>maximum.providerMs); QCOMPARE(maximum.coordinatorMs,300000);
}

void AiInfrastructureTests::completionTimeoutsAreSingleTerminalAndIdentifyTheirLayer() {
    for (const bool watchdog : {false,true}) {
        HttpMock server; QVERIFY(server.listen()); server.setHold(true);
        DeepSeekProvider provider(DeepSeekProvider::MockEndpoint{server.baseUrl()});
        provider.setTimeoutMs(RequestOperation::Completion,1000);
        FakeCredentials credentials; QVERIFY(credentials.store("deepseek","fake-timeout-key",nullptr));
        ProviderRegistry providers; QVERIFY(providers.add(&provider));
        ModelRegistry models; models.applyLiveCatalog("deepseek",{model()});
        AiRequestCoordinator coordinator(&providers,&credentials,&models);
        if(watchdog)coordinator.setTimeoutMs(100); // fault injection: force outer watchdog first
        QSignalSpy finished(&coordinator,&AiRequestCoordinator::completionFinished);
        ChatCompletionRequest request{"deepseek-flash",QJsonArray{QJsonObject{{"role","user"},{"content","json"}}},false,{},1024};
        const auto task=coordinator.complete("deepseek",request);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,3000);
        const auto result=qvariant_cast<ChatCompletionResult>(finished[0][2]);
        QCOMPARE(result.error.code,QString("timeout"));
        QCOMPARE(result.diagnostics.timeoutSource,watchdog?QString("coordinator"):QString("provider"));
        QVERIFY(!result.usage.available); QVERIFY(!coordinator.isPending(task));
        QTest::qWait(80); QCOMPARE(finished.size(),1); QCOMPARE(server.requestCount(),1);
    }
}

QTEST_GUILESS_MAIN(AiInfrastructureTests)
#include "ai-infrastructure-tests.moc"
