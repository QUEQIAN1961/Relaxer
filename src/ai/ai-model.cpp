#include "ai-model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

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
            model.capabilities.contextWindow = qMax<qint64>(0, qint64(capabilities.value("context_window").toDouble()));
        if (capabilities.value("max_output_tokens").isDouble())
            model.capabilities.maxOutputTokens = qMax<qint64>(0, qint64(capabilities.value("max_output_tokens").toDouble()));

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

QJsonObject modelToJson(const ModelInfo &model) {
    const auto &cap = model.capabilities;
    return {{"providerId", model.providerId},
            {"id", model.id},
            {"displayName", model.displayName},
            {"inputModalities", toArray(cap.inputModalities)},
            {"outputModalities", toArray(cap.outputModalities)},
            {"reasoningEfforts", toArray(cap.reasoningEfforts)},
            {"defaultReasoningEffort", cap.defaultReasoningEffort},
            {"contextWindow", double(cap.contextWindow)},
            {"maxOutputTokens", double(cap.maxOutputTokens)},
            {"reasoningMetadataKnown", cap.reasoningMetadataKnown},
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
    if (model.displayName.isEmpty())
        model.displayName = model.id;
    model.capabilities.inputModalities = strings(object.value("inputModalities"));
    model.capabilities.outputModalities = strings(object.value("outputModalities"));
    model.capabilities.reasoningEfforts = supportedEfforts(object.value("reasoningEfforts"));
    model.capabilities.defaultReasoningEffort = object.value("defaultReasoningEffort").toString();
    model.capabilities.contextWindow = qMax<qint64>(0, qint64(object.value("contextWindow").toDouble()));
    model.capabilities.maxOutputTokens = qMax<qint64>(0, qint64(object.value("maxOutputTokens").toDouble()));
    model.capabilities.reasoningMetadataKnown = object.value("reasoningMetadataKnown").toBool();
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
} // namespace relaxer::ai
