#pragma once

#include "ai-request-coordinator.h"
#include "credential-store.h"
#include "deepseek-provider.h"
#include "../storage/settings-store.h"

#include <QHash>
#include <QObject>
#include <QUrl>
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

class AiRuntime final : public QObject {
    Q_OBJECT
  public:
    explicit AiRuntime(Library *library, QObject *parent = nullptr);
    AiRuntime(Library *library, std::unique_ptr<CredentialStore> credentials, QObject *parent = nullptr);
    AiRuntime(Library *library, DeepSeekProvider::MockEndpoint endpoint, std::unique_ptr<CredentialStore> credentials,
              QObject *parent = nullptr);

    ~AiRuntime() override;

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
    bool hasCredential(const QString &providerId, QString *error = nullptr) const;
    bool acceptModelCatalog(const QString &providerId, const QList<ModelInfo> &models, QString *error = nullptr);

  signals:
    void configurationChanged();
    void modelCatalogFinished(quint64 taskId, const QString &providerId,
                              const QList<relaxer::ai::ModelInfo> &models,
                              const relaxer::ai::ProviderError &error);

  private:
    SettingsStore m_settings;
    AiServiceConfiguration m_configuration;
    ModelRegistry m_models;
    std::unique_ptr<CredentialStore> m_ownedCredentials;
    CredentialStore *m_credentials = nullptr;
    ProviderRegistry m_providers;
    DeepSeekProvider *m_deepSeek = nullptr;
    AiRequestCoordinator *m_requests = nullptr;
    void initialize(std::unique_ptr<CredentialStore> credentials,
                    QUrl deepSeekBaseUrl = QUrl(QStringLiteral("https://api.deepseek.com")), bool mockEndpoint = false);
};

} // namespace relaxer::ai

Q_DECLARE_METATYPE(relaxer::ai::AiTaskConfiguration)
