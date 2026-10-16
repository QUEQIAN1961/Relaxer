#pragma once

#include <QJsonObject>
#include <QByteArray>
#include <QList>
#include <QStringList>

namespace relaxer::ai {

inline constexpr const char *DeepSeekProviderId = "deepseek";

enum class ModelAvailability { Unverified, Available, Unavailable };

struct ModelCapabilities {
    QStringList inputModalities;
    QStringList outputModalities;
    QStringList reasoningEfforts;
    QString defaultReasoningEffort;
    qint64 contextWindow = 0;
    qint64 maxOutputTokens = 0;
    bool reasoningMetadataKnown = false;
};

struct ModelProtocolCapabilities {
    bool chatCompletions = false;
    bool jsonObjectOutput = false;
    bool maxTokens = false;
    bool explicitThinkingControl = false;

    bool supportsStructuredChat() const {
        return chatCompletions && jsonObjectOutput && maxTokens && explicitThinkingControl;
    }
};

struct ModelInfo {
    QString providerId;
    QString id;
    QString displayName;
    ModelCapabilities capabilities;
    ModelProtocolCapabilities protocol;
    ModelAvailability availability = ModelAvailability::Unverified;
};

struct ModelCatalogResult {
    QList<ModelInfo> models;
    QString error;
    bool ok = false;
};

ModelCatalogResult parseDeepSeekModelCatalog(const QByteArray &body);
QJsonObject modelToJson(const ModelInfo &model);
ModelInfo modelFromJson(const QJsonObject &object);
QString modelAvailabilityText(ModelAvailability availability);

} // namespace relaxer::ai
