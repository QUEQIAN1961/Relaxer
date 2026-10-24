#include "c4c5-ui-fixture.h"
#include <QApplication>
using namespace c4c5_fixture;
namespace {
bool click(QQuickWindow *w,const QString &name) {
    QTest::qWait(60);
    auto *item=visual(w->contentItem(),name);
    if(!item||!item->isVisible()||!item->isEnabled()) return false;
    const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();
    if(!QRect(QPoint(),w->size()).contains(point)) return false;
    QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,point); return true;
}
void closeButton(QQuickWindow *w) {
    QTest::qWait(60);
    QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPoint(w->width()-21,16));
}
QObject *dialog(QQuickWindow *w,const char *name="appearanceUnsavedDialog") {return w->findChild<QObject *>(name);}
bool opened(QObject *d) {return d && d->property("opened").toBool();}
void screenshot(QQuickWindow *w,const QString &name) {
    const auto folder=qEnvironmentVariable("RELAXER_EXIT_GUARD_EVIDENCE");
    if(!folder.isEmpty()) w->grabWindow().save(QDir(folder).filePath(name+".png"));
}
bool prepare(Fixture &f) {
    if(!f.ok) return false;
    auto *w=f.shell->window();w->showNormal();w->resize(1240,790);
    f.shell->playbackBridge()->pause();
    return QTest::qWaitForWindowExposed(w);
}
void editPage(Fixture &f) {
    f.shell->appBridge()->navigate(3);
    f.shell->window()->setProperty("settingsSection","appearance");
    f.shell->settingsBridge()->beginAppearance();
}
bool seedPreset(Fixture &f) {
    auto *s=f.shell->settingsBridge();s->beginAppearance();
    return s->setAppearanceMode(3)&&s->saveAppearance(false);
}
}
class ExitGuardTests:public QObject {
    Q_OBJECT
private slots:
    void closeDecisions_data() {
        QTest::addColumn<QString>("decision");
        QTest::newRow("cancel")<<QString("Cancel");
        QTest::newRow("discard")<<QString("Discard");
        QTest::newRow("save")<<QString("Save");
    }
    void closeDecisions() {
        QFETCH(QString,decision);Fixture f(2);QVERIFY(prepare(f));QVERIFY(seedPreset(f));editPage(f);
        auto *w=f.shell->window();auto *s=f.shell->settingsBridge();
        const auto before=f.library.setting("appearanceHistory");
        QVERIFY(s->editAppearance("accent","#123456"));const auto draft=s->appearanceDraft();
        closeButton(w);QTRY_VERIFY(opened(dialog(w)));QVERIFY(w->isVisible());QVERIFY(!f.shell->facade()->isShutdown());
        screenshot(w,"close-"+decision);
        QVERIFY(click(w,"appearanceLeave"+decision));
        if(decision=="Cancel") {
            QTRY_VERIFY(!opened(dialog(w)));QVERIFY(w->isVisible());QVERIFY(s->appearanceDirty());
            QCOMPARE(s->appearanceDraft(),draft);QCOMPARE(w->property("settingsSection").toString(),QString("appearance"));
            QCOMPARE(f.library.setting("appearanceHistory"),before);QVERIFY(!f.shell->facade()->isShutdown());
            closeButton(w);QTRY_VERIFY(opened(dialog(w)));QVERIFY(click(w,"appearanceLeaveDiscard"));
        }
        QTRY_VERIFY(!w->isVisible());QVERIFY(f.shell->facade()->isShutdown());QVERIFY(f.shell->shutdown());
        if(decision=="Save") {
            SettingsBridge reopened(&f.library);reopened.beginAppearance();
            QCOMPARE(reopened.appearanceDraft()["accent"].toString(),QString("#123456"));
        } else QCOMPARE(f.library.setting("appearanceHistory"),before);
    }
    void saveFailureRetainsDraftAndCanRetry() {
        Fixture f(2);QVERIFY(prepare(f));QVERIFY(seedPreset(f));editPage(f);
        auto *w=f.shell->window();auto *s=f.shell->settingsBridge();
        const auto before=f.library.setting("appearanceHistory");
        QVERIFY(s->setAppearanceMode(4));QVERIFY(s->editAppearance("sourceLeft",f.dir.filePath("missing.png")));
        closeButton(w);QTRY_VERIFY(opened(dialog(w)));QVERIFY(click(w,"appearanceLeaveSave"));
        QTRY_VERIFY(!dialog(w)->property("saveError").toString().isEmpty());
        QVERIFY(w->isVisible());QVERIFY(opened(dialog(w)));QVERIFY(s->appearanceDirty());
        QVERIFY(!f.shell->facade()->isShutdown());QCOMPARE(f.library.setting("appearanceHistory"),before);
        screenshot(w,"save-failure");QVERIFY(click(w,"appearanceLeaveCancel"));
        QTRY_VERIFY(!opened(dialog(w)));QVERIFY(s->appearanceDirty());
        QVERIFY(s->editAppearance("sourceLeft",QString()));QVERIFY(s->editAppearance("accent","#345678"));
        closeButton(w);QTRY_VERIFY(opened(dialog(w)));QVERIFY(click(w,"appearanceLeaveSave"));
        QTRY_VERIFY(!w->isVisible());QVERIFY(f.shell->shutdown());
        SettingsBridge reopened(&f.library);reopened.beginAppearance();
        QCOMPARE(reopened.appearanceDraft()["accent"].toString(),QString("#345678"));
    }
    void cleanClose() {
        Fixture f(2);QVERIFY(prepare(f));auto *w=f.shell->window();
        QVERIFY(!f.shell->settingsBridge()->appearanceDirty());closeButton(w);
        QTRY_VERIFY(!w->isVisible());QVERIFY(!opened(dialog(w)));QVERIFY(f.shell->facade()->isShutdown());QVERIFY(f.shell->shutdown());
    }
    void appearanceThenBackgroundGuard() {
        Fixture f(2);QVERIFY(prepare(f));QVERIFY(seedPreset(f));editPage(f);
        auto *w=f.shell->window();auto *s=f.shell->settingsBridge();auto *t=f.shell->translationBridge();
        QVERIFY(t->setTargetTrack(QString::number(f.a.id)));
        QVERIFY(!t->prepareBrowser("bing","en").isEmpty());QVERIFY(t->unfinished()); // No request is started.
        QVERIFY(s->editAppearance("accent","#456789"));closeButton(w);
        QTRY_VERIFY(opened(dialog(w)));QVERIFY(!opened(dialog(w,"closeTasksConfirmation")));
        QVERIFY(click(w,"appearanceLeaveSave"));QTRY_VERIFY(opened(dialog(w,"closeTasksConfirmation")));
        QVERIFY(!opened(dialog(w)));QVERIFY(w->isVisible());QVERIFY(!f.shell->facade()->isShutdown());
        screenshot(w,"background-guard");
        QVERIFY(QMetaObject::invokeMethod(dialog(w,"closeTasksConfirmation"),"reject"));
        QVERIFY(w->isVisible());QVERIFY(t->unfinished());QVERIFY(!s->appearanceDirty());
        closeButton(w);QTRY_VERIFY(opened(dialog(w,"closeTasksConfirmation")));QVERIFY(!opened(dialog(w)));
        QVERIFY(QMetaObject::invokeMethod(dialog(w,"closeTasksConfirmation"),"accept"));
        QTRY_VERIFY(!w->isVisible());QVERIFY(f.shell->shutdown());QVERIFY(!t->unfinished());
    }
    void overwriteExistingPresetByClick() {
        Fixture f(2);QVERIFY(prepare(f));editPage(f);auto *w=f.shell->window();auto *s=f.shell->settingsBridge();
        QVERIFY(s->setAppearanceMode(3));QVERIFY(s->editAppearance("accent","#567890"));
        QVERIFY(click(w,"appearanceNewSave"));QTRY_VERIFY(!s->appearanceDirty());QTest::qWait(250);
        const auto id=s->activeAppearanceId();QVERIFY(!id.isEmpty());const auto count=s->appearances().size();
        editPage(f);QCOMPARE(s->activeAppearanceId(),id);QVERIFY(s->editAppearance("accent","#678901"));
        screenshot(w,"before-overwrite");QVERIFY(click(w,"appearanceOverwriteSave"));QTRY_VERIFY(!s->appearanceDirty());
        QCOMPARE(s->activeAppearanceId(),id);QCOMPARE(s->appearances().size(),count);
        QTest::qWait(250);editPage(f);QCOMPARE(s->appearanceDraft()["accent"].toString(),QString("#678901"));
        SettingsBridge reopened(&f.library);reopened.beginAppearance();
        QCOMPARE(reopened.activeAppearanceId(),id);QCOMPARE(reopened.appearances().size(),count);
        QCOMPARE(reopened.appearanceDraft()["accent"].toString(),QString("#678901"));
        screenshot(w,"after-overwrite");QVERIFY(f.shell->warnings().isEmpty());
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);
    ExitGuardTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "exit-guard-tests.moc"
