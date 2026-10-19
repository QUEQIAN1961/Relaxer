#pragma once

#include "ai/ai-runtime.h"
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

namespace relaxer {

class AiBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY changed)
    Q_PROPERTY(QVariantList models READ models NOTIFY changed)
    Q_PROPERTY(QVariantList defaultModels READ defaultModels NOTIFY changed)
    Q_PROPERTY(QVariantList translationModels READ translationModels NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY changed)
    Q_PROPERTY(QString defaultModelCapabilities READ defaultModelCapabilities NOTIFY changed)
    Q_PROPERTY(QString configurationStatus READ configurationStatus NOTIFY changed)
    Q_PROPERTY(QStringList defaultEfforts READ defaultEfforts NOTIFY changed)
    Q_PROPERTY(QStringList translationEfforts READ translationEfforts NOTIFY changed)
    Q_PROPERTY(QVariantList defaultThinkingChoices READ defaultThinkingChoices NOTIFY changed)
    Q_PROPERTY(QVariantList translationThinkingChoices READ translationThinkingChoices NOTIFY changed)
    Q_PROPERTY(bool defaultThinkingEditable READ defaultThinkingEditable NOTIFY changed)
    Q_PROPERTY(bool translationThinkingEditable READ translationThinkingEditable NOTIFY changed)

public:
    explicit AiBridge(ai::AiRuntime *runtime, QObject *parent = nullptr);
    ~AiBridge() override;

    QVariantMap draft() const { return m_draft; }
    QVariantList models() const;
    QVariantList defaultModels() const;
    QVariantList translationModels() const;
    bool busy() const { return m_request != 0; }
    bool available() const { return !m_runtime.isNull(); }
    QString status() const { return m_status; }
    QString credentialStatus() const { return m_credentialStatus; }
    QString defaultModelCapabilities() const;
    QString configurationStatus() const;
    QStringList defaultEfforts() const;
    QStringList translationEfforts() const;
    QVariantList defaultThinkingChoices() const;
    QVariantList translationThinkingChoices() const;
    bool defaultThinkingEditable() const;
    bool translationThinkingEditable() const;

    Q_INVOKABLE void resetDraft();
    Q_INVOKABLE bool edit(const QString &key, const QVariant &value);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool setThinkingEffort(bool translation, const QString &effort);
    Q_INVOKABLE bool restoreTranslationStyle();
    Q_INVOKABLE bool storeKey(QString secret);
    Q_INVOKABLE bool removeKey();
    Q_INVOKABLE bool checkCredential();
    Q_INVOKABLE bool refreshModels(bool confirmed = false);
    Q_INVOKABLE void cancelModels();

signals:
    void changed();

private:
    QPointer<ai::AiRuntime> m_runtime;
    QVariantMap m_draft;
    QVariantMap m_savedDraft;
    quint64 m_request = 0;
    QString m_status;
    QString m_credentialStatus = "凭据尚未检查 · 保存在 Windows 凭据管理器";

    void status(const QString &text);
    void synchronizeConfiguration();
    static QVariantMap configurationDraft(const ai::AiServiceConfiguration &configuration);
    ai::AiTaskConfiguration selection(bool translation) const;
    QVariantList modelChoices(bool translation) const;
    QVariantList thinkingChoices(bool translation) const;
    QStringList efforts(bool translation) const;
    bool thinkingEditable(bool translation) const;
};

} // namespace relaxer
