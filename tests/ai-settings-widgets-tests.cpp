#include "ai/ai-settings-page.h"
#include "library.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDir>
#include <QFileInfo>
#include <QAbstractItemView>

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
    bool allowsMockRequests() const override { return true; }
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
        QVERIFY(runtime.credentials()->store("deepseek", "fake-widget-key", nullptr));
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
        QCOMPARE(runtime.credentials()->read("deepseek").secret, QString("fake-only-ui-key"));
        QVERIFY(!library.setting("aiServiceConfigV1").toString().contains("fake-only-ui-key"));
        QVERIFY(!library.setting("aiServiceConfigV1").toObject().value("defaults").toString().contains("fake-only-ui-key"));

        key->setText("replacement-fake-key");
        save->click();
        QCOMPARE(runtime.credentials()->read("deepseek").secret, QString("replacement-fake-key"));
        remove->click();
        QVERIFY(!runtime.hasCredential("deepseek"));
    }

    void modelNamesCapabilitiesAndIncompatibleEffortStayExplicit() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        QVERIFY(runtime.credentials()->store("deepseek", "fake-widget-key", nullptr));
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

    void modelActionsRequireStoredCredential() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        AiSettingsPage page(&runtime);
        page.findChild<QComboBox *>("aiProviderSelection")->setCurrentIndex(1);
        QVERIFY(!page.findChild<QPushButton *>("aiProviderVerify")->isEnabled());
        QVERIFY(!page.findChild<QPushButton *>("aiModelRefresh")->isEnabled());
    }

    void stylePromptEditsAreCoalesced() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        AiSettingsPage page(&runtime);
        auto *style = page.findChild<QPlainTextEdit *>("aiTranslationStylePrompt");
        QSignalSpy changed(&runtime, &AiRuntime::configurationChanged);
        style->setPlainText("draft one");
        style->setPlainText("draft two");
        style->setPlainText("final draft");
        QCOMPARE(changed.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 1000);
        QCOMPARE(runtime.configuration().translationStylePrompt, QString("final draft"));
        style->setPlainText("pending while another setting changes");
        auto config = runtime.configuration();
        config.defaults.providerId = "deepseek";
        QVERIFY(runtime.saveConfiguration(config));
        QCOMPARE(style->toPlainText(), QString("pending while another setting changes"));
        QTRY_COMPARE(runtime.configuration().translationStylePrompt, style->toPlainText());
        QCOMPARE(runtime.configuration().defaults.providerId, QString("deepseek"));
        page.show();
        style->setPlainText("flush on page exit");
        page.hide();
        QCOMPARE(runtime.configuration().translationStylePrompt, QString("flush on page exit"));
    }

    void translationThinkingEnableStateCombinesAllPrerequisites() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        runtime.acceptModelCatalog("deepseek", {liveModel("deepseek-flash", "Flash", false)});
        auto configuration = runtime.configuration();
        configuration.defaults = {"deepseek", "deepseek-flash", false, {}};
        configuration.translationOverride = configuration.defaults;
        configuration.translationUsesDefaults = false;
        QVERIFY(runtime.saveConfiguration(configuration));
        AiSettingsPage page(&runtime);
        auto *thinking = page.findChild<QComboBox *>("aiTranslationThinking");
        auto *useGlobal = page.findChild<QCheckBox *>("aiTranslationUseGlobal");
        QVERIFY(thinking && useGlobal);
        QVERIFY(!thinking->isEnabled());
        runtime.acceptModelCatalog("deepseek", {liveModel("deepseek-flash", "Flash", true)});
        QVERIFY(thinking->isEnabled());
        useGlobal->setChecked(true);
        QVERIFY(!thinking->isEnabled());
        useGlobal->setChecked(false);
        QVERIFY(thinking->isEnabled());
        runtime.acceptModelCatalog("deepseek", {});
        QVERIFY(!thinking->isEnabled());
        configuration = runtime.configuration();
        configuration.translationOverride.providerId.clear();
        QVERIFY(runtime.saveConfiguration(configuration));
        QVERIFY(!thinking->isEnabled());
    }

    void leavingSettingsCancelsRefreshAndIgnoresLateCatalog() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QUrl url(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{url}, std::make_unique<MemoryCredentials>());
        QVERIFY(runtime.credentials()->store("deepseek", "fake-page-key", nullptr));
        auto configuration = runtime.configuration();
        configuration.defaults = {"deepseek", "deepseek-flash", false, {}};
        QVERIFY(runtime.saveConfiguration(configuration));
        const auto saved = library.setting("aiServiceConfigV1");
        AiSettingsPage page(&runtime);
        page.show();
        auto *refresh = page.findChild<QPushButton *>("aiModelRefresh");
        refresh->click();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *socket = server.nextPendingConnection();
        QTRY_VERIFY(socket->bytesAvailable() > 0);
        const auto taskId = runtime.requests()->latestModelTask("deepseek");
        QVERIFY(runtime.requests()->isPending(taskId));
        page.hide();
        QVERIFY(!runtime.requests()->isPending(taskId));
        runtime.deepSeekProvider()->modelsFinished(1, {liveModel("deepseek-flash", "late model")}, {});
        QTest::qWait(40);
        QCOMPARE(library.setting("aiServiceConfigV1"), saved);
        QVERIFY(runtime.models("deepseek").isEmpty());
        page.show();
        QVERIFY(refresh->isEnabled());
    }

    void settingsCanOutliveRuntimeWithPendingRefresh() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QUrl url(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        auto runtime = std::make_unique<AiRuntime>(&library, DeepSeekProvider::MockEndpoint{url}, std::make_unique<MemoryCredentials>());
        QVERIFY(runtime->credentials()->store("deepseek", "fake-destruction-key", nullptr));
        auto configuration = runtime->configuration();
        configuration.defaults.providerId = "deepseek";
        QVERIFY(runtime->saveConfiguration(configuration));
        AiSettingsPage page(runtime.get());
        page.show();
        page.findChild<QPushButton *>("aiModelRefresh")->click();
        QTRY_VERIFY(server.hasPendingConnections());
        runtime.reset();
        QVERIFY(!page.isEnabled());
        page.findChild<QComboBox *>("aiProviderSelection")->setCurrentIndex(1);
        page.findChild<QLineEdit *>("aiCredentialInput")->setText("fake-after-destruction");
        page.hide();
    }

    void catalogPersistenceFailureIsShownToTheUser() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, &server, [socket] {
                auto buffer = socket->property("buffer").toByteArray() + socket->readAll();
                socket->setProperty("buffer", buffer);
                if (!buffer.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
                socket->setProperty("sent", true);
                const QByteArray body = R"({"object":"list","data":[{"id":"deepseek-v4-pro","name":"Pro"}]})";
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        const QUrl url(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        AiRuntime runtime(&library, DeepSeekProvider::MockEndpoint{url}, std::make_unique<MemoryCredentials>());
        QVERIFY(runtime.credentials()->store("deepseek", "fake-persistence-key", nullptr));
        QVERIFY(runtime.acceptModelCatalog("deepseek", {liveModel("deepseek-flash", "Flash")}));
        auto config = runtime.configuration();
        config.defaults = {"deepseek", "deepseek-flash", false, {}};
        QVERIFY(runtime.saveConfiguration(config));
        const auto saved = library.setting("aiServiceConfigV1");
        AiSettingsPage page(&runtime);
        page.show();
        QSqlDatabase database;
        for (const auto &name : QSqlDatabase::connectionNames()) {
            auto candidate = QSqlDatabase::database(name);
            if (QFileInfo(candidate.databaseName()).absoluteFilePath() ==
                QFileInfo(library.absolute("relaxer.sqlite")).absoluteFilePath()) database = candidate;
        }
        QVERIFY(database.isValid());
        QSqlQuery query(database);
        QVERIFY(query.exec("PRAGMA query_only = ON"));
        auto *refresh = page.findChild<QPushButton *>("aiModelRefresh");
        refresh->click();
        QTRY_VERIFY(page.findChild<QLabel *>("aiProviderStatus")->text().contains(QStringLiteral("无法保存")));
        QVERIFY(refresh->isEnabled());
        QVERIFY(!runtime.model("deepseek", "deepseek-v4-pro"));
        QCOMPARE(runtime.model("deepseek", "deepseek-flash")->availability, ModelAvailability::Available);
        QCOMPARE(library.setting("aiServiceConfigV1"), saved);
        QVERIFY(query.exec("PRAGMA query_only = OFF"));
    }

    void aiPagesRenderWithLongNamesAndThemes() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        QVERIFY(runtime.credentials()->store("deepseek", "fake-render-key", nullptr));
        const auto name = QStringLiteral("DeepSeek Flash 长模型名称 / ") + QString(100, QLatin1Char('W'));
        QVERIFY(runtime.acceptModelCatalog("deepseek", {liveModel("deepseek-flash", name)}));
        auto config = runtime.configuration();
        config.defaults = {"deepseek", "deepseek-flash", false, {}};
        config.translationOverride = config.defaults;
        config.translationUsesDefaults = false;
        QVERIFY(runtime.saveConfiguration(config));
        AiSettingsPage page(&runtime);
        page.resize(740, 620);
        page.show();
        auto *model = page.findChild<QComboBox *>("aiDefaultModel");
        QCOMPARE(model->view()->textElideMode(), Qt::ElideRight);
        QVERIFY(model->toolTip().contains(name));
        const auto evidence = qEnvironmentVariable("RELAXER_AI_SCREENSHOT_DIR");
        if (!evidence.isEmpty()) QVERIFY(QDir().mkpath(evidence));
        for (const bool dark : {false, true}) {
            QPalette palette = page.palette();
            palette.setColor(QPalette::Window, dark ? QColor("#202124") : QColor("#f4f5f7"));
            palette.setColor(QPalette::Base, dark ? QColor("#292a2d") : QColor("#ffffff"));
            palette.setColor(QPalette::WindowText, dark ? Qt::white : Qt::black);
            palette.setColor(QPalette::Text, dark ? Qt::white : Qt::black);
            page.setPalette(palette);
            page.setAutoFillBackground(true);
            for (const auto &subpage : {QString("home"), QString("translation")}) {
                page.setSubpageForTest(subpage);
                QCoreApplication::processEvents();
                const auto image = page.grab();
                QVERIFY(!image.isNull());
                if (!evidence.isEmpty())
                    QVERIFY(image.save(QDir(evidence).filePath(subpage + (dark ? "-dark.png" : "-light.png"))));
            }
        }
    }

    void settingsNavigationAndNarrowScrollableLayoutWork() {
        QTemporaryDir directory;
        Library library(directory.filePath("library"));
        QVERIFY(library.open());
        AiRuntime runtime(&library, std::make_unique<MemoryCredentials>());
        QVERIFY(runtime.credentials()->store("deepseek", "fake-widget-key", nullptr));
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
