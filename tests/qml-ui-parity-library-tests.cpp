#include "ui-bridge/library-bridge.h"
#include <QAbstractItemModelTester>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
namespace {
qint64 addTrack(Library &library, const QString &title, const QString &artist, const QString &album, qint64 duration = 1000) {
    Track track; track.hash = title; track.title = title; track.artist = artist; track.album = album;
    track.file = "Music/" + title + ".wav"; track.duration = duration;
    QFile file(library.absolute(track.file));
    if (!file.open(QIODevice::WriteOnly) || file.write("fixture") != 7) return 0;
    file.close(); return library.insert(track);
}
struct Projection {
    PlaybackFacade facade;
    TrackListModel source;
    TrackFilterModel view;
    PlaylistListModel lists;
    LibraryBridge bridge;
    explicit Projection(Library &library) : facade(&library, nullptr, true), source(&library), view(&source), lists(&library),
        bridge(&library, &facade, &source, &view, &lists) {}
};
}
class UiParityLibraryTests : public QObject {
    Q_OBJECT
private slots:
    void playlistSortingCyclesAndPersistsIndependently() {
        QTemporaryDir dir; QString first,second,cloud; qint64 a=0,b=0,c=0;
        {
            Library library(dir.path()); QVERIFY(library.open());
            a=addTrack(library,"C","First","One",3000);
            b=addTrack(library,"A","Second","Two",1000);
            c=addTrack(library,"B","Third","Three",2000); QVERIFY(a&&b&&c);
            Projection p(library);
            first=p.bridge.createPlaylist("First"); second=p.bridge.createPlaylist("Second");
            QVERIFY(first!="0"&&second!="0");
            QVERIFY(p.bridge.addTracksToPlaylist(first,{QString::number(c),QString::number(a),QString::number(b)}));
            QVERIFY(p.bridge.addTracksToPlaylist(second,{QString::number(c),QString::number(b)}));
            QVERIFY(p.bridge.openPlaylist(first)); QCoreApplication::processEvents();
            const QVector<qint64> manual{c,a,b}; QCOMPARE(p.view.ids(),manual);
            QSignalSpy resets(&p.source,&QAbstractItemModel::modelReset);
            QSignalSpy changed(&p.view,&TrackFilterModel::sortChanged);
            p.view.cycleSort(1); QCOMPARE(p.view.ids(),(QVector<qint64>{b,c,a}));
            QCOMPARE(changed.size(),1); QCOMPARE(resets.size(),0);
            auto saved=library.setting("qml.playlistSorting").toObject().value(first).toObject();
            QCOMPARE(saved.value("key").toInt(),1); QVERIFY(!saved.value("descending").toBool());
            p.view.cycleSort(1); QCOMPARE(p.view.ids(),(QVector<qint64>{a,c,b}));
            p.view.cycleSort(1); QCOMPARE(p.view.sortKey(),0); QVERIFY(!p.view.descending()); QCOMPARE(p.view.ids(),manual);
            QCOMPARE(changed.size(),3); QCOMPARE(resets.size(),0);
            p.view.cycleSort(1); p.view.cycleSort(1);
            QVERIFY(p.bridge.openPlaylist(second)); QCOMPARE(p.view.sortKey(),0);
            p.view.cycleSort(4); QCOMPARE(p.view.ids(),(QVector<qint64>{b,c}));
            QVERIFY(p.bridge.openPlaylist(first)); QCOMPARE(p.view.sortKey(),1); QVERIFY(p.view.descending());
            QCOMPARE(p.view.ids(),(QVector<qint64>{a,c,b}));
            QVERIFY(p.bridge.openPlaylist(second)); QCOMPARE(p.view.sortKey(),4); QVERIFY(!p.view.descending());
            cloud=p.bridge.pinCloudPlaylist({{"provider","netease"},{"id","999"},{"name","Cloud"},{"count",2}});
            QVERIFY(cloud!="0"); QVERIFY(p.bridge.openPlaylist(cloud));
            p.bridge.setCloudSongs("netease",{{"id","999"},{"name","Cloud"},{"count",2}},
                {QVariantMap{{"id","91"},{"title","Z"}},QVariantMap{{"id","92"},{"title","A"}}});
            p.view.cycleSort(1); QCOMPARE(p.view.data(p.view.index(0,0),TrackListModel::Title).toString(),QString("A"));
            p.view.cycleSort(1); QCOMPARE(p.view.data(p.view.index(0,0),TrackListModel::Title).toString(),QString("Z"));
            QVERIFY(p.bridge.openPlaylist("0")); p.view.cycleSort(2);
        }
        {
            Library library(dir.path()); QVERIFY(library.open()); Projection p(library);
            QCOMPARE(p.view.sortKey(),2); QVERIFY(!p.view.descending());
            QVERIFY(p.bridge.openPlaylist(first)); QCOMPARE(p.view.sortKey(),1); QVERIFY(p.view.descending());
            QCOMPARE(p.view.ids(),(QVector<qint64>{a,c,b}));
            QVERIFY(p.bridge.openPlaylist(second)); QCOMPARE(p.view.sortKey(),4); QVERIFY(!p.view.descending());
            QCOMPARE(p.view.ids(),(QVector<qint64>{b,c}));
            QVERIFY(p.bridge.openPlaylist(cloud)); QCOMPARE(p.view.sortKey(),1); QVERIFY(p.view.descending());
            p.bridge.setCloudSongs("netease",{{"id","999"},{"name","Cloud"},{"count",2}},
                {QVariantMap{{"id","91"},{"title","Z"}},QVariantMap{{"id","92"},{"title","A"}}});
            QCOMPARE(p.view.data(p.view.index(0,0),TrackListModel::Title).toString(),QString("Z"));
            p.view.cycleSort(1); QCOMPARE(p.view.sortKey(),0);
            QCOMPARE(p.view.data(p.view.index(0,0),TrackListModel::Title).toString(),QString("Z"));
            QVERIFY(p.bridge.deletePlaylist(second));
            QVERIFY(!library.setting("qml.playlistSorting").toObject().contains(second));
        }
    }
    void recentAndFrequentUseExistingSessions() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto a = addTrack(library, "A", "Artist A", "One");
        const auto b = addTrack(library, "B", "Artist B", "Two"); QVERIFY(a && b);
        auto first = library.startSession(a, 1000); QVERIFY(library.saveSession(first, 900, 80));
        auto second = library.startSession(b, 1000); QVERIFY(library.saveSession(second, 100, 80));
        Projection p(library); QAbstractItemModelTester tester(&p.view, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(p.bridge.openPlaylist("-2")); QCOMPARE(p.source.ids(), (QVector<qint64>{b, a}));
        QCOMPARE(p.bridge.playlistName(), QString("最近播放"));
        QVERIFY(p.bridge.openPlaylist("-3")); QCOMPARE(p.source.idAt(0), a); QCOMPARE(p.bridge.playlistName(), QString("最长播放"));
        QVERIFY(p.bridge.openPlaylist("0")); p.view.setSortKey(5); p.view.setDescending(true);
        QCOMPARE(p.view.idAt(0), QString::number(a)); QCOMPARE(p.view.data(p.view.index(0, 0), TrackListModel::Plays).toLongLong(), 1);
        p.view.setSortKey(7); QCOMPARE(p.view.idAt(0), QString::number(a));
        const auto details = p.bridge.trackDetails(QString::number(a));
        QCOMPARE(details["plays"].toLongLong(), 1); QCOMPARE(details["fileSize"].toLongLong(), 7);
        QVERIFY(!details["importedAt"].toString().isEmpty()); QVERIFY(!p.bridge.openPlaylist("-4"));
    }
    void artistAndAlbumFiltersPreserveStableSemantics() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        QVERIFY(addTrack(library, "Shared", "Alice / Bob", "Shared album", 2000));
        QVERIFY(addTrack(library, "Rock", "AC/DC", "Rock album", 3000));
        QVERIFY(addTrack(library, "Solo", "Alice", "Solo album", 4000));
        Projection p(library);
        QVERIFY(p.view.artists().contains("Alice / Bob")); QVERIFY(p.view.artists().contains("Alice"));
        QVERIFY(p.view.artists().contains("Bob")); QVERIFY(p.view.artists().contains("AC/DC"));
        QVERIFY(!p.view.artists().contains("AC"));
        p.view.setArtistFilter("bob"); QCOMPARE(p.view.count(), 1); QCOMPARE(p.view.totalDuration(), 2000);
        QCOMPARE(p.view.albums(), QStringList{"Shared album"});
        p.view.setAlbumFilter("Shared album"); p.view.setArtistFilter("AC/DC");
        QCOMPARE(p.view.albumFilter(), QString()); QCOMPARE(p.view.count(), 1);
        p.view.setFilterText("missing"); QCOMPARE(p.view.count(), 0);
        p.view.clearFilters(); QCOMPARE(p.view.count(), 3); QCOMPARE(p.view.totalDuration(), 9000);
        p.view.setArtistFilter("Alice"); QCOMPARE(p.view.count(), 2);
        QVERIFY(p.bridge.openPlaylist("-1")); QCOMPARE(p.view.artistFilter(), QString()); QCOMPARE(p.view.filterText(), QString());
    }
    void bulkPlaylistCommandsPreserveOrderAndFiles() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto a = addTrack(library, "A", "Artist", "Album"), b = addTrack(library, "B", "Artist", "Album"),
                   c = addTrack(library, "C", "Artist", "Album"); QVERIFY(a && b && c);
        Projection p(library); const QStringList selected{QString::number(b), QString::number(c)};
        const auto list = p.bridge.createPlaylist("Playlist"); QVERIFY(list != "0");
        QVERIFY(p.bridge.addTracksToPlaylist(list, {QString::number(a), QString::number(b), QString::number(c), QString::number(c)}));
        QVERIFY(p.bridge.openPlaylist(list)); QCOMPARE(p.source.ids(), (QVector<qint64>{a, b, c}));
        QVERIFY(p.bridge.moveTracksInPlaylist(selected, -1)); QTRY_COMPARE(p.source.ids(), (QVector<qint64>{b, c, a}));
        QVERIFY(p.bridge.applyFavorite(selected, true)); QTRY_VERIFY(library.track(b).favorite && library.track(c).favorite);
        QVERIFY(!p.bridge.applyFavorite({QString::number(a), "missing"}, true)); QVERIFY(!library.track(a).favorite);
        QVERIFY(p.bridge.removeTracksFromPlaylist({QString::number(b)})); QTRY_COMPARE(p.source.ids(), (QVector<qint64>{c, a}));
        QCOMPARE(library.trackCount(), 3); QVERIFY(QFile::exists(library.absolute("Music/B.wav")));
        QVERIFY(p.bridge.removeTracks({QString::number(b)})); QCOMPARE(library.trackCount(), 2);
        QVERIFY(QFile::exists(library.absolute("Music/B.wav")));
        QVERIFY(p.bridge.removeTracks({QString::number(c)}, true)); QCOMPARE(library.trackCount(), 1);
        QVERIFY(!QFile::exists(library.absolute("Music/C.wav")));
    }
    void profileAndPlaylistMetadataUseExistingPersistence() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto a = addTrack(library, "A", "Artist", "Album"); QVERIFY(a);
        Projection p(library); QVERIFY(p.bridge.saveProfile("Listener", "个人简介"));
        QCOMPARE(library.setting("nickname").toString(), QString("Listener"));
        QCOMPARE(p.bridge.profile()["description"].toString(), QString("个人简介"));
        QVERIFY(!p.bridge.saveProfile(" ", "Invalid")); QCOMPARE(p.bridge.profile()["nickname"].toString(), QString("Listener"));
        QVERIFY(!p.bridge.saveProfile("Listener", QString(1001, QChar('x'))));
        QImage image(24, 24, QImage::Format_ARGB32); image.fill(Qt::red);
        const auto imagePath = dir.filePath("fixture-cover.png"); QVERIFY(image.save(imagePath));
        QVERIFY(p.bridge.setProfileAvatar(QUrl::fromLocalFile(imagePath)));
        QVERIFY(QFile::exists(p.bridge.profile()["avatar"].toUrl().toLocalFile()));
        const auto list = p.bridge.createPlaylist("Playlist"); QVERIFY(p.bridge.addToPlaylist(list, QString::number(a)));
        QVERIFY(p.bridge.setPlaylistCover(list, QUrl::fromLocalFile(imagePath)));
        QVERIFY(p.bridge.setPlaylistTags(list, {"ACG", "流行"})); QVERIFY(p.bridge.setPlaylistDescription(list, "我的歌单"));
        QVERIFY(p.bridge.openPlaylist(list));
        QCOMPARE(p.bridge.playlistDetails()["tags"].toStringList(), (QStringList{"流行", "ACG"}));
        QCOMPARE(p.bridge.playlistDetails()["description"].toString(), QString("我的歌单"));
        QVERIFY(p.bridge.playlistDetails()["cover"].toUrl().isLocalFile());
        QTRY_COMPARE(p.lists.count(), 1); QCOMPARE(p.lists.data(p.lists.index(0), Qt::UserRole + 7).toString(), QString("我的歌单"));
        QVERIFY(!p.bridge.setPlaylistTags(list, {"unknown"}));
    }
    void listeningUpdatesStatisticsWithoutResettingOrdinaryList() {
        QTemporaryDir dir; Library library(dir.path()); QVERIFY(library.open());
        const auto a = addTrack(library, "A", "Artist", "Album"); QVERIFY(a);
        Projection p(library); QSignalSpy resets(&p.source, &QAbstractItemModel::modelReset);
        auto session = library.startSession(a, 1000); QVERIFY(library.saveSession(session, 900, 80));
        p.source.refreshStatistics(a); p.bridge.refreshStatistics();
        QCOMPARE(resets.size(), 0); QCOMPARE(p.source.data(p.source.index(0), TrackListModel::Plays).toLongLong(), 1);
        QCOMPARE(p.bridge.statistics()["trackCount"].toLongLong(), 1);
        QCOMPARE(p.bridge.statistics()["totalPlays"].toLongLong(), 1); QCOMPARE(p.bridge.statistics()["totalHeard"].toLongLong(), 900);
    }
};
int main(int argc, char **argv) { QGuiApplication app(argc, argv); UiParityLibraryTests tests; return QTest::qExec(&tests, argc, argv); }
#include "qml-ui-parity-library-tests.moc"
