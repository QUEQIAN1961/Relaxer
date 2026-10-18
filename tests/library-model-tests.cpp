#include "ui-bridge/library-model.h"
#include <QAbstractItemModelTester>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
static bool seedRows(Library &library, int count) {
    auto db = QSqlDatabase::database(QSqlDatabase::connectionNames().last());
    if (!db.transaction()) return false;
    library.deferChanges(true);
    bool ok = true;
    for (int i = 0; i < count; ++i) {
        Track track; track.hash = QString::number(i); track.file = QString("Music/%1.wav").arg(i);
        track.title = QString("歌曲 日本語 Track %1").arg(i, 5, 10, QChar('0'));
        track.artist = i % 2 ? "Artist B" : "Artist A"; track.album = QString("Album %1").arg(i % 7);
        track.duration = (i + 1) * 1000; ok &= library.insert(track) > 0;
    }
    ok &= db.commit(); library.deferChanges(false); return ok;
}
class LibraryModelTests : public QObject {
    Q_OBJECT
  private slots:
    void countAndScale_data() {
        QTest::addColumn<int>("count");
        for (int n : {0, 1000, 10000}) QTest::newRow(qPrintable(QString::number(n))) << n;
    }
    void countAndScale() {
        QFETCH(int, count); QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto connections = QSqlDatabase::connectionNames();
        QVERIFY(seedRows(library, count)); QCOMPARE(library.trackCount(), count);
        QElapsedTimer clock; clock.start();
        for (int i = 0; i < 10; ++i) QCOMPARE(library.trackCount(), count);
        const auto countNs = clock.nsecsElapsed() / 10;
        clock.restart(); qsizetype retainedPayload = 0;
        for (int i = 0; i < 10; ++i) {
            const auto snapshot = library.tracks(); QCOMPARE(snapshot.size(), count);
            retainedPayload = snapshot.capacity() * qsizetype(sizeof(Track));
            for (const auto &t : snapshot)
                for (const auto &s : {t.hash,t.file,t.title,t.artist,t.album,t.cover,t.lyrics,t.coverOrigin,t.importedAt})
                    retainedPayload += s.capacity() * qsizetype(sizeof(QChar));
        }
        const auto legacyNs = clock.nsecsElapsed() / 10;
        clock.restart(); TrackListModel source(&library); TrackFilterModel view(&source);
        const auto modelNs = clock.nsecsElapsed(); QCOMPARE(view.count(), count);
        QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::QtTest);
        clock.restart(); view.setFilterText("Track 000"); const auto filterNs = clock.nsecsElapsed();
        QCOMPARE(view.count(), qMin(count, 100));
        view.setFilterText({}); clock.restart(); view.setSortKey(1); const auto sortNs = clock.nsecsElapsed();
        QCOMPARE(view.count(), count);
        QCOMPARE(QSqlDatabase::connectionNames(), connections);
        qInfo().noquote() << QJsonDocument(QJsonObject{{"tracks",count},{"countNs",double(countNs)},
            {"legacyMaterializeNs",double(legacyNs)},{"legacyRetainedPayloadLowerBoundBytes",double(retainedPayload)},
            {"countMaterializedTracks",0},{"modelLoadNs",double(modelNs)},
            {"filterNs",double(filterNs)},{"sortNs",double(sortNs)}}).toJson(QJsonDocument::Compact);
    }
    void rolesIdsAndNoWrites() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seedRows(library, 3));
        const auto id = library.tracks().first().id; QVERIFY(library.favorite(id, true));
        TrackListModel model(&library); QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(model.idAt(0), id); QCOMPARE(model.data(model.index(0), TrackListModel::TrackId).toLongLong(), id);
        QCOMPARE(model.data(model.index(0), TrackListModel::TrackId).metaType(), QMetaType::fromType<qint64>());
        QCOMPARE(model.data(model.index(0), TrackListModel::Title).toString(), library.track(id).title);
        QCOMPARE(model.data(model.index(0), TrackListModel::Artist).toString(), library.track(id).artist);
        QCOMPARE(model.data(model.index(0), TrackListModel::Album).toString(), library.track(id).album);
        QCOMPARE(model.data(model.index(0), TrackListModel::Duration).toLongLong(), library.track(id).duration);
        QVERIFY(model.data(model.index(0), TrackListModel::Favorite).toBool());
        QVERIFY(!model.setData(model.index(0), "unauthorized edit", TrackListModel::Title));
        QCOMPARE(library.track(id).title, model.data(model.index(0), TrackListModel::Title).toString());
        QVERIFY(!model.data(model.index(-1), TrackListModel::Title).isValid());
    }
    void filtersSortingAndStableSelection() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seedRows(library, 10));
        TrackListModel source(&library); TrackFilterModel view(&source);
        const auto selected = QString::number(source.idAt(0)); view.setSelectedId(selected);
        view.setSortKey(1); QCOMPARE(view.selectedId(), selected); QCOMPARE(view.selectedRow(), 9);
        view.setDescending(true); QCOMPARE(view.selectedRow(), 0);
        view.setFilterText("ARTIST A"); QCOMPARE(view.count(), 5); QCOMPARE(view.selectedId(), selected); QCOMPARE(view.selectedRow(), -1);
        view.setFilterText("不存在"); QCOMPARE(view.count(), 0); QCOMPARE(view.selectedId(), selected);
        view.setFilterText({}); QCOMPARE(view.selectedRow(), 0);
        QVERIFY(library.remove({selected.toLongLong()})); QTRY_COMPARE(view.count(), 9);
        QCOMPARE(view.selectedId(), QString("0")); QCOMPARE(view.selectedRow(), -1);
        view.setSortKey(4); QCOMPARE(view.data(view.index(0, 0), TrackListModel::Duration).toLongLong(), 9000);
    }
    void coalescedChangesAndDestruction() {
        QTemporaryDir dir; auto library = std::make_unique<Library>(dir.path()); QVERIFY(library->open());
        TrackListModel source(library.get()); TrackFilterModel view(&source);
        QSignalSpy resets(&source, &QAbstractItemModel::modelReset);
        QVERIFY(seedRows(*library, 100)); QTRY_COMPARE(view.count(), 100); QCOMPARE(resets.size(), 1);
        // Settings/progress persistence does not emit Library::changed.
        QVERIFY(library->setSetting("volume", 17)); QTest::qWait(10); QCOMPARE(resets.size(), 1);
        library.reset(); QCOMPARE(view.count(), 0); QCoreApplication::processEvents();
        Library other(dir.filePath("unused")); QVERIFY(other.open());
        auto transient = std::make_unique<TrackListModel>(&other);
        // Separate source lifetime is covered without allowing stale row pointers.
        TrackFilterModel detached(transient.get());
        transient.reset(); QCOMPARE(detached.rowCount(), 0); QCOMPARE(detached.selectedRow(), -1);
    }
    void playlistOrderAndNotifications() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open()); QVERIFY(seedRows(library, 3));
        PlaylistListModel lists(&library); TrackListModel tracks(&library);
        const auto ids = tracks.ids(); const auto list = library.createPlaylist("测试 Playlist"); QVERIFY(list);
        QVERIFY(library.addToPlaylist(list, {ids[2],ids[0]})); QTRY_COMPARE(lists.count(), 1);
        QVERIFY(lists.contains(list)); tracks.setPlaylist(list); QCOMPARE(tracks.ids(), (QVector<qint64>{ids[2],ids[0]}));
        QVERIFY(library.reorder(list, {ids[0],ids[2]})); QTRY_COMPARE(tracks.ids(), (QVector<qint64>{ids[0],ids[2]}));
        QVERIFY(library.deletePlaylist(list)); QTRY_COMPARE(lists.count(), 0); QTRY_COMPARE(tracks.rowCount(), 0);
    }
};
QTEST_GUILESS_MAIN(LibraryModelTests)
#include "library-model-tests.moc"
