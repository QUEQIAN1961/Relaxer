#include "ai-bridge.h"

namespace relaxer {
namespace {
QVariantMap modelChoice(const ai::ModelInfo &model) {
    return {{"id", model.id}, {"providerId", model.providerId},
            {"label", model.displayName + " · " + ai::modelAvailabilityText(model.availability)},
            {"availability", int(model.availability)},
            {"reasoningEfforts", model.capabilities.reasoningEfforts}};
}
}

AiBridge::AiBridge(ai::AiRuntime *runtime, QObject *parent)
    : QObject(parent), m_runtime(runtime) {
    resetDraft();
    if (!runtime) return;
    connect(runtime, &ai::AiRuntime::configurationChanged, this, &AiBridge::synchronizeConfiguration);
    connect(runtime, &ai::AiRuntime::modelCatalogFinished, this,
            [this](quint64 task, const QString &, const QList<ai::ModelInfo> &models,
                   const ai::ProviderError &error) {
        if (task != m_request) return;
        m_request = 0;
        if (!error.isEmpty()) {
            status(error.message);
            return;
        }
        status(models.isEmpty() ? QStringLiteral("连接成功，但供应商未返回可用模型")
                                : QString("连接成功，已读取 %1 个模型").arg(models.size()));
    });
    connect(runtime, &QObject::destroyed, this, [this] {
        m_runtime = nullptr;
        m_request = 0;
        status("AI 服务已关闭");
    });
}

AiBridge::~AiBridge() { cancelModels(); }

void AiBridge::status(const QString &text) {
    m_status = text;
    emit changed();
}

QVariantMap AiBridge::configurationDraft(const ai::AiServiceConfiguration &configuration) {
    return {{"providerId", configuration.defaults.providerId},
            {"modelId", configuration.defaults.modelId},
            {"thinkingEnabled", configuration.defaults.thinkingEnabled},
            {"reasoningEffort", configuration.defaults.reasoningEffort},
            {"translationUsesDefaults", configuration.translationUsesDefaults},
            {"translationProviderId", configuration.translationOverride.providerId},
            {"translationModelId", configuration.translationOverride.modelId},
            {"translationThinkingEnabled", configuration.translationOverride.thinkingEnabled},
            {"translationReasoningEffort", configuration.translationOverride.reasoningEffort},
            {"targetLanguage", configuration.targetLanguage},
            {"translationStylePrompt", configuration.translationStylePrompt}};
}

void AiBridge::resetDraft() {
    if (!m_runtime) return;
    m_draft = configurationDraft(m_runtime->configuration());
    m_savedDraft = m_draft;
    emit changed();
}

void AiBridge::synchronizeConfiguration() {
    if (!m_runtime) return;
    const auto saved = configurationDraft(m_runtime->configuration());
    // Catalog refresh persists metadata and emits configurationChanged too.
    // Adopt unrelated persisted changes while preserving pending user edits.
    for (auto it = saved.cbegin(); it != saved.cend(); ++it)
        if (m_draft.value(it.key()) == m_savedDraft.value(it.key()))
            m_draft[it.key()] = it.value();
    m_savedDraft = saved;
    emit changed();
}

QVariantList AiBridge::models() const {
    QVariantList result;
    if (m_runtime)
        for (const auto &model : m_runtime->models()) result.append(modelChoice(model));
    return result;
}

ai::AiTaskConfiguration AiBridge::selection(bool translation) const {
    if (translation)
        return {m_draft.value("translationProviderId").toString(),
                m_draft.value("translationModelId").toString(),
                m_draft.value("translationThinkingEnabled").toBool(),
                m_draft.value("translationReasoningEffort").toString()};
    return {m_draft.value("providerId").toString(), m_draft.value("modelId").toString(),
            m_draft.value("thinkingEnabled").toBool(), m_draft.value("reasoningEffort").toString()};
}

QVariantList AiBridge::modelChoices(bool translation) const {
    const auto selected = selection(translation);
    QVariantList result{QVariantMap{{"id", QString()},
                                   {"label", selected.providerId.isEmpty()
                                        ? QStringLiteral("先选择供应商") : QStringLiteral("请选择模型")}}};
    if (!m_runtime || selected.providerId.isEmpty()) return result;
    bool containsSelected = selected.modelId.isEmpty();
    for (const auto &model : m_runtime->models(selected.providerId)) {
        result.append(modelChoice(model));
        containsSelected = containsSelected || model.id == selected.modelId;
    }
    if (!containsSelected)
        result.append(QVariantMap{{"id", selected.modelId}, {"providerId", selected.providerId},
                                  {"label", selected.modelId + " · 尚未验证"},
                                  {"availability", int(ai::ModelAvailability::Unverified)}});
    return result;
}

QVariantList AiBridge::defaultModels() const { return modelChoices(false); }
QVariantList AiBridge::translationModels() const { return modelChoices(true); }

QStringList AiBridge::efforts(bool translation) const {
    const auto selected = selection(translation);
    const auto *model = m_runtime ? m_runtime->model(selected.providerId, selected.modelId) : nullptr;
    return model && model->capabilities.reasoningMetadataKnown
        ? model->capabilities.reasoningEfforts : QStringList();
}

QStringList AiBridge::defaultEfforts() const { return efforts(false); }
QStringList AiBridge::translationEfforts() const { return efforts(true); }

bool AiBridge::thinkingEditable(bool translation) const {
    const auto selected = selection(translation);
    const auto *model = m_runtime ? m_runtime->model(selected.providerId, selected.modelId) : nullptr;
    return model && model->availability == ai::ModelAvailability::Available &&
           model->protocol.supportsStructuredChat() && model->capabilities.reasoningMetadataKnown &&
           !model->capabilities.reasoningEfforts.isEmpty() &&
           (!translation || !m_draft.value("translationUsesDefaults").toBool());
}

bool AiBridge::defaultThinkingEditable() const { return thinkingEditable(false); }
bool AiBridge::translationThinkingEditable() const { return thinkingEditable(true); }

QVariantList AiBridge::thinkingChoices(bool translation) const {
    const auto selected = selection(translation);
    const auto supported = efforts(translation);
    QVariantList result;
    if (selected.thinkingEnabled && !supported.contains(selected.reasoningEffort))
        result.append(QVariantMap{{"id", "invalid"},
                                  {"label", QString("需重新选择（%1）").arg(selected.reasoningEffort)}});
    result.append(QVariantMap{{"id", "off"}, {"label", "关闭思考"}});
    for (const auto &effort : supported)
        result.append(QVariantMap{{"id", effort}, {"label", effort.toUpper()}});
    return result;
}

QVariantList AiBridge::defaultThinkingChoices() const { return thinkingChoices(false); }
QVariantList AiBridge::translationThinkingChoices() const { return thinkingChoices(true); }

QString AiBridge::defaultModelCapabilities() const {
    const auto selected = selection(false);
    const auto *model = m_runtime ? m_runtime->model(selected.providerId, selected.modelId) : nullptr;
    if (!model) return "模型能力未知。刷新列表后才能验证并使用模型。";
    QStringList parts{ai::modelAvailabilityText(model->availability)};
    if (model->capabilities.contextWindow > 0)
        parts << QString("上下文 %1 tokens").arg(model->capabilities.contextWindow);
    if (model->capabilities.maxOutputTokens > 0)
        parts << QString("输出上限 %1 tokens").arg(model->capabilities.maxOutputTokens);
    parts << (model->capabilities.reasoningMetadataKnown
                  ? QString("思考强度 %1").arg(model->capabilities.reasoningEfforts.join(" / "))
                  : QStringLiteral("思考能力未知"));
    parts << (model->protocol.supportsStructuredChat()
                  ? QStringLiteral("请求协议已确认") : QStringLiteral("请求协议未确认，暂不可调用"));
    return parts.join(" · ");
}

QString AiBridge::configurationStatus() const {
    if (!m_runtime) return "AI 服务不可用";
    const auto selected = selection(!m_draft.value("translationUsesDefaults").toBool());
    if (selected.providerId.isEmpty()) return "请先选择供应商并保存 API Key。";
    const auto error = ai::validateModelSelection(m_runtime->model(selected.providerId, selected.modelId),
        selected.providerId, selected.modelId, selected.thinkingEnabled, selected.reasoningEffort);
    return error.isEmpty()
        ? QString("歌词翻译使用 %1；开始翻译前仍会检查已保存凭据。整首歌词仅在确认后发送。").arg(selected.modelId)
        : error;
}

bool AiBridge::edit(const QString &key, const QVariant &value) {
    if (!m_runtime || !m_draft.contains(key)) return false;
    if ((key == "providerId" || key == "translationProviderId") && m_draft.value(key) != value)
        cancelModels();
    m_draft[key] = value;
    if (key == "providerId" && value.toString() == ai::DeepSeekProviderId) {
        if (m_draft.value("modelId").toString().isEmpty()) m_draft["modelId"] = "deepseek-flash";
        if (m_draft.value("translationUsesDefaults").toBool() &&
            m_draft.value("translationProviderId").toString().isEmpty()) {
            m_draft["translationProviderId"] = m_draft.value("providerId");
            m_draft["translationModelId"] = m_draft.value("modelId");
            m_draft["translationThinkingEnabled"] = m_draft.value("thinkingEnabled");
            m_draft["translationReasoningEffort"] = m_draft.value("reasoningEffort");
        }
    }
    if (key == "translationProviderId" && value.toString() == ai::DeepSeekProviderId &&
        m_draft.value("translationModelId").toString().isEmpty())
        m_draft["translationModelId"] = "deepseek-flash";
    if (key == "translationUsesDefaults" && !value.toBool() &&
        m_draft.value("translationProviderId").toString().isEmpty()) {
        m_draft["translationProviderId"] = m_draft.value("providerId");
        m_draft["translationModelId"] = m_draft.value("modelId");
        m_draft["translationThinkingEnabled"] = m_draft.value("thinkingEnabled");
        m_draft["translationReasoningEffort"] = m_draft.value("reasoningEffort");
    }
    emit changed();
    return true;
}

bool AiBridge::save() {
    if (!m_runtime) return false;
    if (m_draft.value("targetLanguage").toString() != "zh-CN") {
        status("当前整首 AI 翻译仅支持简体中文目标语言");
        return false;
    }
    for (const auto &key : {"providerId", "translationProviderId"}) {
        const auto id = m_draft.value(key).toString();
        if (!id.isEmpty() && id != ai::DeepSeekProviderId) {
            status("供应商无效");
            return false;
        }
    }
    auto configuration = m_runtime->configuration();
    configuration.defaults = selection(false);
    configuration.translationUsesDefaults = m_draft.value("translationUsesDefaults").toBool();
    configuration.translationOverride = selection(true);
    configuration.targetLanguage = "zh-CN";
    configuration.translationStylePrompt = m_draft.value("translationStylePrompt").toString();
    QString error;
    if (!m_runtime->saveConfiguration(configuration, &error)) {
        status(error);
        return false;
    }
    resetDraft();
    status("AI 设置已保存");
    return true;
}

bool AiBridge::setThinkingEffort(bool translation, const QString &effort) {
    if (!m_runtime || (effort != "off" &&
        (!thinkingEditable(translation) || !efforts(translation).contains(effort)))) {
        status("当前模型不支持所选思考强度，请重新选择");
        return false;
    }
    m_draft[translation ? "translationThinkingEnabled" : "thinkingEnabled"] = effort != "off";
    m_draft[translation ? "translationReasoningEffort" : "reasoningEffort"] =
        effort == "off" ? QString() : effort;
    return save();
}

bool AiBridge::restoreTranslationStyle() {
    return edit("translationStylePrompt", ai::defaultTranslationStylePrompt()) && save();
}

bool AiBridge::storeKey(QString secret) {
    QString error;
    auto normalized = ai::normalizedApiKey(secret, &error);
    secret.fill(QChar());
    if (normalized.isEmpty()) {
        status("API Key 为空或包含非法字符");
        return false;
    }
    cancelModels();
    const bool ok = m_runtime && m_runtime->credentials() &&
        m_runtime->credentials()->store(ai::DeepSeekProviderId, normalized, &error);
    normalized.fill(QChar());
    if (ok) m_credentialStatus = "已配置（完整 Key 已隐藏）";
    status(ok ? "凭据已保存" : "凭据保存失败，请检查 Windows 凭据管理器");
    return ok;
}

bool AiBridge::removeKey() {
    cancelModels();
    QString error;
    const bool ok = m_runtime && m_runtime->credentials() &&
        m_runtime->credentials()->remove(ai::DeepSeekProviderId, &error);
    if (ok) m_credentialStatus = "未配置";
    status(ok ? "凭据已移除" : "凭据移除失败，请检查 Windows 凭据管理器");
    return ok;
}

bool AiBridge::checkCredential() {
    QString error;
    const bool found = m_runtime && m_runtime->hasCredential(ai::DeepSeekProviderId, &error);
    m_credentialStatus = found ? "已配置（完整 Key 已隐藏）"
                              : error.isEmpty() ? "未配置" : "无法访问凭据存储";
    emit changed();
    return found;
}

bool AiBridge::refreshModels(bool confirmed) {
    if (!confirmed) {
        status("请先确认连接 DeepSeek 并读取模型目录");
        return false;
    }
    if (!m_runtime || busy()) return false;
    if (m_draft.value("providerId").toString() != ai::DeepSeekProviderId) {
        status("请先明确选择供应商");
        return false;
    }
    if (!checkCredential()) {
        status("请先安全保存有效的供应商 API Key");
        return false;
    }
    m_request = m_runtime->requests()->fetchModels(ai::DeepSeekProviderId);
    status("正在读取模型目录…");
    return m_request != 0;
}

void AiBridge::cancelModels() {
    const auto request = m_request;
    m_request = 0;
    if (request && m_runtime) m_runtime->requests()->cancel(request);
    if (request) emit changed();
}

} // namespace relaxer
