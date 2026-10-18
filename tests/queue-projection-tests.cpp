#include "ui-bridge/qml-shell.h"
#include <QAbstractItemModelTester>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
using namespace relaxer;
namespace {
bool seed(Library &library, int count) {
    QFile audio(library.absolute("Music/fixture.wav"));
    if (!audio.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&audio); out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4); out << quint32(160036); out.writeRawData("WAVEfmt ",8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
    out.writeRawData("data",4); out << quint32(160000); const QByteArray pcm(160000,'\0'); out.writeRawData(pcm.data(),pcm.size());
    audio.close();
    const auto names=QSqlDatabase::connectionNames();
    auto db=QSqlDatabase::database(names.last()); if(!db.transaction()) return false;
    library.deferChanges(true);
    bool ok=true;
    for(int i=0;i<count;++i) {
        Track t; t.hash=QString("queue-%1").arg(i);t.file="Music/fixture.wav";
        t.title=QString("歌曲 日本語 Song %1").arg(i);t.artist="Artist";t.duration=10000;
        ok &= library.insert(t)>0;
    }
    ok &= db.commit(); library.deferChanges(false);return ok;
}
qsizetype payload(const Track &t) {
    qsizetype result=sizeof(Track);
    for(const auto &s:{t.hash,t.file,t.title,t.artist,t.album,t.cover,t.lyrics,t.coverOrigin,t.importedAt}) result+=s.capacity()*qsizetype(sizeof(QChar));
    return result;
}
}
class QueueProjectionTests : public QObject {
    Q_OBJECT
private slots:
    void selectiveLookupScale_data() {
        QTest::addColumn<int>("count");QTest::addColumn<int>("queueSize");
        for(int n:{0,1000,10000}) for(int q:{3,500})
            QTest::newRow(qPrintable(QString("%1-%2").arg(n).arg(q)))<<n<<q;
    }
    void selectiveLookupScale() {
        QFETCH(int,count);QFETCH(int,queueSize);QTemporaryDir dir;Library library(dir.path());QVERIFY(library.open());
        QVERIFY(seed(library,count));const auto connections=QSqlDatabase::connectionNames();
        const auto source=library.tracks();QVector<qint64> ids;
        for(int i=0;i<queueSize;++i)ids.append(count?source[(i*7)%count].id:123456+i);
        ids.append(-1);ids.append(999999);if(count)ids.append(ids.first());
        QVector<qint64> oldTimes,newTimes;qsizetype oldPayload=0,newPayload=0;int newRows=0;
        for(int repeat=-3;repeat<31;++repeat) {
            QElapsedTimer timer;timer.start();QHash<qint64,Track> old;
            const auto all=library.tracks();for(const auto &t:all)old.insert(t.id,t);
            QVector<Track> oldProjection;for(auto id:ids)oldProjection.append(old.value(id));
            const auto oldNs=timer.nsecsElapsed();timer.restart();const auto selected=library.tracksByIds(ids);
            QVector<Track> newProjection;for(auto id:ids)newProjection.append(selected.value(id));
            const auto newNs=timer.nsecsElapsed();
            QCOMPARE(newProjection.size(),ids.size());
            for(int i=0;i<ids.size();++i){QCOMPARE(newProjection[i].id,oldProjection[i].id);QCOMPARE(newProjection[i].title,oldProjection[i].title);}
            if(repeat>=0){oldTimes.append(oldNs);newTimes.append(newNs);}
            oldPayload=0;for(const auto &t:all)oldPayload+=payload(t);
            newPayload=0;for(const auto &t:selected)newPayload+=payload(t);
            newRows=selected.size();
        }
        std::sort(oldTimes.begin(),oldTimes.end());std::sort(newTimes.begin(),newTimes.end());
        QCOMPARE(QSqlDatabase::connectionNames(),connections);
        QCOMPARE(library.trackCount(),qint64(count));
        QVERIFY(newRows<=qMin(queueSize,count));
        qInfo().noquote()<<QJsonDocument(QJsonObject{{"tracks",count},{"requestedQueue",queueSize},{"repetitions",31},{"warmups",3},
            {"legacyMedianNs",double(oldTimes[15])},{"legacyMaxNs",double(oldTimes.last())},
            {"selectiveMedianNs",double(newTimes[15])},{"selectiveMaxNs",double(newTimes.last())},
            {"legacyMaterializedRows",count},{"selectiveMaterializedRows",newRows},
            {"legacyRetainedPayloadLowerBoundBytes",double(oldPayload)},{"selectiveRetainedPayloadLowerBoundBytes",double(newPayload)}}).toJson(QJsonDocument::Compact);
    }
    void queueOrderMissingAndProgress() {
        QTemporaryDir dir;Library library(dir.path());QVERIFY(library.open());QVERIFY(seed(library,1000));
        const auto all=library.tracks();const QVector<qint64> ids{all[8].id,all[2].id,all[8].id,all[600].id};
        QmlShell shell(&library,nullptr,true);auto *facade=shell.facade();auto *model=shell.queueModel();
        QAbstractItemModelTester tester(model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        facade->play(ids,ids.first());QTRY_COMPARE(model->ids(),facade->queue().items);
        for(int i=0;i<model->count();++i)QCOMPARE(model->data(model->index(i),TrackListModel::TrackId).toLongLong(),facade->queue().items[i]);
        QSignalSpy resets(model,&QAbstractItemModel::modelReset);
        QTRY_VERIFY(facade->position()>500);QCOMPARE(resets.size(),0);
        QVERIFY(library.remove({all[2].id}));QTRY_VERIFY(!facade->queue().items.contains(all[2].id));
        QTRY_COMPARE(model->ids(),facade->queue().items);QVERIFY(!model->ids().contains(all[2].id));
        QVERIFY(shell.shutdown());
    }
    void emptyAndDestroyedLibrary() {
        QTemporaryDir dir;auto library=std::make_unique<Library>(dir.path());QVERIFY(library->open());
        QVERIFY(library->tracksByIds({}).isEmpty());QVERIFY(library->tracksByIds({0,-1,44,44}).isEmpty());
        auto facade=std::make_unique<PlaybackFacade>(library.get(),nullptr,true);
        QueueListModel model(library.get(),facade.get());QCOMPARE(model.count(),0);QVERIFY(facade->shutdown());
        facade.reset();QCoreApplication::processEvents();QCOMPARE(model.count(),0);library.reset();QCOMPARE(model.count(),0);
    }
};
QTEST_MAIN(QueueProjectionTests)
#include "queue-projection-tests.moc"
