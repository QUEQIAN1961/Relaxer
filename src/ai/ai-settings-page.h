#pragma once

#include "ai-runtime.h"

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;

namespace relaxer::ai {

class AiSettingsPage final : public QWidget {
    Q_OBJECT
  public:
    explicit AiSettingsPage(AiRuntime *runtime, QWidget *parent = nullptr);
    ~AiSettingsPage() override;
    void setSubpageForTest(const QString &name);
  signals:
    void backRequested();
  protected:
    void hideEvent(QHideEvent *event) override;
  private:
    QPointer<AiRuntime> m_runtime;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_home = nullptr;
    QWidget *m_translation = nullptr;
    QScrollArea *m_homeScroll = nullptr;
    QScrollArea *m_translationScroll = nullptr;
    QComboBox *m_provider = nullptr;
    QComboBox *m_model = nullptr;
    QComboBox *m_thinking = nullptr;
    QLabel *m_providerStatus = nullptr;
    QLabel *m_credentialStatus = nullptr;
    QLabel *m_modelCapabilities = nullptr;
    QLineEdit *m_keyInput = nullptr;
    QPushButton *m_saveKey = nullptr;
    QPushButton *m_deleteKey = nullptr;
    QPushButton *m_verify = nullptr;
    QPushButton *m_refresh = nullptr;
    QCheckBox *m_translationUsesDefaults = nullptr;
    QComboBox *m_translationProvider = nullptr;
    QComboBox *m_translationModel = nullptr;
    QComboBox *m_translationThinking = nullptr;
    QComboBox *m_language = nullptr;
    QPlainTextEdit *m_stylePrompt = nullptr;
    QLabel *m_translationStatus = nullptr;
    bool m_populating = false;
    quint64 m_pendingRefresh = 0;
    void buildHome();
    void buildTranslation();
    void populate();
    void updateCredentialStatus();
    void updateModels();
    void updateThinkingOptions(QComboBox *combo, const AiTaskConfiguration &selection, const ModelInfo *model, bool featureEnabled = true);
    void updateCapabilityText();
    void saveConfiguration(const AiServiceConfiguration &configuration, QLabel *status);
    void selectSubpage(bool translation);
    void setRequestState(bool running, const QString &message = {});
    void cancelRefresh();
    void requestModelList(const QString &purpose);
};

} // namespace relaxer::ai
