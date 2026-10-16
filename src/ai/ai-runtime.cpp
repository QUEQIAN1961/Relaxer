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

void ModelRegistry::restoreUnverified(const QList<ModelInfo> &models) {
    m_models.clear();
    for (auto model : models) {
        model.availability = ModelAvailability::Unverified;
        if (!find(model.providerId, model.id))
            m_models.append(model);
    }
}

void ModelRegistry::applyLiveCatalog(const QString &providerId, const QList<ModelInfo> &models) {
    QList<ModelInfo> retained;
    for (auto model : m_models) {
        if (model.providerId != providerId) {
            retained.append(model);
            continue;
        }
        const bool present = std::any_of(models.cbegin(), models.cend(), [&](const ModelInfo &live) {
            return live.id == model.id;
        });
        if (!present) {
            model.availability = ModelAvailability::Unavailable;
            retained.append(model);
        }
    }
    for (auto model : models) {
        model.providerId = providerId;
        model.availability = ModelAvailability::Available;
        const auto existing = std::find_if(retained.begin(), retained.end(), [&](const ModelInfo &candidate) {
            return candidate.providerId == providerId && candidate.id == model.id;
        });
        if (existing == retained.end())
            retained.append(model);
        else
            *existing = model;
    }
    m_models = retained;
}

QList<ModelInfo> ModelRegistry::models() const { return m_models; }

QList<ModelInfo> ModelRegistry::models(const QString &providerId) const {
    QList<ModelInfo> result;
    for (const auto &model : m_models)
        if (providerId.isEmpty() || model.providerId == providerId)
            result.append(model);
    return result;
}

const ModelInfo *ModelRegistry::find(const QString &providerId, const QString &modelId) const {
    for (const auto &model : m_models)
        if (model.providerId == providerId && model.id == modelId)
            return &model;
    return nullptr;
}

AiRuntime::AiRuntime(Library *library, QObject *parent) : AiRuntime(library, {}, parent) {}

AiRuntime::AiRuntime(Library *library, std::unique_ptr<CredentialStore> credentials, QObject *parent)
    : QObject(parent), m_settings(library) {
    initialize(std::move(credentials));
}

AiRuntime::AiRuntime(Library *library, QUrl deepSeekBaseUrl, std::unique_ptr<CredentialStore> credentials,
                     QObject *parent)
    : QObject(parent), m_settings(library) {
    initialize(std::move(credentials), std::move(deepSeekBaseUrl));
}

void AiRuntime::initialize(std::unique_ptr<CredentialStore> credentials, QUrl deepSeekBaseUrl) {
    m_configuration = configurationFromJson(m_settings.get(QString::fromLatin1(ConfigKey)).toObject());
    m_models.restoreUnverified(m_configuration.cachedModels);
    if (credentials) {
        m_ownedCredentials = std::move(credentials);
        m_credentials = m_ownedCredentials.get();
    } else {
        m_ownedCredentials = std::make_unique<WindowsCredentialStore>();
        m_credentials = m_ownedCredentials.get();
    }
    m_deepSeek = new DeepSeekProvider(std::move(deepSeekBaseUrl), this);
    m_providers.add(m_deepSeek);
    m_requests = new AiRequestCoordinator(&m_providers, this);
    connect(m_requests, &AiRequestCoordinator::modelsFinished, this,
            [this](quint64, const QString &providerId, const QList<ModelInfo> &models, const ProviderError &error) {
        if (error.isEmpty())
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
    if (configuration.modelId.isEmpty())
        return "请先选择模型";
    const auto *selected = m_models.find(configuration.providerId, configuration.modelId);
    if (!selected)
        return "请先刷新模型列表并确认模型可用";
    if (selected->availability != ModelAvailability::Available)
        return "所选模型尚未验证可用，请刷新模型列表";
    if (!selected->protocol.supportsStructuredChat())
        return "此模型的 Chat Completions、JSON 输出或思考控制协议尚未确认";
    if (!selected->capabilities.inputModalities.contains("text") ||
        !selected->capabilities.outputModalities.contains("text"))
        return "所选模型未确认支持文本输入和输出";
    if (selected->capabilities.contextWindow <= 0 || selected->capabilities.maxOutputTokens <= 0)
        return "所选模型的上下文或输出上限尚未确认";
    if (configuration.thinkingEnabled) {
        if (!selected->capabilities.reasoningMetadataKnown ||
            !selected->capabilities.reasoningEfforts.contains(configuration.reasoningEffort))
            return "当前模型不支持所选思考强度，请重新选择";
    }
    return {};
}

const ModelInfo *AiRuntime::model(const QString &providerId, const QString &modelId) const {
    return m_models.find(providerId, modelId);
}

QList<ModelInfo> AiRuntime::models(const QString &providerId) const { return m_models.models(providerId); }

bool AiRuntime::hasCredential(const QString &providerId, QString *error) {
    if (!m_credentials) {
        if (error)
            *error = "安全凭据存储不可用";
        return false;
    }
    auto result = m_credentials->read(providerId);
    if (error)
        *error = result.error;
    const bool found = result.ok && result.found && !result.secret.isEmpty();
    result.secret.fill(QChar());
    return found;
}

QString AiRuntime::readCredential(const QString &providerId, QString *error) {
    if (!m_credentials) {
        if (error)
            *error = "安全凭据存储不可用";
        return {};
    }
    auto result = m_credentials->read(providerId);
    if (error)
        *error = result.error;
    return result.ok && result.found ? result.secret : QString();
}

void AiRuntime::acceptModelCatalog(const QString &providerId, const QList<ModelInfo> &models) {
    m_models.applyLiveCatalog(providerId, models);
    m_configuration.cachedModels = m_models.models();
    saveConfiguration(m_configuration);
    emit modelsChanged();
}
} // namespace relaxer::ai
