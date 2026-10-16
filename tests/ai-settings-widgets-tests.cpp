#include "ai/ai-settings-page.h"
#include "library.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QtTest>

using namespace relaxer;
using namespace relaxer::ai;

namespace {
class MemoryCredentials final : public CredentialStore {
  public:
    bool store(const QString &providerId, const QString &secret, QString *error = nullptr) override {
        if (providerId.isEmpty() || secret.isEmpty()) {
            if (error) *error = "invalid";
            return false;
        }
        m_values.insert(providerId, secret);
        if (error) error->clear();
        return true;
    }
    CredentialReadResult read(const QString &providerId) override {
        CredentialReadResult result;
        result.ok = true;
        result.found = m_values.contains(providerId);
        result.secret = m_values.value(providerId);
        return result;
    }
    bool remove(const QString &providerId, QString *error = nullptr) override {
        m_values.remove(providerId);
        if (error) error->clear();
        return true;
    }
    bool supported() const override { return true; }
  private:
    QHash<QString, QString> m_values;
};

ModelInfo liveModel(QString id, QString name, bool reasoning = true) {
    ModelInfo model;
    model.providerId = "deepseek";
    model.id = std::move(id);
    model.displayName = std::move(name);
    model.availability = ModelAvailability::Available;
    model.capabilities.inputModalities = {"text"};
    model.capabilities.outputModalities = {"text"};
    model.capabilities.contextWindow = 1048576;
    model.capabilities.maxOutputTokens = 393216;
    if (model.id == "deepseek-flash" || model.id == "deepseek-v4-pro") {
        model.protocol.chatCompletions = true;
        model.protocol.jsonObjectOutput = true;
        model.protocol.maxTokens = true;
        model.protocol.explicitThinkingControl = true;
    }
    model.capabilities.reasoningMetadataKnown = reasoning;
    if (reasoning) {
        model.capabilities.reasoningEfforts = {"low", "high", "max"};
        model.capabilities.defaultReasoningEffort = "high";
    }
    return model;
}
} // namespace

class AiSettingsWidgetsTests final : public QObject {
    Q_OBJECT
  private slots:
    void credentialControlsStayMaskedAndSeparateFromSettings() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        AiSettingsPage page(&runtime);
        page.resize(740, 620);
        page.show();

        auto *provider = page.findChild<QComboBox *>("aiProviderSelection");
        auto *key = page.findChild<QLineEdit *>("aiCredentialInput");
        auto *save = page.findChild<QPushButton *>("aiCredentialSave");
        auto *remove = page.findChild<QPushButton *>("aiCredentialDelete");
        QVERIFY(provider && key && save && remove);
        QCOMPARE(key->echoMode(), QLineEdit::Password);
        provider->setCurrentIndex(provider->findData("deepseek"));
        key->setText("fake-only-ui-key");
        QVERIFY(save->isEnabled());
        save->click();
        QVERIFY(key->text().isEmpty());
        QVERIFY(runtime.hasCredential("deepseek"));
        QCOMPARE(runtime.readCredential("deepseek"), QString("fake-only-ui-key"));
        QVERIFY(!library.setting("aiServiceConfigV1").toString().contains("fake-only-ui-key"));
        QVERIFY(!library.setting("aiServiceConfigV1").toObject().value("defaults").toString().contains("fake-only-ui-key"));

        key->setText("replacement-fake-key");
        save->click();
        QCOMPARE(runtime.readCredential("deepseek"), QString("replacement-fake-key"));
        remove->click();
        QVERIFY(!runtime.hasCredential("deepseek"));
    }

    void modelNamesCapabilitiesAndIncompatibleEffortStayExplicit() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        AiSettingsPage page(&runtime);
        auto *provider = page.findChild<QComboBox *>("aiProviderSelection");
        auto *model = page.findChild<QComboBox *>("aiDefaultModel");
        auto *thinking = page.findChild<QComboBox *>("aiDefaultThinking");
        QVERIFY(provider && model && thinking);
        provider->setCurrentIndex(provider->findData("deepseek"));
        runtime.acceptModelCatalog("deepseek", {liveModel("deepseek-flash", "A very long DeepSeek Flash display name"),
                                                  liveModel("deepseek-v4-pro", "DeepSeek Pro")});
        const int flashIndex = model->findData("deepseek-flash");
        QVERIFY(flashIndex > 0);
        QVERIFY(model->itemText(flashIndex).contains("DeepSeek Flash display name"));
        QCOMPARE(model->itemData(flashIndex).toString(), QString("deepseek-flash"));
        QVERIFY(model->itemData(flashIndex, Qt::ToolTipRole).toString().contains("A very long DeepSeek Flash display name"));
        QVERIFY(model->itemData(flashIndex, Qt::ToolTipRole).toString().contains("deepseek-flash"));
        QCOMPARE(thinking->count(), 4);
        QVERIFY(thinking->findData("off") >= 0);
        QVERIFY(thinking->findData("low") >= 0);
        QVERIFY(thinking->findData("high") >= 0);
        QVERIFY(thinking->findData("max") >= 0);

        thinking->setCurrentIndex(thinking->findData("high"));
        QVERIFY(runtime.configuration().defaults.thinkingEnabled);
        QCOMPARE(runtime.configuration().defaults.reasoningEffort, QString("high"));

        runtime.acceptModelCatalog("deepseek", {liveModel("deepseek-flash", "A very long DeepSeek Flash display name", false),
                                                  liveModel("text-only-unknown", "Text model with unknown effort", false)});
        const int unknownIndex = model->findData("text-only-unknown");
        QVERIFY(unknownIndex > 0);
        model->setCurrentIndex(unknownIndex);
        QCOMPARE(model->currentData().toString(), QString("text-only-unknown"));
        QVERIFY(runtime.configuration().defaults.thinkingEnabled);
        QCOMPARE(thinking->currentData().toString(), QString("invalid"));
        QVERIFY(!runtime.validateConfiguration(runtime.translationConfiguration()).isEmpty());

        model->setCurrentIndex(model->findData("deepseek-flash"));
        QCOMPARE(thinking->currentData().toString(), QString("invalid"));
        QVERIFY(!runtime.validateConfiguration(runtime.translationConfiguration()).isEmpty());
        thinking->setCurrentIndex(thinking->findData("off"));
        QVERIFY(runtime.validateConfiguration(runtime.translationConfiguration()).isEmpty());
        model->setCurrentIndex(unknownIndex);
        thinking->setCurrentIndex(thinking->findData("off"));
        QVERIFY(!runtime.validateConfiguration(runtime.translationConfiguration()).isEmpty());

        runtime.acceptModelCatalog("deepseek", {});
        QCOMPARE(runtime.configuration().defaults.modelId, QString("text-only-unknown"));
        QCOMPARE(runtime.model("deepseek", "text-only-unknown")->availability, ModelAvailability::Unavailable);
    }

    void settingsNavigationAndNarrowScrollableLayoutWork() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        AiSettingsPage page(&runtime);
        page.resize(620, 520);
        page.show();
        QTRY_VERIFY(page.findChild<QScrollArea *>("aiServiceScroll"));
        auto *scroll = page.findChild<QScrollArea *>("aiServiceScroll");
        QVERIFY(scroll->widgetResizable());
        auto *provider = page.findChild<QComboBox *>("aiProviderSelection");
        QVERIFY(provider);
        const QRect providerRect(provider->mapTo(&page, QPoint()), provider->size());
        QVERIFY(page.rect().intersects(providerRect));
        page.setSubpageForTest("translation");
        auto *language = page.findChild<QComboBox *>("aiTranslationLanguage");
        auto *style = page.findChild<QPlainTextEdit *>("aiTranslationStylePrompt");
        auto *useGlobal = page.findChild<QCheckBox *>("aiTranslationUseGlobal");
        QVERIFY(language && style && useGlobal);
        QCOMPARE(language->currentData().toString(), QString("zh-CN"));
        QVERIFY(style->toPlainText().contains("简体中文"));
        QVERIFY(page.findChild<QScrollArea *>("aiTranslationSettingsScroll")->widgetResizable());
        useGlobal->setChecked(false);
        QVERIFY(!runtime.configuration().translationUsesDefaults);
        auto *back = page.findChild<QPushButton *>("aiTranslationBack");
        QVERIFY(back);
        back->click();
        QVERIFY(page.findChild<QWidget *>("aiServiceHome")->isVisible());
    }
};

QTEST_MAIN(AiSettingsWidgetsTests)
#include "ai-settings-widgets-tests.moc"
