#include "ui-bridge/qml-shell.h"
#include <QAbstractItemModelTester>
#include <QDataStream>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
namespace {
bool audioFixture(Library &library) {
    QFile file(library.absolute("Music/fixture.wav"));
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&file); out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4); out << quint32(160036); out.writeRawData("WAVEfmt ",8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
    out.writeRawData("data",4); out << quint32(160000); const QByteArray pcm(160000,'\0'); out.writeRawData(pcm.data(),pcm.size());
    return out.status() == QDataStream::Ok;
}
bool seed(Library &library, int count) {
    if (!audioFixture(library)) return false;
    auto db = QSqlDatabase::database(QSqlDatabase::connectionNames().last());
    if (!db.transaction()) return false;
    library.deferChanges(true);
    for (int i=0;i<count;++i) {
        Track t; t.hash=QString("c2c3-%1").arg(i); t.file="Music/fixture.wav"; t.title=QString("歌曲 日本語 Song %1").arg(i,5,10,QChar('0'));
        t.artist=i%2?"Artist B":"Artist A"; t.album="Fixture album"; t.duration=10000;
        if (!library.insert(t)) { db.rollback(); library.deferChanges(false); return false; }
    }
    const bool ok=db.commit(); library.deferChanges(false); return ok;
}
QQuickItem *visual(QQuickItem *item,const QString &name) {
    if (item->objectName()==name) return item;
    for(auto *child:item->childItems()) if(auto *found=visual(child,name)) return found;
    return nullptr;
}
int delegates(QQuickItem *item) {
    int count=item->objectName()=="trackDelegate"?1:0;
    for(auto *child:item->childItems()) count+=delegates(child);
    return count;
}
}
class C2C3Tests : public QObject {
    Q_OBJECT
  private slots:
    void queueProjectionHasOneAuthorityAndNoProgressResets() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seed(library,4));
        QmlShell shell(&library,nullptr,true); QVERIFY(shell.load());
        auto *queue=shell.queueModel(); QAbstractItemModelTester tester(queue,QAbstractItemModelTester::FailureReportingMode::QtTest);
        shell.libraryBridge()->playAll(); QTRY_COMPARE(queue->ids(),shell.facade()->queue().items); QCOMPARE(queue->count(),4);
        QSignalSpy resets(queue,&QAbstractItemModel::modelReset);
        QTest::qWait(600); QCOMPARE(resets.size(),0);
        const auto id=queue->ids()[1]; shell.playbackBridge()->playQueued(QString::number(id));
        QTRY_COMPARE(shell.facade()->current().id,id); QCoreApplication::processEvents(); QCOMPARE(resets.size(),0);
        // An external existing Library command is reconciled through facade,
        // never by mutating a writable queue inside the model.
        QVERIFY(library.remove({id})); QTRY_VERIFY(!shell.facade()->queue().items.contains(id));
        QTRY_COMPARE(queue->ids(),shell.facade()->queue().items); QCOMPARE(queue->count(),3);
        const auto other=queue->ids().first(); QVERIFY(shell.libraryBridge()->toggleFavorite(QString::number(other)));
        QTRY_VERIFY(queue->data(queue->index(queue->indexForKey(QString::number(other)),0),TrackListModel::Favorite).toBool());
        QVERIFY(shell.shutdown()); QVERIFY(shell.warnings().isEmpty());
    }
    void progressDraggingDoesNotFightPlaybackOrSeekNewTrack() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seed(library,3));
        QmlShell shell(&library,nullptr,true); QVERIFY(shell.load()); shell.libraryBridge()->playAll();
        QTRY_VERIFY(shell.playbackBridge()->seekable()); shell.playbackBridge()->pause(); QTest::qWait(50);
        auto *slider=visual(shell.window()->contentItem(),"playbackProgress"); QVERIFY(slider);
        const auto start=slider->mapToScene(QPointF(slider->width()*.3,slider->height()/2)).toPoint();
        const auto moved=slider->mapToScene(QPointF(slider->width()*.6,slider->height()/2)).toPoint();
        QTest::mousePress(shell.window(),Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(shell.window(),moved,20); QTRY_VERIFY(slider->property("pressed").toBool());
        const auto held=slider->property("value").toDouble(); QTest::qWait(300);
        QVERIFY(qAbs(slider->property("value").toDouble()-held)<1);
        QTest::mouseRelease(shell.window(),Qt::LeftButton,Qt::NoModifier,moved);
        QTRY_VERIFY(qAbs(shell.facade()->position()-qint64(held))<200);
        QTest::mousePress(shell.window(),Qt::LeftButton,Qt::NoModifier,start);
        QTest::mouseMove(shell.window(),moved,20); const auto previous=shell.facade()->current().id;
        shell.playbackBridge()->setMode(0); shell.playbackBridge()->next(); QTRY_VERIFY(shell.facade()->current().id!=previous);
        QTest::mouseRelease(shell.window(),Qt::LeftButton,Qt::NoModifier,moved);
        QVERIFY(shell.facade()->position()<1500); QVERIFY(shell.warnings().isEmpty());
    }
    void queueSessionRestoresWithExistingFacadeSemantics() {
        QTemporaryDir dir; QVector<qint64> ids; qint64 current=0;
        {
            Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seed(library,3));
            QmlShell shell(&library,nullptr,true); QVERIFY(shell.load()); shell.libraryBridge()->playAll();
            QTRY_VERIFY(shell.playbackBridge()->seekable()); shell.playbackBridge()->pause();
            ids=shell.facade()->queue().items; current=ids[1]; shell.playbackBridge()->playQueued(QString::number(current));
            QTRY_VERIFY(shell.playbackBridge()->seekable()); shell.playbackBridge()->pause(); shell.playbackBridge()->seek(2400);
            QTRY_VERIFY(qAbs(shell.facade()->position()-2400)<150); shell.playbackBridge()->setMode(3); QVERIFY(shell.shutdown());
        }
        {
            Library library(dir.path()); QVERIFY(library.open()); PlaybackFacade widgetsFacade(&library,nullptr,true);
            widgetsFacade.restore(); QCOMPARE(widgetsFacade.queue().items,ids); QCOMPARE(widgetsFacade.current().id,current);
            QCOMPARE(widgetsFacade.mode(),Mode::Shuffle); QTRY_VERIFY(widgetsFacade.seekable());
            QTRY_VERIFY(qAbs(widgetsFacade.position()-2400)<200); QVERIFY(!widgetsFacade.playing());
            widgetsFacade.setMode(Mode::ListLoop); QVERIFY(widgetsFacade.shutdown());
        }
        {
            Library library(dir.path()); QVERIFY(library.open()); QmlShell shell(&library,nullptr,true);
            QTRY_COMPARE(shell.queueModel()->ids(),ids); QCOMPARE(shell.facade()->current().id,current); QCOMPARE(shell.playbackBridge()->mode(),1);
            QCOMPARE(QSqlDatabase::connectionNames().size(),1); QCOMPARE(shell.findChildren<Playback *>().size(),1);
            QVERIFY(shell.shutdown());
        }
        QVERIFY(QSqlDatabase::connectionNames().isEmpty());
    }
    void playbackCommandsSeekAndModes() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seed(library,3));
        QmlShell shell(&library,nullptr,true); QVERIFY(shell.load());
        auto *bridge=shell.playbackBridge(); const auto ids=shell.libraryModel()->ids();
        shell.libraryBridge()->play(QString::number(ids[0])); QTRY_VERIFY(bridge->playing()); QTRY_VERIFY(bridge->seekable());
        QVERIFY(bridge->canPlay()); QCOMPARE(bridge->trackId(),QString::number(ids[0]));
        bridge->pause(); QTRY_VERIFY(!bridge->playing()); bridge->pause(); QVERIFY(!shell.facade()->playing());
        bridge->seek(3500); QTRY_VERIFY(qAbs(shell.facade()->position()-3500)<150);
        QCOMPARE(bridge->position(),shell.facade()->position()); QVERIFY(!bridge->playing());
        bridge->play(); QTRY_VERIFY(bridge->playing()); bridge->play(); QVERIFY(shell.facade()->playing());
        bridge->setMode(0); QCOMPARE(shell.facade()->mode(),Mode::Sequential);
        bridge->next(); QTRY_COMPARE(bridge->trackId(),QString::number(ids[1]));
        QVERIFY(bridge->position()<1000); bridge->previous(); QTRY_COMPARE(bridge->trackId(),QString::number(ids[0]));
        for(int mode=0;mode<4;++mode) { bridge->setMode(mode); QCOMPARE(bridge->mode(),mode); QCOMPARE(int(shell.facade()->queue().mode),mode); }
        bridge->setMode(-1); QCOMPARE(bridge->mode(),3); bridge->setMode(4); QCOMPARE(bridge->mode(),3);
        bridge->playQueued(QString::number(ids[2])); QTRY_COMPARE(bridge->trackId(),QString::number(ids[2]));
        QCOMPARE(shell.facade()->queue().items,ids);
        bridge->removeFromQueue(QString::number(ids[1])); QVERIFY(!shell.facade()->queue().items.contains(ids[1]));
        QCOMPARE(library.trackCount(),qint64(3)); QVERIFY(shell.shutdown());
        bridge->setVolume(10); bridge->next(); bridge->seek(10); QVERIFY(!bridge->available());
        QVERIFY(shell.warnings().isEmpty());
    }
    void volumeMuteAndWindowRecreation() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        {
            QmlShell shell(&library,nullptr,true); auto *bridge=shell.playbackBridge();
            bridge->setVolume(37); QCOMPARE(shell.facade()->volume(),37); QVERIFY(!bridge->muted());
            bridge->toggleMute(); QCOMPARE(shell.facade()->volume(),0); QVERIFY(bridge->muted());
            bridge->toggleMute(); QCOMPARE(shell.facade()->volume(),37);
            bridge->setVolume(200); QCOMPARE(bridge->volume(),100); bridge->setVolume(-10); QCOMPARE(bridge->volume(),0);
            bridge->setVolume(37); bridge->toggleMute(); QVERIFY(shell.shutdown());
        }
        QCOMPARE(library.setting("volume").toInt(),0);
        {
            QmlShell rebuilt(&library,nullptr,true); QCOMPARE(rebuilt.facade()->volume(),0);
            rebuilt.playbackBridge()->toggleMute(); QCOMPARE(rebuilt.facade()->volume(),37);
            QVERIFY(rebuilt.shutdown());
        }
        QCOMPARE(library.setting("volume").toInt(),37);
    }
    void commandsUseRealLibraryAndFacade() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seed(library,3));
        const auto connections=QSqlDatabase::connectionNames(); QmlShell shell(&library,nullptr,true); QVERIFY2(shell.load(),qPrintable(shell.warnings().join('\n')));
        auto *view=shell.libraryModel(); auto *bridge=shell.libraryBridge();
        QCOMPARE(view->count(),3); const auto first=view->idAt(0),second=view->idAt(1);
        view->setSelectedId(first); bridge->play(second); QTRY_COMPARE(shell.facade()->current().id,second.toLongLong());
        QCOMPARE(view->selectedId(),first); QCOMPARE(shell.facade()->queue().items,view->ids());
        QVERIFY(bridge->toggleFavorite(first)); QTRY_VERIFY(library.track(first.toLongLong()).favorite);
        const auto list=bridge->createPlaylist("测试 Playlist"); QVERIFY(list!="0");
        // Commands read the Library, so they need not wait for model refresh.
        QVERIFY(bridge->addToPlaylist(list,second)); QVERIFY(bridge->addToPlaylist(list,first));
        QVERIFY(bridge->openPlaylist(list)); QCOMPARE(view->ids(),(QVector<qint64>{second.toLongLong(),first.toLongLong()}));
        QVERIFY(bridge->moveInPlaylist(first,-1)); QTRY_COMPARE(view->idAt(0),first);
        view->setFilterText("missing"); QVERIFY(!bridge->moveInPlaylist(first,1)); view->setFilterText({});
        bridge->playAll(); QCOMPARE(shell.facade()->queue().items,view->ids());
        QVERIFY(bridge->renamePlaylist(list,"Renamed")); QTRY_COMPARE(bridge->playlistName(),QString("Renamed"));
        QVERIFY(bridge->removeFromPlaylist(first)); QTRY_COMPARE(view->count(),1);
        QVERIFY(bridge->deletePlaylist(list)); QTRY_COMPARE(bridge->playlistId(),QString("0")); QTRY_COMPARE(view->count(),3);
        QVERIFY(!bridge->openPlaylist("9999")); QVERIFY(!bridge->openPlaylist("invalid"));
        view->setSelectedId(first); QVERIFY(bridge->removeTrack(first)); QTRY_COMPARE(view->count(),2);
        QCOMPARE(view->selectedId(),QString("0")); QVERIFY(!shell.facade()->queue().items.contains(first.toLongLong()));
        QVERIFY(QFile::exists(library.absolute("Music/fixture.wav"))); QCOMPARE(QSqlDatabase::connectionNames(),connections);
        QVERIFY(shell.shutdown()); QVERIFY(shell.warnings().isEmpty());
    }
    void realDelegatesSelectionAndLazyInstantiation() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seed(library,1000));
        QmlShell shell(&library,nullptr,true); QVERIFY2(shell.load(),qPrintable(shell.warnings().join('\n')));
        QTest::qWait(100); auto *root=shell.window()->contentItem(); auto *list=visual(root,"trackListView"); QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(),1000); QTRY_VERIFY(delegates(root)>0); QVERIFY(delegates(root)<100);
        auto *row=visual(root,"trackDelegate"); QVERIFY(row);
        const auto point=row->mapToScene(QPointF(row->width()/2,row->height()/2)).toPoint();
        QTest::mouseClick(shell.window(),Qt::LeftButton,Qt::NoModifier,point);
        QTRY_VERIFY(shell.libraryModel()->selectedId()!="0"); QCOMPARE(shell.facade()->current().id,qint64(0));
        QTest::mouseDClick(shell.window(),Qt::LeftButton,Qt::NoModifier,point);
        QTRY_COMPARE(shell.playbackBridge()->trackId(),shell.libraryModel()->selectedId());
        auto *search=visual(root,"librarySearch"); QVERIFY(search); search->setProperty("text","ARTIST A");
        QVERIFY(QMetaObject::invokeMethod(search,"textEdited")); QTRY_COMPARE(shell.libraryModel()->count(),500);
        const auto selected=shell.libraryModel()->selectedId(); shell.appBridge()->navigate(1); shell.appBridge()->navigate(0);
        QCOMPARE(shell.libraryModel()->selectedId(),selected); QCOMPARE(shell.libraryModel()->count(),500);
        QVERIFY(shell.warnings().isEmpty());
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    C2C3Tests tests; return QTest::qExec(&tests,argc,argv);
}
#include "qml-c2c3-tests.moc"
