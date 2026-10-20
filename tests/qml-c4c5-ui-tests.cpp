#include "c4c5-ui-fixture.h"
#include <QApplication>
#include <QWheelEvent>
#include <QSignalSpy>
using namespace c4c5_fixture;
class QmlC4C5UiTests:public QObject {
    Q_OBJECT
private slots:
    void fullscreenUsesSameWindowRestoresGeometryAndPlayback(){
        Fixture f;QVERIFY(f.ok);auto *window=f.shell->window();window->setGeometry(35,45,1000,700);QTest::qWait(50);
        const auto bounds=window->geometry();auto *facade=f.shell->facade();const auto queue=facade->queue().items;
        QTRY_VERIFY(facade->position()>500);const auto before=facade->position();
        QVERIFY(QMetaObject::invokeMethod(window,"toggleFullscreen"));QTRY_COMPARE(window->visibility(),QWindow::FullScreen);
        QCOMPARE(f.shell->window(),window);QCOMPARE(f.shell->facade(),facade);QCOMPARE(facade->queue().items,queue);
        QTest::qWait(150);QVERIFY(facade->position()>=before);QTest::keyClick(window,Qt::Key_Escape);
        QTRY_VERIFY(window->visibility()!=QWindow::FullScreen);QTRY_COMPARE(window->geometry(),bounds);
        for(int i=0;i<8;++i){QVERIFY(QMetaObject::invokeMethod(window,"toggleFullscreen"));QCoreApplication::processEvents();}
        QTRY_COMPARE(window->geometry(),bounds);QCOMPARE(facade->queue().items,queue);QCOMPARE(f.shell->findChildren<Playback *>().size(),1);
        QCOMPARE(QSqlDatabase::connectionNames().size(),1);QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void lyricHighlightScaleManualScrollAndRealSeek(){
        Fixture f;QVERIFY(f.ok);auto *window=f.shell->window();auto *facade=f.shell->facade();QVERIFY(QMetaObject::invokeMethod(window,"showLyrics"));
        QTRY_COMPARE(f.shell->lyricsModel()->count(),80);QTRY_VERIFY(facade->seekable());f.shell->playbackBridge()->pause();facade->seek(3000);
        QTRY_COMPARE(f.shell->lyricsModel()->currentIndex(),3);QTest::qWait(250);
        auto *panel=visual(window->contentItem(),"lyricsPanel");auto *list=visual(window->contentItem(),"lyricsList");QVERIFY(panel);QVERIFY(list);
        const auto rows=delegates(list);QVERIFY(!rows.isEmpty());QVERIFY(rows.size()<20);int active=0;
        for(auto *row:rows){if(row->property("current").toBool()){++active;QCOMPARE(row->property("activeScale").toDouble(),1.2);QVERIFY(qAbs(row->childItems().first()->scale()-1.2)<0.001);}else QCOMPARE(row->property("activeScale").toDouble(),1.0);}
        QCOMPARE(active,1);const auto point=list->mapToScene(QPointF(list->width()/2,list->height()/2));
        QVERIFY(list->width()>250);QVERIFY(QRectF(0,0,window->width(),window->height()).contains(point));
        QTest::mouseMove(window,point.toPoint());QTest::qWait(20);
        QWheelEvent wheel(point,window->mapToGlobal(point.toPoint()),QPoint(),QPoint(0,-240),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        wheel.setTimestamp(++QTest::lastMouseTimestamp);
        qInfo()<<"lyric viewport"<<list->size()<<"scene wheel"<<point<<"window"<<window->size()<<"visible"<<list->isVisible();
        QCoreApplication::sendEvent(window,&wheel);QTRY_VERIFY(panel->property("manualScroll").toBool());
        QTest::qWait(100);QVERIFY(QMetaObject::invokeMethod(list,"cancelFlick"));QTest::qWait(50);
        QTRY_VERIFY(!list->property("moving").toBool());
        const auto y=list->property("contentY").toReal();facade->seek(12000);QTRY_COMPARE(f.shell->lyricsModel()->currentIndex(),12);
        QTest::qWait(60);QCOMPARE(list->property("contentY").toReal(),y);
        auto *back=visual(window->contentItem(),"lyricsReturnCurrent");QVERIFY(back);QVERIFY(QMetaObject::invokeMethod(back,"click"));QVERIFY(!panel->property("manualScroll").toBool());
        QQuickItem *target=nullptr;for(auto *row:delegates(list))if(row->property("index").toInt()==12)target=row;QVERIFY(target);
        const auto hit=target->mapToScene(QPointF(target->width()/2,target->height()/2)).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,hit);QTRY_VERIFY(qAbs(facade->position()-12000)<200);
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void artworkResetsAndReduceMotionSettingsAreLive(){
        Fixture f;QVERIFY(f.ok);auto *window=f.shell->window();QVERIFY(QMetaObject::invokeMethod(window,"showLyrics"));QTRY_VERIFY(!f.shell->lyricsModel()->cover().isEmpty());
        auto *art=visual(window->contentItem(),"currentArtwork");QVERIFY(art);QTRY_COMPARE(art->property("file").toUrl(),f.shell->playbackBridge()->coverUrl());
        f.shell->facade()->play({f.a.id,f.b.id},f.b.id);QTRY_COMPARE(f.shell->lyricsModel()->trackKey(),QString::number(f.b.id));
        QTRY_VERIFY(f.shell->lyricsModel()->cover().isEmpty());QTRY_COMPARE(art->property("file").toUrl(),QUrl());QTRY_VERIFY(art->isVisible());QVERIFY(art->property("imageUrl").toUrl().scheme()=="image");
        QVERIFY(f.shell->settingsBridge()->edit("reduceMotion",true));QVERIFY(f.shell->settingsBridge()->save());
        auto *panel=visual(window->contentItem(),"lyricsPanel");QVERIFY(panel);QVERIFY(panel->property("reduceMotion").toBool());
        const auto queue=f.shell->facade()->queue().items;QVERIFY(f.shell->settingsBridge()->setTheme(1));QCOMPARE(f.shell->facade()->queue().items,queue);
        for(const auto &size:{QSize(900,600),QSize(1180,760),QSize(1600,900)}){window->resize(size);QTest::qWait(30);QVERIFY(art->width()>0);QVERIFY(panel->width()>0);}
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void realConsentDialogDeclineAcceptAndSave(){
        Fixture f(2);QVERIFY(f.ok);auto *window=f.shell->window();const QVariant selected=QStringList{QString::number(f.a.id)};QVERIFY(QMetaObject::invokeMethod(window,"openLyricsTools",Q_ARG(QVariant,selected),Q_ARG(QVariant,QVariant(false))));
        QTRY_COMPARE(f.shell->lyricsModel()->count(),2);QVERIFY(f.shell->translationBridge()->classify(1));QCOMPARE(f.http.requestCount(),0);
        auto *button=visual(window->contentItem(),"translateAiButton");QVERIFY(button);QVERIFY(QMetaObject::invokeMethod(button,"click"));
        auto *dialog=window->findChild<QObject *>("translationConsent");QVERIFY(dialog);QTRY_VERIFY(dialog->property("visible").toBool());
        QCOMPARE(f.http.requestCount(),0);QVERIFY(QMetaObject::invokeMethod(dialog,"reject"));QTRY_COMPARE(f.shell->translationBridge()->state(),QString("Cancelled"));
        f.http.enqueue(completion(QString::fromUtf8(translationJson({{0,"你好月亮"},{1,"你好世界"}}))));
        QVERIFY(QMetaObject::invokeMethod(button,"click"));QTRY_VERIFY(dialog->property("visible").toBool());auto *accept=visual(window->contentItem(),"consentAcceptButton");QVERIFY(accept);
        QVERIFY(QMetaObject::invokeMethod(accept,"click"));QTRY_VERIFY(f.shell->translationBridge()->preview());QCOMPARE(f.http.requestCount(),1);
        QCOMPARE(LyricsDocumentReader::readFile(f.library.absolute(f.library.track(f.a.id).lyrics)).bytes,f.source);
        auto *save=visual(window->contentItem(),"saveTranslationButton");QVERIFY(save);
        // Preview completion may precede the modal exit transition on native Windows.
        QTRY_VERIFY(!dialog->property("visible").toBool());QTRY_VERIFY(save->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(save,"click"));
        QTRY_COMPARE(f.shell->translationBridge()->state(),QString("Saved"));QVERIFY(LyricsDocumentReader::readFile(f.library.absolute(f.library.track(f.a.id).lyrics)).bytes.contains("relaxer-bilingual"));
        QVERIFY2(f.shell->warnings().isEmpty(),qPrintable(f.shell->warnings().join('\n')));
    }
    void importVisibleProgressAndDeferredClose(){
        Fixture f;QVERIFY(f.ok);const auto folder=f.dir.filePath("新 歌曲");QVERIFY(QDir().mkpath(folder));
        for(int i=0;i<80;++i)QVERIFY(import_fixture::midi(folder+QString("/%1.mid").arg(i),i));
        auto *window=f.shell->window();
        const auto queue=f.shell->facade()->queue().items;auto *importPage=window->findChild<QObject *>("importDialog");QVERIFY(importPage);QVariant started;
        QTRY_VERIFY(f.shell->facade()->position()>500);const auto beforeImport=f.shell->facade()->position();
        const auto single=f.dir.filePath("single.mid");QVERIFY(import_fixture::midi(single,9999));const QVariant singleUrl=QVariant::fromValue(QList<QUrl>{QUrl::fromLocalFile(single)});
        QVERIFY(QMetaObject::invokeMethod(importPage,"submitFiles",Q_RETURN_ARG(QVariant,started),Q_ARG(QVariant,singleUrl)));QVERIFY(started.toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!f.shell->importBridge()->busy(),30000);QTRY_COMPARE(f.shell->libraryModel()->count(),3);
        QCOMPARE(f.shell->facade()->current().id,f.a.id);QVERIFY(f.shell->facade()->playing());QVERIFY(f.shell->facade()->position()>=beforeImport);QCOMPARE(f.shell->facade()->queue().items,queue);
        const QVariant urls=QVariant::fromValue(QList<QUrl>{QUrl::fromLocalFile(folder)});
        QVERIFY(QMetaObject::invokeMethod(importPage,"submitFiles",Q_RETURN_ARG(QVariant,started),Q_ARG(QVariant,urls)));QVERIFY(started.toBool());
        auto *progress=visual(window->contentItem(),"importProgress");QVERIFY(progress);QVERIFY(progress->isVisible());
        QVERIFY(!window->close());auto *confirm=window->findChild<QObject *>("closeTasksConfirmation");QVERIFY(confirm);QTRY_VERIFY(confirm->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(confirm,"reject"));QVERIFY(window->isVisible());QVERIFY(!f.shell->importBridge()->closing());
        QVERIFY(!window->close());QVERIFY(QMetaObject::invokeMethod(confirm,"accept"));QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(),30000);
        QVERIFY(f.shell->importCoordinator()->isIdle());QCOMPARE(f.shell->facade()->queue().items,queue);QVERIFY(f.shell->shutdown());
        QPointer<LyricsModel> model=f.shell->lyricsModel();QPointer<PlaybackFacade> player=f.shell->facade();f.shell.reset();QVERIFY(model.isNull());QVERIFY(player.isNull());
    }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);QmlC4C5UiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "qml-c4c5-ui-tests.moc"
