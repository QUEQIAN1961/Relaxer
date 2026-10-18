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
