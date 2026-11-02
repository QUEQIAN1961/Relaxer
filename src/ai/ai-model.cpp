#include "ai-model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QRegularExpression>
#include <QVersionNumber>
#include <algorithm>
#include <cmath>
#include <limits>

namespace relaxer::ai {
namespace {
QStringList strings(const QJsonValue &value) {
    QStringList result;
    if (!value.isArray())
        return result;
    for (const auto &item : value.toArray()) {
        if (item.isString() && !item.toString().trimmed().isEmpty())
            result.append(item.toString());
    }
    return result;
}

QStringList supportedEfforts(const QJsonValue &value) {
    static const QSet<QString> supported{"low", "high", "max"};
    QStringList result;
    for (const auto &effort : strings(value)) {
        const auto normalized = effort.trimmed().toLower();
        if (supported.contains(normalized) && !result.contains(normalized))
            result.append(normalized);
    }
    return result;
}

void applyConfirmedDeepSeekProtocol(ModelInfo &model) {
    // These model IDs have a confirmed Chat Completions, JSON Output, max_tokens,
    // and explicit thinking-control contract. Newly listed IDs stay unverified.
    if (model.id == "deepseek-flash" || model.id == "deepseek-v4-pro") {
        model.protocol.chatCompletions = true;
        model.protocol.jsonObjectOutput = true;
        model.protocol.maxTokens = true;
        model.protocol.explicitThinkingControl = true;
    }
}

QJsonArray toArray(const QStringList &values) {
    QJsonArray result;
    for (const auto &value : values)
        result.append(value);
    return result;
}
} // namespace

bool isSupportedProvider(const QString &id) {
    return id == DeepSeekProviderId || id == KimiProviderId || id == QwenProviderId;
}

QString defaultModelId(const QString &id) {
    if (id == DeepSeekProviderId) return QStringLiteral("deepseek-flash");
    if (id == KimiProviderId) return QStringLiteral("kimi-k3");
    if (id == QwenProviderId) return QStringLiteral("qwen3.8-max");
    return {};
}

qint64 nonNegativeJsonInteger(const QJsonValue &value, bool *valid) {
    const double number = value.toDouble(-1);
    const bool ok = value.isDouble() && std::isfinite(number) && number >= 0 &&
                    number < double(std::numeric_limits<qint64>::max()) && std::floor(number) == number;
    if (valid) *valid = ok;
    return ok ? qint64(number) : 0;
}

bool usesKimiK3Protocol(const QString &modelId) {
    return modelId == "kimi-k3" || modelId == "kimi-k3.1-pro" || modelId == "kimi-k3.1-pro-highspeed";
}

ModelCatalogResult parseDeepSeekModelCatalog(const QByteArray &body) {
    ModelCatalogResult result;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = "模型列表响应不是有效 JSON 对象";
        return result;
    }
    const auto root = document.object();
    if (root.value("object").toString() != "list" || !root.value("data").isArray()) {
        result.error = "模型列表响应结构不完整";
        return result;
    }

    QSet<QString> seen;
    for (const auto &value : root.value("data").toArray()) {
        if (!value.isObject())
            continue;
        const auto item = value.toObject();
        const auto id = item.value("id").toString().trimmed();
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);

        ModelInfo model;
        model.providerId = QString::fromLatin1(DeepSeekProviderId);
        model.id = id;
        model.displayName = item.value("name").toString().trimmed();
        if (model.displayName.isEmpty())
            model.displayName = id;
        model.availability = ModelAvailability::Available;
        applyConfirmedDeepSeekProtocol(model);

        const auto capabilities = item;
        model.capabilities.inputModalities = strings(capabilities.value("input_modalities"));
        model.capabilities.outputModalities = strings(capabilities.value("output_modalities"));
        if (capabilities.value("context_window").isDouble())
            model.capabilities.contextWindow = nonNegativeJsonInteger(capabilities.value("context_window"));
        if (capabilities.value("max_output_tokens").isDouble())
            model.capabilities.maxOutputTokens = nonNegativeJsonInteger(capabilities.value("max_output_tokens"));

        const auto effort = capabilities.value("effort");
        model.capabilities.reasoningMetadataKnown = effort.isObject();
        if (effort.isObject()) {
            const auto effortObject = effort.toObject();
            model.capabilities.reasoningEfforts = supportedEfforts(effortObject.value("supported_levels"));
            model.capabilities.defaultReasoningEffort = effortObject.value("default_level").toString().trimmed().toLower();
            if (!model.capabilities.reasoningEfforts.contains(model.capabilities.defaultReasoningEffort))
                model.capabilities.defaultReasoningEffort.clear();
        }
        result.models.append(model);
    }
    result.ok = true;
    return result;
}

ModelCatalogResult parseCompatibleModelCatalog(const QByteArray &body, const QString &providerId) {
    if (providerId == QwenProviderId) {
        const auto root = QJsonDocument::fromJson(body).object();
        if (root.contains("output") || root.contains("success")) {
            ModelCatalogResult catalog;
            const auto output = root.value("output").toObject();
            const auto entries = output.value("models").toArray();
            bool validTotal = false;
            const auto total = nonNegativeJsonInteger(output.value("total"), &validTotal);
            const auto page = nonNegativeJsonInteger(output.value("page_no"));
            const auto size = nonNegativeJsonInteger(output.value("page_size"));
            if (!root.value("success").toBool() || !output.value("models").isArray() ||
                !validTotal || page < 1 || size < 1 || page > 100000 || size > 1000 ||
                total < (page - 1) * size || entries.size() != qMin(size, total - (page - 1) * size)) {
                catalog.error = QStringLiteral("百炼模型能力目录响应不完整");
                return catalog;
            }
            for (const auto &entry : entries) {
                const auto item = entry.toObject();
                if (item.value("provider").toString() != "qwen") continue;
                ModelInfo model;
                model.providerId = providerId;
                model.id = item.value("model").toString().trimmed();
                if (model.id.isEmpty()) continue;
                // Keep API IDs visible; translated marketing names are often identical across snapshots.
                model.displayName = model.id;
                model.publishedTime = item.value("published_time").toString();
                model.availability = ModelAvailability::Available;
                const auto modality = item.value("inference_metadata").toObject();
                for (const auto &value : strings(modality.value("request_modality"))) model.capabilities.inputModalities << value.toLower();
                for (const auto &value : strings(modality.value("response_modality"))) model.capabilities.outputModalities << value.toLower();
                const auto limits = item.value("model_info").toObject();
                model.capabilities.contextWindow = nonNegativeJsonInteger(limits.value("context_window"));
                model.capabilities.maxOutputTokens = nonNegativeJsonInteger(limits.value("max_output_tokens"));
                if (strings(item.value("capabilities")).contains("TG") &&
                    strings(item.value("features")).contains("structured-outputs")) {
                    model.protocol = {true, true, true, true};
                    model.capabilities.reasoningMetadataKnown = true;
                }
                catalog.models.append(model);
            }
            catalog.nextPage = page * size < total ? int(page + 1) : 0;
            catalog.ok = true;
            return catalog;
        }
    }
    auto result = parseDeepSeekModelCatalog(body);
    for (auto &model : result.models) {
        model.providerId = providerId;
        model.protocol = {};
        bool supported = false;
        qint64 context = 32768, output = 8192;
        if (providerId == KimiProviderId) {
            supported = usesKimiK3Protocol(model.id) || model.id == "kimi-k2.5" || model.id == "kimi-k2-0905-preview" || model.id == "kimi-k2-0711-preview"
                     || model.id == "moonshot-v1-8k" || model.id == "moonshot-v1-32k" || model.id == "moonshot-v1-128k";
            if (model.id == "kimi-k2.5") { context = 262144; output = 32768; }
            else if (model.id == "kimi-k2-0905-preview") context = 262144;
            else if (model.id == "kimi-k2-0711-preview") context = 131072;
            else if (model.id == "moonshot-v1-8k") { context = 8192; output = 4096; }
            else if (model.id == "moonshot-v1-128k") context = 131072;
        } else if (providerId == QwenProviderId) {
            supported = model.id == "qwen-plus" || model.id == "qwen-turbo" || model.id == "qwen-max";
        }
        if (supported) {
            model.protocol = {true, true, true, true};
            model.capabilities.inputModalities = {"text"};
            model.capabilities.outputModalities = {"text"};
            // Conservative request budgets when /models omits token limits.
            if (!model.capabilities.contextWindow) model.capabilities.contextWindow = context;
            if (!model.capabilities.maxOutputTokens) model.capabilities.maxOutputTokens = output;
            // Older compatible models translate without thinking; K3 below
            // requires reasoning instead.
            model.capabilities.reasoningMetadataKnown = true;
            model.capabilities.reasoningEfforts.clear();
            model.capabilities.defaultReasoningEffort.clear();
            // K3 uses reasoning_effort, cannot disable thinking, and supports
            // max_completion_tokens. See platform.kimi.com/docs/api/models-overview.
            if (providerId == KimiProviderId && usesKimiK3Protocol(model.id)) {
                model.capabilities.thinkingRequired = true;
                model.capabilities.reasoningEfforts = {"low", "high", "max"};
                model.capabilities.defaultReasoningEffort = "max";
                model.capabilities.contextWindow = 1048576;
                // Use the documented default completion budget, leaving room
                // for lyrics instead of reserving the entire 1M context as output.
                model.capabilities.maxOutputTokens = 131072;
            }
        }
    }
    return result;
}

QList<ModelInfo> recommendedQwenModels(const QList<ModelInfo> &models) {
    static const QRegularExpression family(QStringLiteral("^qwen([1-9][0-9]*(?:\\.[0-9]+)*)-(max|plus|flash)(?:-([0-9]{4}|[0-9]{4}-[0-9]{2}-[0-9]{2}))?$"));
    QList<ModelInfo> aliases, snapshots;
    for (const auto &model : models) {
        if (model.providerId != QwenProviderId || model.availability == ModelAvailability::Unavailable ||
            !model.protocol.supportsStructuredChat() || !model.capabilities.inputModalities.contains("text") ||
            !model.capabilities.outputModalities.contains("text") || model.capabilities.contextWindow <= 0 ||
            model.capabilities.maxOutputTokens <= 0) continue;
        const auto match = family.match(model.id);
        if (!match.hasMatch()) continue;
        (match.captured(3).isEmpty() ? aliases : snapshots).append(model);
    }
    std::sort(aliases.begin(), aliases.end(), [&](const ModelInfo &a, const ModelInfo &b) {
        return QVersionNumber::compare(QVersionNumber::fromString(family.match(a.id).captured(1)),
                                       QVersionNumber::fromString(family.match(b.id).captured(1))) > 0;
    });
    QList<ModelInfo> result;
    QString maxVersion;
    for (const auto &model : aliases) {
        const auto match = family.match(model.id);
        if (match.captured(2) == "max") {
            if (maxVersion.isEmpty()) {
                result.append(model); maxVersion = match.captured(1);
                QList<ModelInfo> currentSnapshots;
                for (const auto &snapshot : snapshots)
                    if (snapshot.id.startsWith("qwen" + maxVersion + "-max-")) currentSnapshots.append(snapshot);
                std::sort(currentSnapshots.begin(), currentSnapshots.end(), [](const ModelInfo &a, const ModelInfo &b) {
                    if (a.publishedTime != b.publishedTime) return a.publishedTime > b.publishedTime;
                    // Prefer the short API ID when the same snapshot has two aliases.
                    if (a.id.size() != b.id.size()) return a.id.size() < b.id.size();
                    return a.id > b.id;
                });
                if (!currentSnapshots.isEmpty()) result.append(currentSnapshots.first());
            } else { result.append(model); break; }
        }
    }
    for (const auto &tier : {QStringLiteral("plus"), QStringLiteral("flash")})
        for (const auto &model : aliases)
            if (family.match(model.id).captured(2) == tier) { result.append(model); break; }
    return result;
}

QJsonObject modelToJson(const ModelInfo &model) {
    const auto &cap = model.capabilities;
    return {{"providerId", model.providerId},
            {"id", model.id},
            {"displayName", model.displayName},
            {"publishedTime", model.publishedTime},
            {"inputModalities", toArray(cap.inputModalities)},
            {"outputModalities", toArray(cap.outputModalities)},
            {"reasoningEfforts", toArray(cap.reasoningEfforts)},
            {"defaultReasoningEffort", cap.defaultReasoningEffort},
            {"contextWindow", double(cap.contextWindow)},
            {"maxOutputTokens", double(cap.maxOutputTokens)},
            {"reasoningMetadataKnown", cap.reasoningMetadataKnown},
            {"thinkingRequired", cap.thinkingRequired},
            {"protocol", QJsonObject{{"chatCompletions", model.protocol.chatCompletions},
                                     {"jsonObjectOutput", model.protocol.jsonObjectOutput},
                                     {"maxTokens", model.protocol.maxTokens},
                                     {"explicitThinkingControl", model.protocol.explicitThinkingControl}}}};
}

ModelInfo modelFromJson(const QJsonObject &object) {
    ModelInfo model;
    model.providerId = object.value("providerId").toString();
    model.id = object.value("id").toString();
    model.displayName = object.value("displayName").toString();
    model.publishedTime = object.value("publishedTime").toString();
    if (model.displayName.isEmpty())
        model.displayName = model.id;
    model.capabilities.inputModalities = strings(object.value("inputModalities"));
    model.capabilities.outputModalities = strings(object.value("outputModalities"));
    model.capabilities.reasoningEfforts = supportedEfforts(object.value("reasoningEfforts"));
    model.capabilities.defaultReasoningEffort = object.value("defaultReasoningEffort").toString();
    model.capabilities.contextWindow = nonNegativeJsonInteger(object.value("contextWindow"));
    model.capabilities.maxOutputTokens = nonNegativeJsonInteger(object.value("maxOutputTokens"));
    model.capabilities.reasoningMetadataKnown = object.value("reasoningMetadataKnown").toBool();
    model.capabilities.thinkingRequired = object.value("thinkingRequired").toBool();
    const auto protocol = object.value("protocol").toObject();
    model.protocol.chatCompletions = protocol.value("chatCompletions").toBool();
    model.protocol.jsonObjectOutput = protocol.value("jsonObjectOutput").toBool();
    model.protocol.maxTokens = protocol.value("maxTokens").toBool();
    model.protocol.explicitThinkingControl = protocol.value("explicitThinkingControl").toBool();
    model.availability = ModelAvailability::Unverified;
    return model;
}

QString modelAvailabilityText(ModelAvailability availability) {
    switch (availability) {
    case ModelAvailability::Available: return "已验证可用";
    case ModelAvailability::Unavailable: return "当前不可用";
    case ModelAvailability::Unverified: return "尚未验证";
    }
    return "尚未验证";
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


QString validateModelSelection(const ModelInfo *selected, const QString &providerId, const QString &modelId,
                               bool thinkingEnabled, const QString &effort) {
    if (modelId.isEmpty())
        return QStringLiteral("请先选择模型");
    if (!selected || selected->providerId != providerId || selected->id != modelId)
        return QStringLiteral("请先刷新模型列表并确认模型可用");
    if (selected->availability != ModelAvailability::Available)
        return QStringLiteral("所选模型尚未验证可用，请刷新模型列表");
    if (!selected->protocol.supportsStructuredChat())
        return QStringLiteral("此模型的 Chat Completions、JSON 输出或思考控制协议尚未确认");
    if (!selected->capabilities.inputModalities.contains("text") ||
        !selected->capabilities.outputModalities.contains("text"))
        return QStringLiteral("所选模型未确认支持文本输入和输出");
    if (selected->capabilities.contextWindow <= 0 || selected->capabilities.maxOutputTokens <= 0)
        return QStringLiteral("所选模型的上下文或输出上限尚未确认");
    if (selected->capabilities.thinkingRequired && !thinkingEnabled)
        return QStringLiteral("此模型始终开启思考，请选择 LOW、HIGH 或 MAX 思考强度");
    if (thinkingEnabled && (!selected->capabilities.reasoningMetadataKnown ||
        !selected->capabilities.reasoningEfforts.contains(effort) ||
        (effort != "low" && effort != "high" && effort != "max")))
        return QStringLiteral("当前模型不支持所选思考强度，请重新选择");
    return {};
}

} // namespace relaxer::ai
