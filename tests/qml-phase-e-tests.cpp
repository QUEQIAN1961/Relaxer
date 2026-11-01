#include "c4c5-ui-fixture.h"
#include <QApplication>
#include <QDir>
#include <QQmlContext>
#include <QQmlEngine>
#include <QWheelEvent>
using namespace c4c5_fixture;
namespace {
QRectF bounds(QQuickItem *item) { return item->mapRectToScene(QRectF(QPointF(), item->size())); }
bool click(QQuickWindow *w, QQuickItem *item) {
    if (!item || !item->isVisible() || !item->isEnabled()) return false;
    QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, bounds(item).center().toPoint()); return true;
}
bool prepare(Fixture &f) {
    if (!f.ok) return false;
    auto *w=f.shell->window(); w->showNormal(); w->setGeometry(50,50,1240,790); w->requestActivate();
    if (!QTest::qWaitForWindowExposed(w)) return false;
    f.shell->playbackBridge()->pause(); QTest::qWait(100); return true;
}
void shot(QQuickWindow *w, const QString &name) {
    const auto folder=qEnvironmentVariable("RELAXER_PHASE_E_SCREENSHOTS");
    if (folder.isEmpty()) return;
    QDir().mkpath(folder); w->grabWindow().save(QDir(folder).filePath(name+".png"));
}
}
class PhaseEUiTests final : public QObject {
    Q_OBJECT
private slots:
    void lyricsWorkspaceAnimatesAndTogglesPreserveSelection() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        const auto missing=seed(f.library,"Missing lyrics","phase-e-missing"); QVERIFY(missing.id);
        QTRY_COMPARE(f.shell->libraryModel()->count(),3);
        auto *workspace=visual(root,"lyricsWorkspace"); QVERIFY(workspace);
        QVERIFY(click(w,visual(root,"findMissingLyrics")));
        QVERIFY(workspace->isVisible()); QVERIFY(!visual(root,"contentHost")->isVisible()); QVERIFY(!visual(root,"playerBar")->isVisible());
        auto *body=qvariant_cast<QQuickItem *>(workspace->property("contentItem")); QVERIFY(body);
        const auto firstTop=bounds(body).top(); shot(w,"lyrics-entry-start");
        QTest::qWait(60);
        QVERIFY(body->opacity()>0 && body->opacity()<1); QVERIFY(bounds(body).top()>firstTop);
        const auto mask=w->grabWindow(); QVERIFY(!mask.isNull());
        auto *tokens=w->property("designTokens").value<QObject *>(); QVERIFY(tokens);
        QCOMPARE(mask.pixelColor(5,mask.height()/2),tokens->property("background").value<QColor>());
        shot(w,"lyrics-entry-middle"); QTRY_VERIFY(body->opacity()>.999);
        QCOMPARE(f.shell->lyricsSearchBridge()->targetTrackKey(),QString::number(missing.id));
        QVERIFY(f.shell->lyricsSearchBridge()->setTargetTracks({QString::number(f.a.id),QString::number(f.b.id)},false,true));
        QVERIFY(f.shell->lyricsSearchBridge()->setTargetTrack(QString::number(f.a.id)));
        auto *back=visual(root,"lyricsWorkspaceClose"); QVERIFY(back); QVERIFY(bounds(back).top()>=24);
        auto *multi=visual(root,"lyricsMultiSelect"); QVERIFY(multi); shot(w,"toggle-circle");
        QVERIFY(click(w,multi)); QVERIFY(multi->property("checked").toBool()); QTest::qWait(60);
        QVERIFY(multi->property("markProgress").toReal()>0 && multi->property("markProgress").toReal()<1); shot(w,"toggle-morph");
        QTRY_COMPARE(multi->property("markProgress").toReal(),1.); shot(w,"toggle-check");
        auto *filters=visual(root,"lyricsFilterRow"); QVERIFY(filters);
        for(auto *child:filters->childItems()) if(child->isVisible()) QVERIFY(bounds(child).right()<=bounds(filters).right()+1);
        auto *all=visual(root,"lyricsSelectAll"); QVERIFY(click(w,all));
        QCOMPARE(workspace->property("selectedTracks").toList().size(),2);
        QVERIFY(click(w,multi)); QVERIFY(!multi->property("checked").toBool());
        QVERIFY(workspace->property("selectedTracks").toList().isEmpty());
        QTRY_COMPARE(multi->property("markProgress").toReal(),0.);
        shot(w,"lyrics-workspace");
        QVERIFY(QMetaObject::invokeMethod(workspace,"close")); QTRY_VERIFY(!workspace->isVisible());
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(260);
        QVERIFY(click(w,visual(root,"uiFontSetting")));
        auto *dialog=w->findChild<QObject *>("uiFontDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool()); QTest::qWait(260); shot(w,"font-dialog");
        auto *header=qvariant_cast<QQuickItem *>(dialog->property("header")); QVERIFY(header);
        QVERIFY(!header->property("background").value<QObject *>());
        QVERIFY(QMetaObject::invokeMethod(dialog,"close")); QTest::qWait(150);
        QVERIFY(QMetaObject::invokeMethod(workspace,"open")); QTest::qWait(280);
        // Open the actual confirmation through its existing action, then cancel.
        auto *mark=visual(root,"lyricsInstrumentalButton");
        QVERIFY(click(w,mark)); QTest::qWait(280); shot(w,"instrumental-confirmation");
        QTest::keyClick(w,Qt::Key_Escape); QTest::qWait(150);
        QVERIFY(QMetaObject::invokeMethod(workspace,"close")); QTRY_VERIFY(!workspace->isVisible());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void feedbackStaysAbovePlayerAndHandsOffToAnimatedDetails() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        auto *notice=visual(root,"feedbackNotice"); auto *bar=visual(root,"playerBar"); QVERIFY(notice); QVERIFY(bar);
        f.shell->feedbackBridge()->post("Test","Error","测试通知","测试详情");
        QTest::qWait(60); const auto intro=bounds(notice); QTest::qWait(320);
        const auto settled=bounds(notice); QVERIFY(intro.x()>settled.x());
        const auto gap=w->width()-settled.right(); QVERIFY(gap>0);
        QVERIFY(qAbs(bounds(bar).top()-settled.bottom()-gap)<1);
        shot(w,"feedback-above-player");
        auto *detailsButton=visual(root,"feedbackDetailsButton"); QVERIFY(detailsButton);
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,bounds(detailsButton).center().toPoint()); QTest::qWait(150);
        QVERIFY(qAbs(notice->scale()-.96)<.001); shot(w,"feedback-pressed");
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,bounds(detailsButton).center().toPoint()); QTest::qWait(60);
        QVERIFY(notice->scale()<=1 && notice->scale()>=.959);
        auto *details=w->findChild<QObject *>("feedbackDetailsDialog"); QVERIFY(details);
        QTRY_VERIFY(details->property("opened").toBool()); QVERIFY(!notice->isVisible());
        QTest::qWait(260); shot(w,"feedback-details");
        QVERIFY(QMetaObject::invokeMethod(details,"close")); QTest::qWait(180);
        f.shell->feedbackBridge()->post("Test","Information","再次测试","关闭时滑回"); QTest::qWait(350);
        const auto before=bounds(notice).x(); QVERIFY(click(w,visual(root,"feedbackDismissButton")));
        QTest::qWait(80); QVERIFY(bounds(notice).x()>before); QVERIFY(f.shell->feedbackBridge()->visible());
        QTRY_VERIFY(!f.shell->feedbackBridge()->visible()); QTRY_VERIFY(!notice->isVisible());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void compatibleProviderSettingsAndPlaybackTransitionRemainUsable() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        auto *bridge=f.shell->aiBridge();
        for (const auto &id : {QString("kimi"),QString("qwen")}) {
            QVERIFY(bridge->edit("providerId",id)); QVERIFY(bridge->save());
            QCOMPARE(f.runtime->configuration().defaults.providerId,id);
            QCOMPARE(f.runtime->configuration().defaults.modelId,ai::defaultModelId(id));
            QVERIFY(!f.runtime->configuration().defaults.thinkingEnabled);
        }
        const auto shortTrack=seed(f.library,"Don't Forget Me","phase-e-short-title"); QVERIFY(shortTrack.id);
        QVERIFY(wave(f.library.absolute(shortTrack.file)));
        f.shell->facade()->play({shortTrack.id},shortTrack.id); f.shell->playbackBridge()->pause();
        QTRY_COMPARE(f.shell->playbackBridge()->title(),QString("Don't Forget Me"));
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics"));
        auto *flight=visual(root,"sharedTitleFlight"); QVERIFY(flight);
        for(int frame=0;frame<20;++frame) {
            QTest::qWait(25);
            if(flight->isVisible()) QVERIFY(flight->width()+.5>=flight->property("contentWidth").toReal());
            if(frame==6)shot(w,"playback-transition");
        }
        QVERIFY(w->property("lyricsOpen").toBool()); shot(w,"expanded-playback");
        QVERIFY(QMetaObject::invokeMethod(w,"closeLyrics")); QTest::qWait(450);
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(260);
        QVERIFY(click(w,visual(root,"openAiServiceSettings"))); QTest::qWait(300); shot(w,"ai-providers");
        auto *providers=visual(root,"aiProviderSelection"); QVERIFY(providers); QCOMPARE(providers->property("count").toInt(),4);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void compactSettingsStopAtContentAndAppearanceOptionsStayInside() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        QVERIFY(f.shell->settingsBridge()->setTheme(1));
        w->resize(1000,660); QTest::qWait(100);
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(280);
        auto *scroll=visual(root,"settingsScroll"); QVERIFY(scroll);
        auto *flick=qvariant_cast<QQuickItem *>(scroll->property("contentItem")); QVERIFY(flick);
        const auto end=qMax(0.,flick->property("contentHeight").toReal()-flick->height());
        flick->setProperty("contentY",end);
        const auto point=bounds(scroll).center().toPoint();
        QWheelEvent wheel(point,w->mapToGlobal(point),QPoint(),QPoint(0,-1200),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(w,&wheel); QTest::qWait(300);
        QVERIFY(flick->property("contentY").toReal()<=end+1);
        shot(w,"compact-settings-bottom");
        flick->setProperty("contentY",0); QTest::qWait(60);
        QVERIFY(click(w,visual(root,"editAppearance"))); QTest::qWait(280);
        QVERIFY(click(w,visual(root,"appearanceMode4"))); QTest::qWait(120);
        auto *options=visual(root,"appearanceImageOptions"); QVERIFY(options);
        for (auto *child:options->childItems()) if(child->isVisible()) QVERIFY(bounds(child).right()<=w->width()-20);
        shot(w,"compact-appearance-options");
        QVERIFY(click(w,visual(root,"appearanceBack"))); QTest::qWait(280); shot(w,"appearance-unsaved");
        auto *unsaved=w->findChild<QObject *>("appearanceUnsavedDialog"); QVERIFY(unsaved); QVERIFY(unsaved->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(unsaved,"reject")); QTest::qWait(150);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
};
int main(int argc,char **argv) { QApplication app(argc,argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false); PhaseEUiTests tests; return QTest::qExec(&tests,argc,argv); }
#include "qml-phase-e-tests.moc"
