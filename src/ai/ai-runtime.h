#pragma once

#include "ai-request-coordinator.h"
#include "credential-store.h"
#include "deepseek-provider.h"
#include "../storage/settings-store.h"

#include <QHash>
#include <QObject>
#include <memory>

namespace relaxer {
class Library;
}

namespace relaxer::ai {

struct AiTaskConfiguration {
    QString providerId;
    QString modelId;
    bool thinkingEnabled = false;
    QString reasoningEffort;
};

struct AiServiceConfiguration {
    AiTaskConfiguration defaults;
    bool translationUsesDefaults = true;
    AiTaskConfiguration translationOverride;
    QString targetLanguage = "zh-CN";
    QString translationStylePrompt;
    QList<ModelInfo> cachedModels;
    QJsonObject extensions;
};

QString defaultTranslationStylePrompt();
QJsonObject configurationToJson(const AiServiceConfiguration &configuration);
AiServiceConfiguration configurationFromJson(const QJsonObject &object);

class ModelRegistry final {
  public:
    void restoreUnverified(const QList<ModelInfo> &models);
    void applyLiveCatalog(const QString &providerId, const QList<ModelInfo> &models);
    QList<ModelInfo> models() const;
    QList<ModelInfo> models(const QString &providerId) const;
    const ModelInfo *find(const QString &providerId, const QString &modelId) const;
  private:
    QList<ModelInfo> m_models;
};

class AiRuntime final : public QObject {
    Q_OBJECT
  public:
    explicit AiRuntime(Library *library, QObject *parent = nullptr);
    AiRuntime(Library *library, std::unique_ptr<CredentialStore> credentials, QObject *parent = nullptr);

    const AiServiceConfiguration &configuration() const { return m_configuration; }
    bool saveConfiguration(const AiServiceConfiguration &configuration, QString *error = nullptr);
    AiTaskConfiguration translationConfiguration() const;
    QString validateConfiguration(const AiTaskConfiguration &configuration) const;
    const ModelInfo *model(const QString &providerId, const QString &modelId) const;
    QList<ModelInfo> models(const QString &providerId = {}) const;
    ProviderRegistry *providers() { return &m_providers; }
    AiRequestCoordinator *requests() { return m_requests; }
    CredentialStore *credentials() { return m_credentials; }
    DeepSeekProvider *deepSeekProvider() const { return m_deepSeek; }
    bool hasCredential(const QString &providerId, QString *error = nullptr);
    QString readCredential(const QString &providerId, QString *error = nullptr);
    void acceptModelCatalog(const QString &providerId, const QList<ModelInfo> &models);

  signals:
    void configurationChanged();
    void modelsChanged();

  private:
    SettingsStore m_settings;
    AiServiceConfiguration m_configuration;
    ModelRegistry m_models;
    std::unique_ptr<CredentialStore> m_ownedCredentials;
    CredentialStore *m_credentials = nullptr;
    ProviderRegistry m_providers;
    DeepSeekProvider *m_deepSeek = nullptr;
    AiRequestCoordinator *m_requests = nullptr;
    void initialize(std::unique_ptr<CredentialStore> credentials);
};

} // namespace relaxer::ai

Q_DECLARE_METATYPE(relaxer::ai::AiTaskConfiguration)
