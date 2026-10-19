#include "ui-bridge/ai-bridge.h"
#include "translation-fixture.h"
#include <QSignalSpy>

namespace {
class CountingCredentials final : public CredentialStore {
public:
    int reads = 0;
    bool failReads = false;
    QHash<QString, QString> values;
    bool store(const QString &provider, const QString &secret, QString *error) override {
        values.insert(provider, secret);
        if (error) error->clear();
        return true;
    }
    CredentialReadResult read(const QString &provider) override {
        ++reads;
        if (failReads) return {false, false, {}, "private store error sentinel"};
        return {true, values.contains(provider), values.value(provider), {}};
    }
    bool remove(const QString &provider, QString *error) override {
        values.remove(provider);
        if (error) error->clear();
        return true;
    }
    bool supported() const override { return true; }
    bool allowsMockRequests() const override { return true; }
};

struct Fixture {
    QTemporaryDir directory;
    Library library{directory.filePath("data")};
    TranslationHttpMock http;
    CountingCredentials *credentials = nullptr;
    std::unique_ptr<AiRuntime> runtime;
    std::unique_ptr<AiBridge> bridge;
    bool ok = false;
    Fixture() {
        if (!directory.isValid() || !library.open() || !http.listen()) return;
        auto store = std::make_unique<CountingCredentials>();
        credentials = store.get();
        runtime = std::make_unique<AiRuntime>(&library,
            DeepSeekProvider::MockEndpoint{http.baseUrl()}, std::move(store));
        bridge = std::make_unique<AiBridge>(runtime.get());
        ok = true;
    }
    bool selectProvider() { return bridge->edit("providerId", "deepseek"); }
    bool configure() {
        return configureRuntime(*runtime);
    }
    QByteArray configurationBytes() const {
        return QJsonDocument(library.setting("aiServiceConfigV1").toObject()).toJson(QJsonDocument::Compact);
    }
};

const QByteArray Catalog = R"({"object":"list","data":[{"id":"deepseek-flash","name":"Flash display","context_window":1048576,"max_output_tokens":393216,"input_modalities":["text"],"output_modalities":["text"],"effort":{"supported_levels":["low","high","max"],"default_level":"high"}}]})";

bool hasChoice(const QVariantList &choices, const QString &id, const QString &labelFragment = {}) {
    for (const auto &choice : choices) {
        const auto fields = choice.toMap();
        if (fields.value("id").toString() == id &&
            (labelFragment.isEmpty() || fields.value("label").toString().contains(labelFragment))) return true;
    }
    return false;
}
}

class QmlAiParityTests final : public QObject {
    Q_OBJECT
private slots:
    void openingAndInspectingThePageNeverReadsKeysOrSendsRequests() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.bridge->available());
        QVERIFY(f.bridge->draft().value("providerId").toString().isEmpty());
        QVERIFY(f.bridge->models().isEmpty());
        QVERIFY(!f.bridge->defaultModels().isEmpty());
        QVERIFY(!f.bridge->defaultModelCapabilities().isEmpty());
        QVERIFY(!f.bridge->configurationStatus().isEmpty());
        QVERIFY(!f.bridge->defaultThinkingChoices().isEmpty());
        QVERIFY(!f.bridge->translationThinkingEditable());
        QCOMPARE(f.credentials->reads, 0);
        QCOMPARE(f.http.requestCount(), 0);
        QVERIFY(!f.bridge->refreshModels(false));
        QVERIFY(!f.bridge->refreshModels(true));
        QCOMPARE(f.credentials->reads, 0);
        QCOMPARE(f.http.requestCount(), 0);
    }

    void modelRefreshPreservesUnsavedProviderModelAndStyle() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.selectProvider());
        QCOMPARE(f.bridge->draft().value("modelId").toString(), QString("deepseek-flash"));
        QVERIFY(f.bridge->edit("translationStylePrompt", "pending style"));
        QVERIFY(f.bridge->storeKey("fake-draft-key"));
        f.http.enqueue(Catalog);
        QVERIFY(f.bridge->refreshModels(true));
        QVERIFY(!f.bridge->refreshModels(true));
        QTRY_VERIFY(!f.bridge->busy());
        QCOMPARE(f.http.requestCount(), 1);
        QCOMPARE(f.bridge->draft().value("providerId").toString(), QString("deepseek"));
        QCOMPARE(f.bridge->draft().value("modelId").toString(), QString("deepseek-flash"));
        QCOMPARE(f.bridge->draft().value("translationStylePrompt").toString(), QString("pending style"));
        QVERIFY(f.runtime->configuration().defaults.providerId.isEmpty());
        QVERIFY(f.bridge->save());
        QCOMPARE(f.runtime->configuration().defaults.providerId, QString("deepseek"));
        QCOMPARE(f.runtime->configuration().translationStylePrompt, QString("pending style"));
        QVERIFY(f.runtime->validateConfiguration(f.runtime->translationConfiguration()).isEmpty());
        QVERIFY(f.http.request(0).startsWith("GET /models"));
        QVERIFY(f.http.requestBody(0).isEmpty());
        QVERIFY(f.http.request(0).contains("Authorization: Bearer fake-draft-key"));
    }

    void catalogChangesMergeUntouchedSettingsWithoutDiscardingEdits() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.configure());
        QVERIFY(f.bridge->edit("translationStylePrompt", "pending style"));
        auto configuration = f.runtime->configuration();
        configuration.defaults.thinkingEnabled = true;
        configuration.defaults.reasoningEffort = "high";
        QVERIFY(f.runtime->saveConfiguration(configuration));
        QCOMPARE(f.bridge->draft().value("translationStylePrompt").toString(), QString("pending style"));
        QVERIFY(f.bridge->draft().value("thinkingEnabled").toBool());
        QCOMPARE(f.bridge->draft().value("reasoningEffort").toString(), QString("high"));
        f.bridge->resetDraft();
        QCOMPARE(f.bridge->draft().value("translationStylePrompt").toString(), configuration.translationStylePrompt);
    }

    void translationOverrideCopiesGlobalOnlyWhenTheOverrideIsEmpty() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.configure());
        QVERIFY(f.bridge->setThinkingEffort(false, "high"));
        QVERIFY(f.bridge->edit("translationUsesDefaults", false));
        QCOMPARE(f.bridge->draft().value("translationProviderId").toString(), QString("deepseek"));
        QCOMPARE(f.bridge->draft().value("translationModelId").toString(), QString("deepseek-flash"));
        QVERIFY(f.bridge->draft().value("translationThinkingEnabled").toBool());
        QCOMPARE(f.bridge->draft().value("translationReasoningEffort").toString(), QString("high"));
        QVERIFY(f.bridge->save());
        QVERIFY(f.runtime->validateConfiguration(f.runtime->translationConfiguration()).isEmpty());
        QVERIFY(f.bridge->edit("translationModelId", "explicit-model"));
        QVERIFY(f.bridge->save());
        QVERIFY(f.bridge->edit("translationUsesDefaults", true));
        QVERIFY(f.bridge->save());
        QVERIFY(f.bridge->edit("modelId", "different-global-model"));
        QVERIFY(f.bridge->edit("translationUsesDefaults", false));
        QCOMPARE(f.bridge->draft().value("translationModelId").toString(), QString("explicit-model"));
        QCOMPARE(f.http.requestCount(), 0);
    }

    void thinkingModeIsOneValidSelectionAndInvalidSavedEffortStaysExplicit() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.configure());
        QVERIFY(f.bridge->defaultThinkingEditable());
        QVERIFY(hasChoice(f.bridge->defaultThinkingChoices(), "off"));
        QVERIFY(hasChoice(f.bridge->defaultThinkingChoices(), "max"));
        QVERIFY(f.bridge->setThinkingEffort(false, "high"));
        QVERIFY(f.runtime->configuration().defaults.thinkingEnabled);
        QCOMPARE(f.runtime->configuration().defaults.reasoningEffort, QString("high"));
        QVERIFY(!f.bridge->setThinkingEffort(false, "medium"));
        QCOMPARE(f.runtime->configuration().defaults.reasoningEffort, QString("high"));
        QVERIFY(f.bridge->setThinkingEffort(false, "off"));
        QVERIFY(!f.runtime->configuration().defaults.thinkingEnabled);
        QVERIFY(f.runtime->configuration().defaults.reasoningEffort.isEmpty());
        auto configuration = f.runtime->configuration();
        configuration.defaults.thinkingEnabled = true;
        configuration.defaults.reasoningEffort = "medium";
        QVERIFY(f.runtime->saveConfiguration(configuration));
        QVERIFY(hasChoice(f.bridge->defaultThinkingChoices(), "invalid", "medium"));
        QVERIFY(f.bridge->configurationStatus().contains("思考强度"));
        QVERIFY(f.bridge->setThinkingEffort(false, "off"));
        QCOMPARE(f.http.requestCount(), 0);
    }

    void missingOrUnconfirmedModelsRemainVisibleAndExplainWhyTheyCannotRun() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.selectProvider());
        QVERIFY(hasChoice(f.bridge->defaultModels(), "deepseek-flash", "尚未验证"));
        auto unknown = confirmedFlash();
        unknown.id = "new-model";
        unknown.displayName = "Unconfirmed provider model";
        unknown.protocol = {};
        QVERIFY(f.runtime->acceptModelCatalog("deepseek", {unknown}));
        QVERIFY(f.bridge->edit("modelId", unknown.id));
        QVERIFY(!f.bridge->defaultThinkingEditable());
        QVERIFY(f.bridge->configurationStatus().contains("协议"));
        QVERIFY(f.bridge->defaultModelCapabilities().contains("暂不可调用"));
        QVERIFY(f.runtime->acceptModelCatalog("deepseek", {}));
        QVERIFY(hasChoice(f.bridge->defaultModels(), unknown.id, "当前不可用"));
        QCOMPARE(f.bridge->draft().value("modelId").toString(), unknown.id);
        QCOMPARE(f.credentials->reads, 0);
    }

    void replacingCredentialsCancelsCatalogAndRejectsLateResults() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.configure());
        f.http.setHold(true);
        f.http.enqueue(Catalog);
        QVERIFY(f.bridge->refreshModels(true));
        QTRY_COMPARE(f.http.heldCount(), 1);
        const auto oldTask = f.runtime->requests()->latestModelTask("deepseek");
        QVERIFY(f.bridge->storeKey("fake-replacement-key"));
        QVERIFY(!f.bridge->busy());
        auto late = confirmedFlash();
        late.id = "late-model";
        f.runtime->deepSeekProvider()->modelsFinished(1, {late}, {});
        QCoreApplication::processEvents();
        QVERIFY(!f.runtime->model("deepseek", "late-model"));
        QVERIFY(!f.runtime->requests()->isPending(oldTask));
        QCOMPARE(f.credentials->values.value("deepseek"), QString("fake-replacement-key"));
    }

    void providerChangeAndFrontendDestructionCancelOwnedCatalogRequest() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.configure());
        f.http.setHold(true);
        f.http.enqueue(Catalog);
        QVERIFY(f.bridge->refreshModels(true));
        QTRY_COMPARE(f.http.requestCount(), 1);
        const auto first = f.runtime->requests()->latestModelTask("deepseek");
        QVERIFY(f.bridge->edit("providerId", ""));
        QVERIFY(!f.bridge->busy());
        QVERIFY(!f.runtime->requests()->isPending(first));
        QVERIFY(f.selectProvider());
        QVERIFY(f.bridge->refreshModels(true));
        QTRY_COMPARE(f.http.requestCount(), 2);
        const auto second = f.runtime->requests()->latestModelTask("deepseek");
        f.bridge.reset();
        QVERIFY(!f.runtime->requests()->isPending(second));
        auto late = confirmedFlash();
        late.id = "late-after-close";
        f.runtime->deepSeekProvider()->modelsFinished(2, {late}, {});
        QCoreApplication::processEvents();
        QVERIFY(!f.runtime->model("deepseek", late.id));
    }

    void runtimeDestructionDisablesTheBridgeWithoutDanglingAccess() {
        Fixture f;
        QVERIFY(f.ok);
        f.runtime.reset();
        QVERIFY(!f.bridge->available());
        QVERIFY(!f.bridge->busy());
        QVERIFY(f.bridge->models().isEmpty());
        QVERIFY(!f.bridge->edit("providerId", "deepseek"));
        QVERIFY(!f.bridge->save());
        QVERIFY(!f.bridge->refreshModels(true));
        QVERIFY(!f.bridge->storeKey("fake-after-close-key"));
        QVERIFY(!f.bridge->removeKey());
        f.bridge->cancelModels();
    }

    void credentialsNeverAppearInPropertiesSettingsOrErrorStatus() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.selectProvider());
        QVERIFY(f.bridge->storeKey("  fake-parity-secret  "));
        QCOMPARE(f.credentials->values.value("deepseek"), QString("fake-parity-secret"));
        QVERIFY(f.bridge->save());
        QVERIFY(f.bridge->metaObject()->indexOfProperty("apiKey") < 0);
        QVERIFY(!QJsonDocument(QJsonObject::fromVariantMap(f.bridge->draft())).toJson().contains("fake-parity-secret"));
        QVERIFY(!f.configurationBytes().contains("fake-parity-secret"));
        QVERIFY(!f.bridge->status().contains("fake-parity-secret"));
        QVERIFY(!f.bridge->storeKey("fake\ninvalid"));
        QCOMPARE(f.credentials->values.value("deepseek"), QString("fake-parity-secret"));
        f.credentials->failReads = true;
        QVERIFY(!f.bridge->refreshModels(true));
        QVERIFY(!f.bridge->status().contains("private store error sentinel"));
        QCOMPARE(f.http.requestCount(), 0);
        QVERIFY(f.bridge->removeKey());
        QVERIFY(!f.credentials->values.contains("deepseek"));
    }

    void refreshedSavedConfigurationDrivesAnActualLoopbackCompletion() {
        Fixture f;
        QVERIFY(f.ok);
        QVERIFY(f.selectProvider());
        QVERIFY(f.bridge->save());
        QVERIFY(f.bridge->storeKey("fake-request-key"));
        f.http.enqueue(Catalog);
        QVERIFY(f.bridge->refreshModels(true));
        QTRY_VERIFY(!f.bridge->busy());
        QVERIFY(f.bridge->setThinkingEffort(false, "high"));
        QVERIFY(f.bridge->edit("translationUsesDefaults", false));
        QVERIFY(f.bridge->save());
        QVERIFY(f.bridge->setThinkingEffort(true, "max"));
        const auto configuration = f.runtime->translationConfiguration();
        QVERIFY(f.runtime->validateConfiguration(configuration).isEmpty());
        QSignalSpy finished(f.runtime->requests(), &AiRequestCoordinator::completionFinished);
        f.http.enqueue(completion("{\"translations\":[]}"));
        ChatCompletionRequest request{configuration.modelId,
            QJsonArray{QJsonObject{{"role", "user"}, {"content", "local mock json request"}}},
            configuration.thinkingEnabled, configuration.reasoningEffort, 1024};
        f.runtime->requests()->complete(configuration.providerId, request);
        QTRY_COMPARE(finished.size(), 1);
        const auto result = qvariant_cast<ChatCompletionResult>(finished.first().at(2));
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error.message));
        QCOMPARE(result.content, QString("{\"translations\":[]}"));
        QCOMPARE(f.http.requestCount(), 2);
        const auto body = QJsonDocument::fromJson(f.http.requestBody(1)).object();
        QCOMPARE(body.value("model").toString(), QString("deepseek-flash"));
        QCOMPARE(body.value("thinking").toObject().value("type").toString(), QString("enabled"));
        QCOMPARE(body.value("reasoning_effort").toString(), QString("max"));
        QVERIFY(f.http.request(1).startsWith("POST /chat/completions"));
        QVERIFY(f.bridge->restoreTranslationStyle());
        QCOMPARE(f.runtime->configuration().translationStylePrompt, defaultTranslationStylePrompt());
        QCOMPARE(f.http.requestCount(), 2);
    }
};

QTEST_GUILESS_MAIN(QmlAiParityTests)
#include "qml-ai-parity-tests.moc"
