#include "c4c5-ui-fixture.h"
#include "qwen-model-fixture.h"
#include <QApplication>
#include <QDir>
#include <QLineF>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QWheelEvent>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif
using namespace c4c5_fixture;
namespace {
class PopupHeightProbe final : public QObject {
    Q_OBJECT
public:
    explicit PopupHeightProbe(QObject *popup) : m_popup(popup) {}
    void reset() { m_samples.clear(); }
    bool animated() const {
        qreal first=-1;
        bool moved=false;
        for(const auto &sample:m_samples) {
            const auto h=sample.value("height").toReal(), full=sample.value("full").toReal();
            if(h<=.01 || h>=full-.01)continue;
            if(sample.value("scale").toReal()!=1 || sample.value("opacity").toReal()!=1)return false;
            if(qAbs(sample.value("width").toReal()-m_popup->property("width").toReal())>.01)return false;
            if(first<0)first=h;
            else if(qAbs(h-first)>.1)moved=true;
        }
        return moved;
    }
    QString diagnostic() const {
        QStringList values;
        for(const auto &sample:m_samples)
            values.append(QString("%1/%2").arg(sample.value("height").toReal()).arg(sample.value("full").toReal()));
        return "popup height samples: "+values.join(", ");
    }
public slots:
    void sample() {
        m_samples.append({{"height",m_popup->property("height")},{"full",m_popup->property("expandedHeight")},
                          {"width",m_popup->property("width")},{"scale",m_popup->property("scale")},
                          {"opacity",m_popup->property("opacity")}});
    }
private:
    QObject *m_popup;
    QList<QVariantMap> m_samples;
};
QRectF bounds(QQuickItem *item) { return item->mapRectToScene(QRectF(QPointF(), item->size())); }
bool click(QQuickWindow *w, QQuickItem *item) {
    if (!item || !item->isVisible() || !item->isEnabled()) return false;
    QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, bounds(item).center().toPoint()); return true;
}
bool prepare(Fixture &f) {
    if (!f.ok) return false;
    auto *w=f.shell->window(); w->showNormal(); w->setGeometry(50,50,1240,790); w->requestActivate();
    if (!QTest::qWaitForWindowExposed(w)) return false;
    if (!QTest::qWaitFor([&] { return f.shell->facade()->playing(); })) return false;
    f.shell->playbackBridge()->pause();
    return QTest::qWaitFor([&] { return !f.shell->facade()->playing(); });
}
void shot(QQuickWindow *w, const QString &name) {
    const auto folder=qEnvironmentVariable("RELAXER_PHASE_E_SCREENSHOTS");
    if (folder.isEmpty()) return;
    QDir().mkpath(folder); w->grabWindow().save(QDir(folder).filePath(name+".png"));
}
QJsonObject memorySnapshot() {
    QJsonObject result;
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX info{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&info), sizeof(info))) {
        result["workingSetMiB"] = double(info.WorkingSetSize) / 1048576.;
        result["privateMiB"] = double(info.PrivateUsage) / 1048576.;
    }
#endif
    return result;
}
}
class PhaseEUiTests final : public QObject {
    Q_OBJECT
private slots:
    void pagesAreDeferredAndBackgroundKeepsUnsavedAppearance() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        for (const auto *name : {"settingsPage", "aiPage", "appearancePage", "versionLogPage", "lyricsWorkspace","nowPlayingPage"})
            QVERIFY2(!visual(root, name), name);
        for (const auto *name : {"playlistEditor","importDialog","batchInstrumentalDialog"})
            QVERIFY2(!w->findChild<QObject *>(name), name);
        QVERIFY(f.shell->appBridge()->navigate(3));
        QTRY_VERIFY(visual(root, "settingsPage"));
        auto *settings=visual(root,"settingsPage");
        QVERIFY(!visual(root,"aiPage")); QVERIFY(!visual(root,"appearancePage"));
        QVERIFY(f.shell->appBridge()->navigate(0)); QVERIFY(f.shell->appBridge()->navigate(3));
        QCOMPARE(visual(root,"settingsPage"),settings);
        f.shell->settingsBridge()->beginAppearance();
        w->setProperty("settingsSection","appearance");
        QTRY_VERIFY(visual(root,"appearancePage"));
        QVERIFY(f.shell->settingsBridge()->setAppearanceMode(3));
        QVERIFY(f.shell->settingsBridge()->editAppearance("right","#335577"));
        const auto draft=f.shell->settingsBridge()->appearanceDraft();
        w->hide(); QTRY_VERIFY(w->property("backgrounded").toBool()); QTest::qWait(400);
        QVERIFY(f.shell->settingsBridge()->appearanceDirty());
        QCOMPARE(f.shell->settingsBridge()->appearanceDraft(),draft);
        w->showNormal(); QTRY_VERIFY(!w->property("backgrounded").toBool());
        QCOMPARE(f.shell->settingsBridge()->appearanceDraft(),draft);
        f.shell->settingsBridge()->discardAppearance();
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void backgroundReleasesImagesAndRestoresWhileTransportContinues() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        auto *art=visual(root,"bottomCover"); QVERIFY(art);
        QTRY_VERIFY(!art->property("displayedUrl").toUrl().isEmpty());
        f.shell->facade()->toggle(); QTRY_VERIFY(f.shell->facade()->playing());
        const auto track=f.shell->facade()->current().id;
        const auto queue=f.shell->facade()->queue().items;
        QTest::qWait(1200);
        QJsonObject evidence{{"foreground",memorySnapshot()}};
        const auto position=f.shell->facade()->position();
        w->hide(); QTRY_VERIFY(w->property("backgrounded").toBool());
        QTRY_VERIFY(art->property("displayedUrl").toUrl().isEmpty());
        QTest::qWait(1600); evidence["hidden"]=memorySnapshot();
        QVERIFY(f.shell->facade()->playing());
        QVERIFY(f.shell->facade()->position()>position+500);
        QCOMPARE(f.shell->facade()->current().id,track); QCOMPARE(f.shell->facade()->queue().items,queue);
        w->showNormal(); QTRY_VERIFY(QTest::qWaitForWindowExposed(w));
        QTRY_VERIFY(!art->property("displayedUrl").toUrl().isEmpty());
        QTest::qWait(500); evidence["restored"]=memorySnapshot();
        w->showMinimized(); QTRY_VERIFY(w->property("backgrounded").toBool());
        QTRY_VERIFY(art->property("displayedUrl").toUrl().isEmpty());
        QTest::qWait(500); evidence["minimized"]=memorySnapshot();
        QVERIFY(f.shell->facade()->playing());
        w->showNormal(); QTRY_VERIFY(!w->property("backgrounded").toBool());
        QTRY_VERIFY(!art->property("displayedUrl").toUrl().isEmpty());
        qInfo().noquote()<<"BACKGROUND_RESOURCE_EVIDENCE"<<QJsonDocument(evidence).toJson(QJsonDocument::Compact);
        QTest::qWait(300); // Allow the restored Canvas image to reach a rendered frame.
        shot(w,"background-restored");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void nonCurrentTrackHoverStaysStableOverPlaybackButton() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        const auto original=f.shell->facade()->current().id;
        const auto key=QString::number(f.b.id); QVERIFY(f.b.id!=original);
        auto *button=visual(root,"trackPlayback-"+key), *number=visual(root,"trackNumber-"+key);
        auto *favorite=visual(root,"trackFavorite-"+key); QVERIFY(button); QVERIFY(number); QVERIFY(favorite);
        auto *row=button->parentItem();
        while(row && row->objectName()!="trackDelegate") row=row->parentItem();
        QVERIFY(row);
        QTest::mouseMove(w,QPoint(15,50)); QTest::qWait(80);
        QVERIFY(!button->isVisible()); QVERIFY(number->isVisible());
        const auto position=bounds(button).center().toPoint();
        QTest::mouseMove(w,position); QTest::qWait(80);
        const auto hoverColor=w->property("designTokens").value<QObject *>()->property("hover").value<QColor>();
        // Keep the pointer stationary across frames, then move within the
        // interactive child. Revealing that child must not cancel row hover.
        for(int sample=0;sample<24;++sample) {
            if(sample%6==0) QTest::mouseMove(w,position+QPoint(sample%3-1,0));
            QTest::qWait(15);
            QVERIFY2(button->isVisible(),"Hovering the sequence cell hid its own playback button");
            QVERIFY(!number->isVisible()); QCOMPARE(row->property("color").value<QColor>(),hoverColor);
            QCOMPARE(f.shell->facade()->current().id,original);
        }
        QTest::mouseMove(w,bounds(favorite).center().toPoint()); QTest::qWait(80);
        QVERIFY(button->isVisible()); QCOMPARE(row->property("color").value<QColor>(),hoverColor);
        QTest::mouseMove(w,QPoint(15,50)); QTest::qWait(80);
        QVERIFY(!button->isVisible()); QVERIFY(number->isVisible());
        QTest::mouseMove(w,position); QTest::qWait(80);
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,position); QTest::qWait(40);
        QVERIFY(button->isVisible()); QCOMPARE(f.shell->facade()->current().id,original);
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,position);
        QTRY_COMPARE(f.shell->facade()->current().id,f.b.id);
        QTRY_VERIFY(f.shell->facade()->playing());
        QTest::mouseMove(w,QPoint(15,50)); QTest::qWait(80);
        QVERIFY(button->isVisible()); QVERIFY(!number->isVisible());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void songRowPlaybackIconsAreLargerAndFollowTransportState() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem(); auto *player=f.shell->facade();
        const auto key=QString::number(f.a.id);
        auto *button=visual(root,"trackPlayback-"+key), *favorite=visual(root,"trackFavorite-"+key); QVERIFY(button); QVERIFY(favorite);
        QTRY_VERIFY(player->seekable()); player->seek(3000); QTRY_VERIFY(player->position()>=3000);
        for (const QSize size:{QSize(1000,660),QSize(1240,790)}) {
            w->resize(size); QTest::qWait(80); f.shell->playbackBridge()->pause();
            QTRY_COMPARE(button->property("symbol").toString(),QString("play")); QVERIFY(button->isVisible());
            auto *icon=qvariant_cast<QQuickItem *>(button->property("contentItem")); QVERIFY(icon); QVERIFY(icon->width()>=24); QVERIFY(icon->height()>=24);
            QVERIFY(bounds(button).right()<=bounds(favorite).left());
            shot(w,size.width()==1000?"track-play-icon-compact":"track-play-icon-wide");
            const auto position=player->position();
            QVERIFY(click(w,button)); QTRY_VERIFY(player->playing()); QTRY_COMPARE(button->property("symbol").toString(),QString("pause"));
            QVERIFY(player->position()>=position-100);
            shot(w,size.width()==1000?"track-pause-icon-compact":"track-pause-icon-wide");
            QVERIFY(click(w,button)); QTRY_VERIFY(!player->playing()); QTRY_COMPARE(button->property("symbol").toString(),QString("play"));
            QVERIFY(player->position()>=position-100);
        }
        auto *other=visual(root,"trackPlayback-"+QString::number(f.b.id)); QVERIFY(other);
        QTest::mouseMove(w,bounds(other).center().toPoint()); QTRY_VERIFY(other->isVisible());
        QCOMPARE(other->property("symbol").toString(),QString("play")); QVERIFY(click(w,other));
        QTRY_COMPARE(player->current().id,f.b.id);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void lyricsWorkspaceAnimatesAndTogglesPreserveSelection() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        const auto missing=seed(f.library,"Missing lyrics","phase-e-missing"); QVERIFY(missing.id);
        QTRY_COMPARE(f.shell->libraryModel()->count(),3);
        QVERIFY(!visual(root,"lyricsWorkspace"));
        QVERIFY(click(w,visual(root,"findMissingLyrics")));
        auto *workspace=visual(root,"lyricsWorkspace"); QVERIFY(workspace);
        QVERIFY(workspace->isVisible()); QVERIFY(!visual(root,"contentHost")->isVisible()); QVERIFY(!visual(root,"playerBar")->isVisible());
        auto *body=qvariant_cast<QQuickItem *>(workspace->property("contentItem")); QVERIFY(body);
        const auto firstTop=bounds(body).top(); shot(w,"lyrics-entry-start");
        QTest::qWait(60);
        QVERIFY(body->opacity()>0 && body->opacity()<1); QVERIFY(bounds(body).top()>firstTop);
        const auto mask=w->grabWindow(); QVERIFY(!mask.isNull());
        auto *tokens=w->property("designTokens").value<QObject *>(); QVERIFY(tokens);
        const auto actualMask=mask.pixelColor(5,mask.height()/2);
        const auto expectedMask=tokens->property("background").value<QColor>();
        // Keep testing the mask during entry: frame compositing can round an
        // RGB channel by one level. Alpha must still be fully opaque.
        QCOMPARE(actualMask.alpha(),255);QCOMPARE(actualMask.alpha(),expectedMask.alpha());
        QVERIFY2(qAbs(actualMask.red()-expectedMask.red())<=1
                 &&qAbs(actualMask.green()-expectedMask.green())<=1
                 &&qAbs(actualMask.blue()-expectedMask.blue())<=1,
                 qPrintable(QString("Entry mask %1 differs from background %2")
                            .arg(actualMask.name(QColor::HexArgb),expectedMask.name(QColor::HexArgb))));
        shot(w,"lyrics-entry-middle"); QTRY_VERIFY(body->opacity()>.999);
        QCOMPARE(f.shell->lyricsSearchBridge()->targetTrackKey(),QString::number(missing.id));
        QVERIFY(f.shell->lyricsSearchBridge()->setTargetTracks({QString::number(f.a.id),QString::number(f.b.id)},false,true));
        QVERIFY(f.shell->lyricsSearchBridge()->setTargetTrack(QString::number(f.a.id)));
        auto *back=visual(root,"lyricsWorkspaceClose"); QVERIFY(back); QVERIFY(bounds(back).top()>=24);
        auto *multi=visual(root,"lyricsMultiSelect"); QVERIFY(multi); shot(w,"toggle-circle");
        const auto beforeWidth=visual(root,"lyricsMissingOnly")->width();
        auto *batch=visual(root,"lyricsBatchReveal"), *start=visual(root,"batchLyricsSearchButton"); QVERIFY(batch); QVERIFY(start);
        auto *startBackground=qvariant_cast<QQuickItem *>(start->property("background")); QVERIFY(startBackground);
        const auto enabledColor=startBackground->property("color").value<QColor>();
        QVERIFY(click(w,multi)); QVERIFY(multi->property("checked").toBool()); QTest::qWait(60);
        QVERIFY(batch->property("progress").toReal()>0 && batch->property("progress").toReal()<1);
        QVERIFY(startBackground->property("color").value<QColor>()!=enabledColor);
        QVERIFY(startBackground->property("color").value<QColor>()!=tokens->property("background").value<QColor>());
        QVERIFY(multi->property("markProgress").toReal()>0 && multi->property("markProgress").toReal()<1); shot(w,"toggle-morph");
        QTRY_COMPARE(multi->property("markProgress").toReal(),1.); shot(w,"toggle-check");
        auto *filters=visual(root,"lyricsFilterRow"); QVERIFY(filters);
        for(auto *child:filters->childItems()) if(child->isVisible()) QVERIFY(bounds(child).right()<=bounds(filters).right()+1);
        auto *all=visual(root,"lyricsSelectAll"); QVERIFY(click(w,all));
        QVERIFY(visual(root,"lyricsMissingOnly")->width()<beforeWidth); QVERIFY(qAbs(bounds(all).top()-bounds(multi).top())<1);
        QCOMPARE(workspace->property("selectedTracks").toList().size(),2);
        QVERIFY(all->property("checked").toBool()); QVERIFY(click(w,all)); QVERIFY(workspace->property("selectedTracks").toList().isEmpty());
        QVERIFY(!all->property("checked").toBool()); QVERIFY(visual(root,"lyricsBatchInstrumental")->height()<=26);
        QVERIFY(click(w,all));
        QVERIFY(click(w,multi)); QVERIFY(!multi->property("checked").toBool());
        QVERIFY(workspace->property("selectedTracks").toList().isEmpty());
        QTRY_COMPARE(multi->property("markProgress").toReal(),0.);
        auto *paste=visual(root,"lyricsPasteButton"), *local=visual(root,"lyricsLocalImportButton"); QVERIFY(paste); QVERIFY(local);
        QVERIFY(qAbs(bounds(paste).right()-bounds(local).right())<1); QVERIFY(bounds(local).top()>bounds(paste).top());
        QVERIFY(click(w,paste)); auto *pasteDialog=w->findChild<QObject *>("lyricsPasteDialog"); QVERIFY(pasteDialog);
        QTRY_VERIFY(pasteDialog->property("opened").toBool()); QVERIFY(QMetaObject::invokeMethod(pasteDialog,"reject")); QTest::qWait(150);
        for(const QSize size:{QSize(1000,660),QSize(1240,790)}) {
            w->resize(size); QTest::qWait(80);
            auto *middle=visual(root,"lyricsSearchColumn"), *right=visual(root,"lyricsTranslationColumn"); QVERIFY(middle); QVERIFY(right);
            QVERIFY(qAbs(bounds(middle).top()-bounds(back).top())<1);
            QVERIFY(qAbs(bounds(right).top()-32-(w->height()-bounds(right).bottom()))<1);
            QVERIFY(click(w,multi)); QTest::qWait(300); QVERIFY(qAbs(bounds(all).top()-bounds(multi).top())<1);
            for(auto *child:filters->childItems()) if(child->isVisible()) QVERIFY(bounds(child).right()<=bounds(filters).right()+1);
            shot(w,size.width()==1000?"lyrics-compact-layout":"lyrics-wide-layout"); QVERIFY(click(w,multi)); QTest::qWait(200);
        }
        auto *target=visual(root,"lyricsTarget-"+QString::number(f.a.id)); QVERIFY(target); QVERIFY(target->setProperty("searchState","searching"));
        auto *spinner=visual(target,"lyricsStateSpinner-"+QString::number(f.a.id)); QVERIFY(spinner);
        auto *ring=qvariant_cast<QQuickItem *>(spinner->property("contentItem")); QVERIFY(ring);
        QTest::qWait(100); const auto angle=ring->rotation(); QTest::qWait(100); QVERIFY(qAbs(ring->rotation()-angle)>10);
        QVERIFY(target->setProperty("searchState","complete"));
        shot(w,"lyrics-workspace");
        const auto settledTop=bounds(body).top();
        QVERIFY(QMetaObject::invokeMethod(workspace,"close")); QTest::qWait(100);
        QVERIFY(workspace->isVisible()); QVERIFY(body->opacity()>0 && body->opacity()<1); QVERIFY(bounds(body).top()<settledTop);
        QVERIFY(visual(root,"contentHost")->isVisible()); shot(w,"lyrics-exit-middle");
        QTRY_VERIFY(!workspace->isVisible());
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
    void dropdownsAnimateAndBrowserMenuUsesButtonAnchor() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        auto exercisePopup=[&](QQuickItem *control, const QString &name) -> bool {
            if(!control) return false;
            auto *popup=qvariant_cast<QObject *>(control->property("popup")); if(!popup) return false;
            PopupHeightProbe probe(popup);
            if(!QQmlProperty(popup,"height").connectNotifySignal(&probe,SLOT(sample())))return false;
            if(!click(w,control))return false;
            const auto openingWidth=popup->property("width").toReal();
            if(!QTest::qWaitFor([&]{return popup->property("opened").toBool();},1000))return false;
            qInfo().noquote()<<name<<"open"<<probe.diagnostic();
            const bool animating=probe.animated();
            shot(w,name);
            if(qAbs(popup->property("width").toReal()-openingWidth)>.01
                || qAbs(popup->property("height").toReal()-popup->property("expandedHeight").toReal())>.01)return false;
            probe.reset();
            if(!QMetaObject::invokeMethod(popup,"close")) return false;
            if(!QTest::qWaitFor([&]{return !popup->property("visible").toBool();},1000))return false;
            qInfo().noquote()<<name<<"close"<<probe.diagnostic();
            return animating && probe.animated() && qAbs(popup->property("width").toReal()-openingWidth)<.01;
        };
        QVERIFY(click(w,visual(root,"findMissingLyrics"))); QTest::qWait(300);
        QVERIFY(exercisePopup(visual(root,"lyricsCandidateSourceFilter"),"sources-dropdown"));
        QVERIFY(exercisePopup(visual(root,"lyricsSourceLanguage"),"language-dropdown"));
        QVERIFY(click(w,visual(root,"lyricsSearchSettingsButton"))); QTest::qWait(280);
        auto *source=visual(root,"lyricsSource-lrclib"); QVERIFY(source); QVERIFY(source->property("markProgress").isValid());
        QVERIFY(click(w,source)); QTest::qWait(60); QCOMPARE(visual(root,"lyricsSource-lrclib"),source);
        QVERIFY(source->property("markProgress").toReal()>0 && source->property("markProgress").toReal()<1);
        QTest::qWait(240); source=visual(root,"lyricsSource-lrclib"); QVERIFY(source); QVERIFY(!source->property("checked").toBool());
        QVERIFY(click(w,source)); QTest::qWait(300); source=visual(root,"lyricsSource-lrclib"); QVERIFY(source); QVERIFY(source->property("checked").toBool()); shot(w,"source-toggle-buttons");
        QVERIFY(click(w,visual(root,"lyricsSearchSettingsClose"))); QTest::qWait(150);
        auto *browser=visual(root,"lyricsBrowserButton"); QVERIFY(browser);
        auto *menu=w->findChild<QObject *>("lyricsBrowserMenu"); QVERIFY(menu); QPointF menuOrigin;
        PopupHeightProbe menuProbe(menu);
        QVERIFY(QQmlProperty(menu,"height").connectNotifySignal(&menuProbe,SLOT(sample())));
        for(int point:{0,1}) {
            const auto button=bounds(browser);
            menuProbe.reset();
            QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,QPointF(button.left()+button.width()*(point?.8:.2),button.center().y()).toPoint());
            const auto openingWidth=menu->property("width").toReal();
            QTRY_VERIFY_WITH_TIMEOUT(menu->property("opened").toBool(),1000);
            qInfo().noquote()<<"browser menu open"<<point<<menuProbe.diagnostic();
            QVERIFY2(menuProbe.animated(),qPrintable(menuProbe.diagnostic()));
            QCOMPARE(menu->property("scale").toReal(),1.);
            QCOMPARE(menu->property("width").toReal(),openingWidth);
            QCOMPARE(menu->property("height").toReal(),menu->property("expandedHeight").toReal());
            auto *content=qvariant_cast<QQuickItem *>(menu->property("contentItem")); QVERIFY(content);
            const auto origin=bounds(content).topLeft();
            if(point) QVERIFY(QLineF(origin,menuOrigin).length()<1); else menuOrigin=origin;
            QVERIFY(qAbs(bounds(content).right()+menu->property("rightPadding").toReal()-button.right())<1);
            QVERIFY(qAbs(origin.y()-menu->property("topPadding").toReal()-button.bottom()-4)<1);
            shot(w,"anchored-browser-menu"); menuProbe.reset();QVERIFY(QMetaObject::invokeMethod(menu,"close"));
            QTRY_VERIFY_WITH_TIMEOUT(!menu->property("visible").toBool(),1000);
            QVERIFY2(menuProbe.animated(),qPrintable(menuProbe.diagnostic()));
        }
        QVERIFY(QMetaObject::invokeMethod(visual(root,"lyricsWorkspace"),"close")); QTest::qWait(300);
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(280);
        QVERIFY(exercisePopup(visual(root,"qmlThemeSetting"),"theme-dropdown"));
        QVERIFY(click(w,visual(root,"openAiServiceSettings"))); QTest::qWait(300);
        QVERIFY(exercisePopup(visual(root,"aiProviderSelection"),"api-provider-dropdown"));
        auto *scroll=visual(root,"aiSettingsScroll"); auto *flick=qvariant_cast<QQuickItem *>(scroll->property("contentItem")); QVERIFY(flick);
        flick->setProperty("contentY",qMax(0.,bounds(visual(root,"aiLyricsTranslationSettings")).bottom()-bounds(scroll).bottom()+20)); QTest::qWait(60);
        QVERIFY(click(w,visual(root,"aiLyricsTranslationSettings"))); QTest::qWait(300);
        auto *global=visual(root,"aiTranslationUseGlobal"); QVERIFY(global); QVERIFY(global->property("markProgress").isValid());
        const auto wasGlobal=global->property("checked").toBool(); QVERIFY(click(w,global)); QTest::qWait(300);
        QCOMPARE(f.shell->aiBridge()->draft().value("translationUsesDefaults").toBool(),!wasGlobal);
        shot(w,"translation-toggle-button");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void multiSelectSlidesOnceAndClosingKeepsRowsStable() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        QStringList keys{QString::number(f.a.id),QString::number(f.b.id)};
        for(int i=0;i<24;++i) { const auto track=seed(f.library,QString("Selection track %1").arg(i),QString("selection-%1").arg(i)); QVERIFY(track.id); keys.append(QString::number(track.id)); }
        QVERIFY(f.shell->lyricsSearchBridge()->setTargetTracks(keys,false,true));
        QVERIFY(QMetaObject::invokeMethod(w,"ensureLyricsWorkspace"));
        auto *workspace=visual(root,"lyricsWorkspace"), *tracks=visual(root,"lyricsTargetTracks"); QVERIFY(workspace); QVERIFY(tracks);
        QVERIFY(QMetaObject::invokeMethod(workspace,"open")); QTest::qWait(300); QCOMPARE(tracks->property("count").toInt(),keys.size());
        auto *text=visual(root,"lyricsTargetText-"+keys.first()), *check=visual(root,"lyricsCheck-"+keys.first()); QVERIFY(text); QVERIFY(check);
        const auto initialX=text->x(); QVERIFY(!check->isVisible());
        QVERIFY(click(w,visual(root,"lyricsMultiSelect"))); QTest::qWait(60);
        QVERIFY(check->isVisible()); QVERIFY(check->x()>-30 && check->x()<0);
        QVERIFY(check->opacity()>0 && check->opacity()<1); QVERIFY(text->x()>initialX && text->x()<initialX+30);
        shot(w,"multiselect-slide-middle"); QTest::qWait(240); QCOMPARE(text->x(),initialX+30);
        QVERIFY(QMetaObject::invokeMethod(tracks,"positionViewAtEnd")); QTest::qWait(60);
        text=visual(root,"lyricsTargetText-"+keys.last()); check=visual(root,"lyricsCheck-"+keys.last()); QVERIFY(text); QVERIFY(check);
        QCOMPARE(text->x(),initialX+30); QCOMPARE(check->x(),0.); QCOMPARE(check->opacity(),1.);
        shot(w,"multiselect-scrolled");
        QVERIFY(QMetaObject::invokeMethod(tracks,"positionViewAtBeginning")); QTest::qWait(60);
        check=visual(root,"lyricsCheck-"+keys.first()); QVERIFY(check); QCOMPARE(check->x(),0.); QCOMPARE(check->opacity(),1.);
        auto *summary=visual(root,"lyricsTargetSummary"); QVERIFY(summary); const auto caption=summary->property("text").toString();
        QVERIFY(QMetaObject::invokeMethod(workspace,"close")); QTest::qWait(100); QVERIFY(workspace->isVisible());
        QCOMPARE(summary->property("text").toString(),caption); QCOMPARE(tracks->property("count").toInt(),keys.size());
        shot(w,"lyrics-exit-keeps-count"); QTRY_VERIFY(!workspace->isVisible());
        QVERIFY(QMetaObject::invokeMethod(workspace,"open")); QTest::qWait(300);
        QCOMPARE(tracks->property("count").toInt(),f.shell->lyricsSearchBridge()->targetTracks().size());
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void lyricsSeekOnlyThroughLeftPreviewButton() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem(); auto *player=f.shell->facade();
        QTRY_VERIFY(player->seekable()); QVERIFY(QMetaObject::invokeMethod(w,"showLyrics")); QTRY_COMPARE(w->property("playerProgress").toReal(),1.);
        auto *panel=visual(root,"lyricsPanel"), *list=visual(root,"lyricsList"), *stamp=visual(root,"lyricsPreviewTime"); QVERIFY(panel); QVERIFY(list); QVERIFY(stamp);
        for(const QSize size:{QSize(1000,660),QSize(1240,790)}) {
            w->resize(size); player->seek(3000); QTRY_COMPARE(f.shell->lyricsModel()->currentIndex(),3);
            QVERIFY(QMetaObject::invokeMethod(panel,"returnCurrent")); QTest::qWait(420);
            QVERIFY(QMetaObject::invokeMethod(panel,"takeOver")); QTest::qWait(60);
            list->setProperty("contentY",list->property("contentY").toReal()+400); QTest::qWait(80);
            QVERIFY(stamp->isVisible()); QVERIFY(bounds(stamp).right()<=bounds(list).left()-11); QVERIFY(bounds(stamp).left()>=0);
            const auto selected=panel->property("previewIndex").toInt(); QVERIFY(selected>3);
            QQuickItem *row=nullptr; for(auto *item:delegates(list)) if(item->property("index").toInt()==selected) row=item; QVERIFY(row);
            const auto before=player->position(); QVERIFY(click(w,row)); QTest::qWait(100);
            QVERIFY(qAbs(player->position()-before)<100); QVERIFY(panel->property("manualScroll").toBool());
            shot(w,size.width()==1000?"lyrics-left-preview-compact":"lyrics-left-preview-wide");
            const auto time=f.shell->lyricsModel()->timestampAt(selected); QVERIFY(click(w,stamp));
            QTRY_VERIFY(qAbs(player->position()-time)<250); QVERIFY(!panel->property("manualScroll").toBool());
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void lyricsPreviewClickKeepsItsTargetWhilePlaying() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem(); auto *player=f.shell->facade();
        QTRY_VERIFY(player->seekable()); player->toggle(); QTRY_VERIFY(player->playing());
        QVERIFY(QMetaObject::invokeMethod(w,"showLyrics")); QTRY_COMPARE(w->property("playerProgress").toReal(),1.);
        auto *panel=visual(root,"lyricsPanel"), *list=visual(root,"lyricsList"), *stamp=visual(root,"lyricsPreviewTime"); QVERIFY(panel); QVERIFY(list); QVERIFY(stamp);
        player->seek(3000); QVERIFY(QMetaObject::invokeMethod(panel,"returnCurrent")); QTest::qWait(420);
        QVERIFY(QMetaObject::invokeMethod(panel,"takeOver"));
        list->setProperty("contentY",list->property("contentY").toReal()+1000); QTest::qWait(80);
        QVERIFY(stamp->isVisible()); const auto selected=panel->property("previewIndex").toInt(); QVERIFY(selected>8);
        const auto time=f.shell->lyricsModel()->timestampAt(selected);
        QTest::mouseMove(w,bounds(stamp).center().toPoint());
        QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,bounds(stamp).center().toPoint());
        QVERIFY(stamp->property("pressed").toBool()); QTest::qWait(3200);
        QVERIFY(stamp->isVisible()); QVERIFY(panel->property("manualScroll").toBool());
        QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,bounds(stamp).center().toPoint());
        QTRY_VERIFY(qAbs(player->position()-time)<500); QVERIFY(player->playing());
        QVERIFY(!panel->property("manualScroll").toBool());
        QVERIFY(QMetaObject::invokeMethod(w,"toggleFullscreen")); QTRY_COMPARE(w->visibility(),QWindow::FullScreen);
        QTest::qWait(250); QVERIFY(QMetaObject::invokeMethod(panel,"takeOver"));
        list->setProperty("contentY",list->property("contentY").toReal()+400); QTest::qWait(80);
        const auto fullTime=f.shell->lyricsModel()->timestampAt(panel->property("previewIndex").toInt());
        QVERIFY(click(w,stamp)); QTRY_VERIFY(qAbs(player->position()-fullTime)<500);
        QVERIFY(!panel->property("manualScroll").toBool()); QVERIFY(player->playing());
        QVERIFY(QMetaObject::invokeMethod(w,"toggleFullscreen")); QTRY_COMPARE(w->visibility(),QWindow::Windowed);
        QTest::mouseMove(w,bounds(list).center().toPoint());
        auto *tools=visual(root,"lyricToolsQuestion"); QVERIFY(tools); QTRY_VERIFY(tools->opacity()>.99); QVERIFY(click(w,tools));
        auto *menu=w->findChild<QObject *>("lyricToolsMenu"); QVERIFY(menu); QTRY_VERIFY(menu->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(menu,"close"));
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
        auto *list=visual(root,"trackListView"), *bar=visual(root,"playerBar"); QVERIFY(list); QVERIFY(bar);
        QVERIFY(qAbs(bounds(list).bottom()-bounds(bar).top())<1);
        QVERIFY(click(w,visual(root,"navigation2"))); QTRY_COMPARE(f.shell->appBridge()->page(),2); QTest::qWait(280);
        auto *recent=visual(visual(root,"profilePage"),"trackListView"); QVERIFY(recent);
        QVERIFY(qAbs(bounds(recent).bottom()-bounds(bar).top())<1); shot(w,"profile-list-bottom");
        QVERIFY(click(w,visual(root,"navigation0"))); QTRY_COMPARE(f.shell->appBridge()->page(),0); QTest::qWait(280);
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
    void qwenRecommendedModelsCanPrepareLyricsTranslation() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem(); auto *bridge=f.shell->aiBridge();
        auto ids=qwen_fixture::CurrentModels; ids << "qwen-mt-plus" << "qwen-plus";
        const auto catalog=ai::parseCompatibleModelCatalog(qwen_fixture::catalog(ids),"qwen"); QVERIFY(catalog.ok);
        QVERIFY(f.runtime->acceptModelCatalog("qwen",catalog.models)); QVERIFY(f.runtime->credentials()->store("qwen","fake-qwen-ui-key",nullptr));
        QVERIFY(bridge->edit("providerId","qwen")); QVERIFY(bridge->save());
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(280);
        QVERIFY(click(w,visual(root,"openAiServiceSettings"))); QTest::qWait(300);
        auto *models=visual(root,"aiDefaultModel"); QVERIFY(models); QCOMPARE(models->property("count").toInt(),6);
        QCOMPARE(models->property("currentValue").toString(),QString("qwen3.8-max"));
        QVERIFY(bridge->defaultModelCapabilities().contains("LOW / MEDIUM / XHIGH"));
        shot(w,"qwen-recommended-models");
        QVERIFY(click(w,models)); QTest::qWait(280); shot(w,"qwen-recommended-dropdown");
        auto *popup=qvariant_cast<QObject *>(models->property("popup")); QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup,"close")); QTest::qWait(180);
        auto *thinking=visual(root,"aiDefaultThinking"); QVERIFY(thinking); QVERIFY(thinking->isEnabled());
        QCOMPARE(thinking->property("count").toInt(),5); QCOMPARE(thinking->property("currentValue").toString(),QString("off"));
        QVERIFY(click(w,thinking)); QTest::qWait(280);
        auto *thinkingPopup=qvariant_cast<QObject *>(thinking->property("popup")); QVERIFY(thinkingPopup);
        auto *choices=qvariant_cast<QQuickItem *>(thinkingPopup->property("contentItem")); QVERIFY(choices);
        auto *choiceContent=qvariant_cast<QQuickItem *>(choices->property("contentItem")); QVERIFY(choiceContent);
        QQuickItem *enableChoice=nullptr;
        for (auto *item:choiceContent->childItems()) if (item->property("text").toString()=="开启思考（模型默认）") enableChoice=item;
        QVERIFY(click(w,enableChoice));
        QTRY_COMPARE(f.runtime->configuration().defaults.reasoningEffort,QString("default"));
        QVERIFY(f.runtime->configuration().defaults.thinkingEnabled); QTest::qWait(180); shot(w,"qwen-thinking-enabled");
        QVERIFY(click(w,thinking)); QTest::qWait(280); shot(w,"qwen-strength-options");
        QQuickItem *mediumChoice=nullptr;
        for (auto *item:choiceContent->childItems()) if (item->property("text").toString()=="MEDIUM") mediumChoice=item;
        QVERIFY(click(w,mediumChoice)); QTRY_COMPARE(f.runtime->configuration().defaults.reasoningEffort,QString("medium"));
        auto *translation=f.shell->translationBridge(); QVERIFY(translation->setTargetTrack(QString::number(f.a.id))); QVERIFY(translation->classify(1));
        const auto mediumTask=translation->prepareAi(); QVERIFY2(!mediumTask.isEmpty(),qPrintable(translation->status())); translation->declineConsent();
        QVERIFY(bridge->setThinkingEffort(false,"default"));
        for (const auto &id:qwen_fixture::CurrentModels) {
            QVERIFY(bridge->edit("modelId",id)); QVERIFY(bridge->save());
            QVERIFY(f.runtime->validateConfiguration(f.runtime->configuration().defaults).isEmpty());
            const auto task=translation->prepareAi(); QVERIFY2(!task.isEmpty(),qPrintable(translation->status()));
            translation->declineConsent();
        }
        QVERIFY(bridge->setThinkingEffort(false,"off")); QVERIFY(!f.runtime->configuration().defaults.thinkingEnabled);
        QCOMPARE(f.http.requestCount(),0);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void kimiK3SettingsCanPrepareLyricsTranslation() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem(); auto *bridge=f.shell->aiBridge();
        const auto catalog=ai::parseCompatibleModelCatalog(R"({"object":"list","data":[{"id":"kimi-k3"}]})","kimi");
        QVERIFY(f.runtime->acceptModelCatalog("kimi",catalog.models)); QVERIFY(f.runtime->credentials()->store("kimi","fake-k3-ui-key",nullptr));
        QVERIFY(bridge->edit("providerId","kimi")); QVERIFY(bridge->edit("modelId","kimi-k3")); QVERIFY(bridge->save());
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(280);
        QVERIFY(click(w,visual(root,"openAiServiceSettings"))); QTest::qWait(300);
        auto *thinking=visual(root,"aiDefaultThinking"); QVERIFY(thinking); QVERIFY(thinking->isEnabled());
        QCOMPARE(thinking->property("currentValue").toString(),QString("invalid")); QCOMPARE(thinking->property("count").toInt(),4);
        QVERIFY(click(w,thinking)); QTest::qWait(280); QTest::keyClick(w,Qt::Key_Down); QTest::keyClick(w,Qt::Key_Return);
        QTRY_COMPARE(f.runtime->configuration().defaults.reasoningEffort,QString("low"));
        QVERIFY(f.runtime->configuration().defaults.thinkingEnabled); QTest::qWait(200); shot(w,"kimi-k3-settings");
        // Reproduce a valid KIMI default alongside a stale independent DeepSeek override.
        auto configuration=f.runtime->configuration(); configuration.translationUsesDefaults=false;
        configuration.translationOverride={"deepseek","deepseek-flash",false,{}};
        QVERIFY(f.runtime->saveConfiguration(configuration)); QVERIFY(f.runtime->acceptModelCatalog("deepseek",{}));
        auto *effective=visual(root,"aiEffectiveTranslationStatus"); QVERIFY(effective);
        QVERIFY(effective->property("text").toString().contains("独立配置"));
        QVERIFY(effective->property("text").toString().contains("DeepSeek / DeepSeek-Flash"));
        QVERIFY(click(w,visual(root,"aiSettingsBack"))); QTest::qWait(300);
        QVERIFY(click(w,visual(root,"navigation0"))); QTRY_COMPARE(f.shell->appBridge()->page(),0); QTest::qWait(280);
        QVERIFY(click(w,visual(root,"findMissingLyrics"))); QTest::qWait(300);
        auto *translation=f.shell->translationBridge();
        QVERIFY(f.shell->lyricsSearchBridge()->setTargetTrack(QString::number(f.a.id)));
        QVERIFY(translation->setTargetTrack(QString::number(f.a.id))); QVERIFY(translation->classify(0));
        auto *button=visual(root,"translateAiButton"), *hint=visual(root,"lyricsAiConfigurationStatus"); QVERIFY(button); QVERIFY(hint);
        QVERIFY(!button->isEnabled()); QVERIFY(hint->property("text").toString().contains("请先选择“需要翻译”"));
        QVERIFY(translation->classify(1)); QVERIFY(button->isEnabled());
        QVERIFY(hint->property("text").toString().contains("DeepSeek / DeepSeek-Flash"));
        QVERIFY(translation->prepareAi().isEmpty()); QVERIFY(translation->status().contains("独立配置"));
        QVERIFY(translation->status().contains("DeepSeek-Flash")); QCOMPARE(f.http.requestCount(),0);
        shot(w,"kimi-translation-override-status");
        QVERIFY(bridge->edit("translationUsesDefaults",true)); QVERIFY(bridge->save());
        QVERIFY(hint->property("text").toString().contains("全局默认配置"));
        QVERIFY(hint->property("text").toString().contains("KIMI / KIMI-K3 · LOW"));
        const auto task=translation->prepareAi(); QVERIFY2(!task.isEmpty(),qPrintable(translation->status()));
        QCOMPARE(f.http.requestCount(),0); translation->declineConsent();
        shot(w,"kimi-translation-ready");
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void settingsSubpagesTransitionOnceAfterSnapshot() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(280);
        QVERIFY(click(w,visual(root,"openAiServiceSettings"))); QTRY_COMPARE(w->property("settingsSection").toString(),QString("ai")); QTest::qWait(180);
        auto *ai=visual(root,"aiPage"), *scroll=visual(root,"aiSettingsScroll"); QVERIFY(ai); QVERIFY(scroll);
        auto *flick=qvariant_cast<QQuickItem *>(scroll->property("contentItem")); QVERIFY(flick);
        auto *translation=visual(root,"aiLyricsTranslationSettings"); QVERIFY(translation);
        flick->setProperty("contentY",qMax(0.,bounds(translation).bottom()-bounds(scroll).bottom()+20)); QTest::qWait(50);
        auto revision=w->property("navigationRevision").toInt();
        QVERIFY(click(w,translation)); QCOMPARE(ai->property("subpage").toString(),QString("home"));
        QCOMPARE(w->property("navigationRevision").toInt(),revision+1);
        QTRY_COMPARE(ai->property("subpage").toString(),QString("translation"));
        auto *surface=visual(root,"pageSurface"); QVERIFY(surface); QVERIFY(surface->y()<0);
        QTRY_COMPARE(surface->y(),0.); shot(w,"ai-translation-settings");
        revision=w->property("navigationRevision").toInt(); QVERIFY(click(w,visual(root,"aiSettingsBack")));
        QCOMPARE(ai->property("subpage").toString(),QString("translation"));
        QTRY_COMPARE(ai->property("subpage").toString(),QString("home")); QTest::qWait(180);
        QCOMPARE(w->property("navigationRevision").toInt(),revision+1);
        QVERIFY(click(w,visual(root,"aiSettingsBack"))); QTRY_COMPARE(w->property("settingsSection").toString(),QString("settings")); QTest::qWait(180);
        // Every exit path keeps Appearance visible until its snapshot is ready.
        for(int path=0;path<4;++path) {
            QVERIFY(click(w,visual(root,"editAppearance"))); QTRY_COMPARE(w->property("settingsSection").toString(),QString("appearance")); QTest::qWait(180);
            if(path>0) {
                const auto mode=f.shell->settingsBridge()->appearanceMode();
                QVERIFY(click(w,visual(root,QString("appearanceMode%1").arg(mode==0?1:0))));
                QVERIFY(f.shell->settingsBridge()->appearanceDirty());
            }
            revision=w->property("navigationRevision").toInt();
            if(path==1 || path==2) {
                QVERIFY(click(w,visual(root,"appearanceBack")));
                auto *dialog=w->findChild<QObject *>("appearanceUnsavedDialog"); QVERIFY(dialog);
                QTRY_VERIFY(dialog->property("opened").toBool()); QTest::qWait(180);
                QVERIFY(click(w,visual(root,path==1?"appearanceLeaveDiscard":"appearanceLeaveSave")));
            } else QVERIFY(click(w,visual(root,path==3?"appearanceSavePreset":"appearanceBack")));
            QCOMPARE(w->property("settingsSection").toString(),QString("appearance"));
            QCOMPARE(w->property("navigationRevision").toInt(),revision+1);
            QTRY_COMPARE(w->property("settingsSection").toString(),QString("settings"));
            QTest::qWait(280); QCOMPARE(w->property("navigationRevision").toInt(),revision+1);
            QCOMPARE(surface->y(),0.); QCOMPARE(surface->opacity(),1.);
            QVERIFY(!f.shell->settingsBridge()->appearanceDirty());
        }
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void compactSettingsStopAtContentAndAppearanceOptionsStayInside() {
        Fixture f; QVERIFY(prepare(f)); auto *w=f.shell->window(); auto *root=w->contentItem();
        QVERIFY(f.shell->settingsBridge()->setTheme(1));
        w->resize(1000,660); QTest::qWait(100);
        QVERIFY(click(w,visual(root,"navigation3"))); QTRY_COMPARE(f.shell->appBridge()->page(),3); QTest::qWait(280);
        auto *appearance=visual(root,"settingsAppearanceCell"), *threshold=visual(root,"qmlThresholdSetting"), *close=visual(root,"closeSetting");
        QVERIFY(appearance); QVERIFY(threshold); QVERIFY(close); const auto cellWidth=appearance->width(),thresholdWidth=threshold->width(),closeWidth=close->width();
        for(int theme:{0,2,1}) {
            QVERIFY(f.shell->settingsBridge()->setTheme(theme)); QVERIFY(f.shell->settingsBridge()->setPreference("closeToTray",theme==2)); QTest::qWait(50);
            QVERIFY(qAbs(appearance->width()-cellWidth)<1); QVERIFY(qAbs(threshold->width()-thresholdWidth)<1); QVERIFY(qAbs(close->width()-closeWidth)<1);
        }
        auto *scroll=visual(root,"settingsScroll"); QVERIFY(scroll);
        QVERIFY(qAbs(bounds(scroll).bottom()-bounds(visual(root,"playerBar")).top())<1);
        auto *bold=visual(root,"mutedBoldSetting"), *reduce=visual(root,"reduceMotionSetting"); QVERIFY(bold); QVERIFY(reduce);
        // Responsive options may wrap. Both controls must fit without overlap.
        QVERIFY(!bounds(bold).intersects(bounds(reduce)));
        const auto optionFlow=bounds(bold->parentItem());
        QVERIFY(optionFlow.contains(bounds(bold)));QVERIFY(optionFlow.contains(bounds(reduce)));
        QVERIFY(click(w,bold)); QVERIFY(f.shell->settingsBridge()->values().value("mutedBold").toBool());
        auto *caption=visual(root,"librarySummary"); QVERIFY(caption); QCOMPARE(caption->property("font").value<QFont>().weight(),QFont::DemiBold);
        QVERIFY(click(w,bold)); QCOMPARE(caption->property("font").value<QFont>().weight(),QFont::Medium);
        shot(w,"settings-global-bold-toggle");
        auto *flick=qvariant_cast<QQuickItem *>(scroll->property("contentItem")); QVERIFY(flick);
        const auto end=qMax(0.,flick->property("contentHeight").toReal()-flick->height());
        flick->setProperty("contentY",end);
        const auto point=bounds(scroll).center().toPoint();
        QWheelEvent wheel(point,w->mapToGlobal(point),QPoint(),QPoint(0,-1200),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(w,&wheel); QTest::qWait(300);
        QVERIFY(flick->property("contentY").toReal()<=end+1);
        shot(w,"compact-settings-bottom");
        auto *about=visual(root,"settingsAboutCard"), *help=visual(root,"shortcutsHelp"), *actions=visual(root,"shortcutsActions"), *shortcuts=visual(root,"shortcutsPage");
        QVERIFY(about); QVERIFY(help); QVERIFY(actions); QVERIFY(shortcuts);
        for(const QSize size:{QSize(1240,790),QSize(1000,660)}) {
            w->resize(size); QTest::qWait(80); flick->setProperty("contentY",qMax(0.,flick->property("contentHeight").toReal()-flick->height())); QTest::qWait(50);
            QVERIFY(qAbs(bounds(scroll).bottom()-bounds(about).bottom()-16)<1);
            QVERIFY(qAbs(bounds(help).center().y()-bounds(actions).center().y())<1);
            QVERIFY(bounds(actions).left()>bounds(help).right()); QVERIFY(qAbs(bounds(actions).right()-bounds(shortcuts).right()+20)<1);
            shot(w,size.width()==1240?"settings-actions-wide":"settings-actions-compact");
        }
        flick->setProperty("contentY",0); QTest::qWait(60);
        QVERIFY(click(w,visual(root,"editAppearance"))); QTest::qWait(280);
        QVERIFY(click(w,visual(root,"appearanceMode4"))); QTest::qWait(120);
        auto *options=visual(root,"appearanceImageOptions"); QVERIFY(options);
        QVERIFY(!visual(root,"appearanceMutedBold"));
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
