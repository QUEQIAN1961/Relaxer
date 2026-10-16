#include "ai-runtime.h"

#include "../library.h"

#include <QJsonArray>
#include <QSet>
#include <algorithm>

namespace relaxer::ai {
namespace {
constexpr auto ConfigKey = "aiServiceConfigV1";
constexpr auto TranslationStyle =
    "自然、准确地翻译成简体中文。结合整首歌曲理解语境、叙事、人称和情绪；妥善处理隐喻、双关、口语与重复副歌；保持相同语境下的重复译文一致。保留必要的人名、地名、品牌名和专有名词。避免机械直译，不擅自扩写、总结或添加内容。";

QJsonObject taskToJson(const AiTaskConfiguration &task) {
    return {{"providerId", task.providerId}, {"modelId", task.modelId},
            {"thinkingEnabled", task.thinkingEnabled}, {"reasoningEffort", task.reasoningEffort}};
}

AiTaskConfiguration taskFromJson(const QJsonValue &value) {
    AiTaskConfiguration task;
    if (!value.isObject())
        return task;
    const auto object = value.toObject();
    task.providerId = object.value("providerId").toString();
    task.modelId = object.value("modelId").toString();
    task.thinkingEnabled = object.value("thinkingEnabled").toBool();
    task.reasoningEffort = object.value("reasoningEffort").toString();
    return task;
}

QJsonArray modelsToJson(const QList<ModelInfo> &models) {
    QJsonArray array;
    for (const auto &model : models)
        array.append(modelToJson(model));
    return array;
}

QList<ModelInfo> modelsFromJson(const QJsonValue &value) {
    QList<ModelInfo> models;
    if (!value.isArray())
        return models;
    for (const auto &item : value.toArray()) {
        if (item.isObject()) {
            auto model = modelFromJson(item.toObject());
            if (!model.providerId.isEmpty() && !model.id.isEmpty())
                models.append(model);
        }
    }
    return models;
}
} // namespace

QString defaultTranslationStylePrompt() { return QString::fromUtf8(TranslationStyle); }

QJsonObject configurationToJson(const AiServiceConfiguration &configuration) {
    auto object = configuration.extensions;
    object["schemaVersion"] = 1;
    object["defaults"] = taskToJson(configuration.defaults);
    object["translationUsesDefaults"] = configuration.translationUsesDefaults;
    object["translationOverride"] = taskToJson(configuration.translationOverride);
    object["targetLanguage"] = configuration.targetLanguage;
    object["translationStylePrompt"] = configuration.translationStylePrompt;
    object["cachedModels"] = modelsToJson(configuration.cachedModels);
    return object;
}

AiServiceConfiguration configurationFromJson(const QJsonObject &object) {
    AiServiceConfiguration configuration;
    configuration.defaults = taskFromJson(object.value("defaults"));
    configuration.translationUsesDefaults = object.value("translationUsesDefaults").toBool(true);
    configuration.translationOverride = taskFromJson(object.value("translationOverride"));
    configuration.targetLanguage = object.value("targetLanguage").toString("zh-CN");
    configuration.translationStylePrompt = object.value("translationStylePrompt").toString(defaultTranslationStylePrompt());
    configuration.cachedModels = modelsFromJson(object.value("cachedModels"));
    configuration.extensions = object;
    for (const auto &key : {QStringLiteral("schemaVersion"), QStringLiteral("defaults"),
                            QStringLiteral("translationUsesDefaults"), QStringLiteral("translationOverride"),
                            QStringLiteral("targetLanguage"), QStringLiteral("translationStylePrompt"),
                            QStringLiteral("cachedModels")})
        configuration.extensions.remove(key);
    return configuration;
}

AiRuntime::AiRuntime(Library *library, QObject *parent) : AiRuntime(library, {}, parent) {}

AiRuntime::AiRuntime(Library *library, std::unique_ptr<CredentialStore> credentials, QObject *parent)
    : QObject(parent), m_settings(library) {
    initialize(std::move(credentials));
}

AiRuntime::AiRuntime(Library *library, DeepSeekProvider::MockEndpoint endpoint, std::unique_ptr<CredentialStore> credentials,
                     QObject *parent)
    : QObject(parent), m_settings(library) {
    initialize(std::move(credentials), std::move(endpoint.url), true);
}

AiRuntime::~AiRuntime() {
    // The coordinator borrows member registries/stores; terminate it while they live.
    delete m_requests;
    m_requests = nullptr;
}

void AiRuntime::initialize(std::unique_ptr<CredentialStore> credentials, QUrl deepSeekBaseUrl, bool mockEndpoint) {
    m_configuration = configurationFromJson(m_settings.get(QString::fromLatin1(ConfigKey)).toObject());
    m_models.restoreUnverified(m_configuration.cachedModels);
    if (credentials) {
        m_ownedCredentials = std::move(credentials);
        m_credentials = m_ownedCredentials.get();
    } else if (!mockEndpoint) {
        m_ownedCredentials = std::make_unique<WindowsCredentialStore>();
        m_credentials = m_ownedCredentials.get();
    }
    m_deepSeek = mockEndpoint ? new DeepSeekProvider(DeepSeekProvider::MockEndpoint{std::move(deepSeekBaseUrl)}, this)
                             : new DeepSeekProvider(std::move(deepSeekBaseUrl), this);
    m_providers.add(m_deepSeek);
    m_requests = new AiRequestCoordinator(&m_providers, m_credentials, &m_models, this);
    connect(m_requests, &AiRequestCoordinator::modelsFinished, this,
            [this](quint64 taskId, const QString &providerId, const QList<ModelInfo> &models, const ProviderError &error) {
        if (error.isEmpty() && taskId == m_requests->latestModelTask(providerId))
            acceptModelCatalog(providerId, models);
    });
}

bool AiRuntime::saveConfiguration(const AiServiceConfiguration &configuration, QString *error) {
    auto normalized = configuration;
    if (normalized.targetLanguage.isEmpty())
        normalized.targetLanguage = "zh-CN";
    if (normalized.translationStylePrompt.isNull())
        normalized.translationStylePrompt = defaultTranslationStylePrompt();
    normalized.cachedModels = m_models.models();
    const auto json = configurationToJson(normalized);
    if (!m_settings.set(QString::fromLatin1(ConfigKey), json)) {
        if (error)
            *error = "无法保存 AI 服务设置";
        return false;
    }
    m_configuration = configurationFromJson(json);
    m_configuration.cachedModels = normalized.cachedModels;
    if (error)
        error->clear();
    emit configurationChanged();
    return true;
}

AiTaskConfiguration AiRuntime::translationConfiguration() const {
    return m_configuration.translationUsesDefaults ? m_configuration.defaults : m_configuration.translationOverride;
}

QString AiRuntime::validateConfiguration(const AiTaskConfiguration &configuration) const {
    if (configuration.providerId.isEmpty())
        return "请先在 AI 服务设置中选择供应商";
    if (!m_providers.provider(configuration.providerId))
        return "所选 AI 供应商不可用";
    const auto error = validateModelSelection(m_models.find(configuration.providerId, configuration.modelId),
                                               configuration.providerId, configuration.modelId,
                                               configuration.thinkingEnabled, configuration.reasoningEffort);
    if (!error.isEmpty()) return error;
    QString credentialError;
    if (!hasCredential(configuration.providerId, &credentialError))
        return credentialError.isEmpty() ? QStringLiteral("请先安全保存有效的供应商 API Key") : credentialError;
    return {};
}

const ModelInfo *AiRuntime::model(const QString &providerId, const QString &modelId) const {
    return m_models.find(providerId, modelId);
}

QList<ModelInfo> AiRuntime::models(const QString &providerId) const { return m_models.models(providerId); }

bool AiRuntime::hasCredential(const QString &providerId, QString *error) const {
    if (!m_credentials || (m_deepSeek && m_deepSeek->usesMockEndpoint() && !m_credentials->allowsMockRequests())) {
        if (error)
            *error = "安全凭据存储不可用";
        return false;
    }
    auto result = m_credentials->read(providerId);
    if (error)
        *error = result.error;
    auto normalized = normalizedApiKey(result.secret);
    const bool found = result.ok && result.found && !normalized.isEmpty();
    normalized.fill(QChar());
    result.secret.fill(QChar());
    return found;
}

void AiRuntime::acceptModelCatalog(const QString &providerId, const QList<ModelInfo> &models) {
    m_models.applyLiveCatalog(providerId, models);
    m_configuration.cachedModels = m_models.models();
    saveConfiguration(m_configuration);
    emit modelsChanged();
}
} // namespace relaxer::ai
