#include "window.h"
#include <QtTest>
#include <QAbstractButton>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTimer>
using namespace relaxer;
class SettingsAppearanceWidgetsTests:public QObject {
    Q_OBJECT
    static void answer(QMessageBox::ButtonRole role){
        QTimer::singleShot(0,[role]{
            auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if(!box)return;
            for(auto *button:box->buttons())if(box->buttonRole(button)==role){button->click();return;}
        });
    }
    static void emitSystemChange(){
        const auto scheme=qApp->styleHints()->colorScheme();
        QVERIFY(QMetaObject::invokeMethod(qApp->styleHints(),"colorSchemeChanged",Qt::DirectConnection,Q_ARG(Qt::ColorScheme,scheme)));
    }
  private slots:
    void initTestCase(){qApp->setStyle("Fusion");}
    void presetSaveAndRestart_data(){QTest::addColumn<int>("mode");for(int mode:{0,1,2})QTest::newRow(qPrintable(QString::number(mode)))<<mode;}
    void presetSaveAndRestart(){
        QFETCH(int,mode);QTemporaryDir dir;const auto root=dir.filePath("data");
        {Library library(root);QVERIFY(library.open());int previews=0,restores=0;AppearancePage page(&library,[&](const QJsonObject &){++previews;},[&]{++restores;});page.show();auto *button=page.findChild<QPushButton *>("appearanceMode"+QString::number(mode));QVERIFY(button);button->click();QSignalSpy finished(&page,&AppearancePage::finished);page.findChild<QPushButton *>("appearanceSavePreset")->click();QCOMPARE(library.setting("theme").toInt(),mode);QCOMPARE(finished.size(),1);QCOMPARE(restores,1);QCOMPARE(previews,mode==2?0:1);}
        {Library library(root);QVERIFY(library.open());QCOMPARE(library.setting("theme").toInt(),mode);AppearancePage page(&library,[](const QJsonObject &){},[]{});QVERIFY(page.findChild<QPushButton *>("appearanceMode"+QString::number(mode))->isChecked());}
    }
    void dirtyPreviewCancelThenDiscard(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());AppearanceStore store(&library);QVERIFY(store.save(AppearanceStore::defaults(),{}));const auto before=store.state();QJsonObject preview;int restored=0;
        AppearancePage page(&library,[&](const QJsonObject &value){preview=value;},[&]{++restored;});page.show();page.findChild<QPushButton *>("appearanceMode1")->click();QCOMPARE(preview["preset"].toInt(),1);QCOMPARE(store.state(),before);QCOMPARE(library.setting("theme").toInt(),3);
        answer(QMessageBox::RejectRole);QVERIFY(!page.confirmLeave());QCOMPARE(restored,0);QCOMPARE(store.state(),before);
        answer(QMessageBox::DestructiveRole);QVERIFY(page.confirmLeave());QCOMPARE(restored,1);QCOMPARE(library.setting("theme").toInt(),3);QCOMPARE(store.state(),before);
    }
    void dirtyPreviewSaveFromPrompt(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());int restored=0;AppearancePage page(&library,[](const QJsonObject &){},[&]{++restored;});page.show();page.findChild<QPushButton *>("appearanceMode1")->click();answer(QMessageBox::AcceptRole);QVERIFY(page.confirmLeave());QCOMPARE(library.setting("theme").toInt(),1);QCOMPARE(restored,1);
    }
    void systemSignalUsesCurrentWindowsPreferenceAndRespectsPreview(){
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QCOMPARE(library.setting("theme").toInt(),2);
        Window window(&library);window.playback()->media()->setAudioOutput(nullptr);window.show();window.showSettings();QTest::qWait(40);
#ifdef Q_OS_WIN
        QSettings system("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
        const QColor expected=system.value("AppsUseLightTheme",1).toInt()==0?QColor("#19191e"):QColor("#ffffff");
#else
        const auto expected=qApp->palette().color(QPalette::Window);
#endif
        QCOMPARE(qApp->palette().color(QPalette::Window),expected);auto modified=qApp->palette();modified.setColor(QPalette::Window,QColor("#123456"));qApp->setPalette(modified);emitSystemChange();QTRY_COMPARE(qApp->palette().color(QPalette::Window),expected);QCOMPARE(library.setting("theme").toInt(),2);
        window.findChild<QPushButton *>("editAppearance")->click();auto *page=window.findChild<AppearancePage *>();QVERIFY(page);page->findChild<QPushButton *>("appearanceMode0")->click();QCOMPARE(qApp->palette().color(QPalette::Window),QColor("#ffffff"));emitSystemChange();QTest::qWait(30);QCOMPARE(qApp->palette().color(QPalette::Window),QColor("#ffffff"));QCOMPARE(library.setting("theme").toInt(),2);
        answer(QMessageBox::DestructiveRole);page->findChild<QPushButton *>("appearanceBack")->click();QTRY_COMPARE(qApp->palette().color(QPalette::Window),expected);
        if(const auto directory=qEnvironmentVariable("RELAXER_QA_DIR");!directory.isEmpty()){QVERIFY(QDir().mkpath(directory));QVERIFY(window.grab().save(directory+"/b4-system-restored.png"));}
    }
};
QTEST_MAIN(SettingsAppearanceWidgetsTests)
#include "settings-appearance-widgets-tests.moc"
