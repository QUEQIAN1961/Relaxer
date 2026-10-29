#include <QJsonObject>
#include "importer.h"
#include "library.h"
#include "lyrics.h"
#include "lyric-text.h"
#include "playback.h"
#include "audio-device-fixture.h"
#include <QAudioDevice>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QMediaDevices>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace relaxer;
class Tests : public QObject {
    Q_OBJECT
    static bool runFfmpeg(const QStringList &arguments) {
        QProcess process;
        process.start(toolPath("ffmpeg"), QStringList{"-v", "error", "-y"} + arguments);
        const bool ok = process.waitForFinished(20000) && process.exitCode() == 0;
        if (!ok)
            qWarning().noquote() << process.readAllStandardError();
        return ok;
    }
    static bool makeColor(const QString &path, const QColor &color) {
        QImage image(128, 128, QImage::Format_RGB32);
        image.fill(color);
        return image.save(path);
    }
    static bool artworkMatches(const QString &root, const Track &track, const QColor &expected) {
        const QImage image(root + "/" + track.cover);
        if (image.isNull())
            return false;
        const auto color = image.pixelColor(image.width() / 2, image.height() / 2);
        return qAbs(color.red() - expected.red()) < 10 && qAbs(color.green() - expected.green()) < 10 &&
               qAbs(color.blue() - expected.blue()) < 10;
    }
  private slots:
    // Characterization contracts for Phase B; exercise public APIs across owners.
    void foundationPlaylistAndSettingsSurviveReopen() {
        QTemporaryDir dir;const auto root=dir.filePath("data");QVector<qint64> ids;qint64 playlist=0;
        const QJsonObject extension{{"enabled",true},{"names",QJsonArray{"one","two"}},{"nested",QJsonObject{{"value",7}}}};
        {
            Library library(root);QVERIFY(library.open());
            for(int i=0;i<3;++i){Track t;t.hash=QString("foundation-%1").arg(i);t.file=QString("Music/fixture-%1.wav").arg(i);t.title=QString("track %1").arg(i);t.duration=10000;const auto id=library.insert(t);QVERIFY(id>0);ids<<id;}
            playlist=library.createPlaylist("持久化基线");QVERIFY(playlist>0);
            QVERIFY(library.addToPlaylist(playlist,{ids[0],ids[1],ids[2],ids[0]}));
            QVERIFY(library.reorder(playlist,{ids[2],ids[0],ids[1]}));QVERIFY(library.favorite(ids[1],true));
            QVERIFY(library.playlistTags(playlist,{"流行","民谣"}));QVERIFY(library.playlistDescription(playlist,"跨重启保留\n歌单信息"));
            QVERIFY(library.setSettings({{"theme",2},{"volume",37},{"translateLyrics",false},{"lyricBrowserProvider","baidu"},{"foundationUnknownSetting",extension}}));
        }
        Library reopened(root);QVERIFY(reopened.open());const auto tracks=reopened.tracks(playlist);
        QCOMPARE(tracks.size(),3);QCOMPARE(tracks[0].id,ids[2]);QCOMPARE(tracks[1].id,ids[0]);QCOMPARE(tracks[2].id,ids[1]);QVERIFY(tracks[2].favorite);
        const auto lists=reopened.playlists();QCOMPARE(lists.size(),1);QCOMPARE(lists[0].id,playlist);QCOMPARE(lists[0].count,3);
        QCOMPARE(lists[0].tags,QStringList({"流行","民谣"}));QCOMPARE(lists[0].description,QString("跨重启保留\n歌单信息"));
        QCOMPARE(reopened.setting("theme").toInt(),2);QCOMPARE(reopened.setting("volume").toInt(),37);
        QCOMPARE(reopened.setting("translateLyrics").toBool(),false);QCOMPARE(reopened.setting("lyricBrowserProvider").toString(),QString("baidu"));
        QCOMPARE(reopened.setting("foundationUnknownSetting").toObject(),extension);
    }
    void foundationBilingualLyricsAndOffsetsSurviveReopen() {
        QTemporaryDir dir;const auto root=dir.filePath("data");qint64 id=0;QByteArray stored;
        const QByteArray source("[offset:120]\n[00:01.00]first line\n[00:02.00]second line\n");
        {
            Library library(root);QVERIFY(library.open());Track t;t.hash="foundation-lyrics";t.file="Music/fixture.wav";id=library.insert(t);QVERIFY(id>0);
            QVERIFY(library.setLyricOffset(id,450));QVERIFY(library.saveLyrics(id,attachTranslations(source,{"第一行","第二行"})));
            QFile file(library.absolute(library.track(id).lyrics));QVERIFY(file.open(QIODevice::ReadOnly));stored=file.readAll();QVERIFY(!stored.isEmpty());
        }
        Library reopened(root);QVERIFY(reopened.open());QCOMPARE(reopened.lyricOffset(id),450);QVERIFY(!reopened.isInstrumental(id));
        QFile file(reopened.absolute(reopened.track(id).lyrics));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),stored);
        // Timed translations use paired timestamps; the marker is for plain text.
        const auto lyrics=Lyrics::parse(stored);QVERIFY(lyrics.timed);QCOMPARE(lyrics.lines.size(),4);
        QCOMPARE(lyrics.lines[1].text,QString("第一行"));QCOMPARE(lyrics.lines[3].text,QString("第二行"));
        QCOMPARE(lyrics.lines[0].time,qint64(1120));QCOMPARE(lyrics.lines[1].time,qint64(1120));
        QCOMPARE(lyrics.lines[2].time,qint64(2120));QCOMPARE(lyrics.lines[3].time,qint64(2120));
        QCOMPARE(lyricTexts(sourceLyricsForTranslation(stored)),QStringList({"first line","second line"}));
        QVERIFY(reopened.setLyricOffset(id,-500));file.close();QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),stored);
    }
    void foundationNestedChangeBatchPreservesImmediateReads() {
        QTemporaryDir dir;Library library(dir.filePath("data"));QVERIFY(library.open());QSignalSpy changes(&library,&Library::changed);
        library.deferChanges(true);library.deferChanges(true);
        Track t;t.hash="foundation-batch";t.file="Music/fixture.wav";const auto id=library.insert(t);QVERIFY(id>0);
        const auto playlist=library.createPlaylist("batch");QVERIFY(playlist>0);QVERIFY(library.addToPlaylist(playlist,{id}));
        QCOMPARE(changes.size(),0);QCOMPARE(library.tracks(playlist).size(),1);QCOMPARE(library.track(id).id,id);
        library.deferChanges(false);QCOMPARE(changes.size(),0);QVERIFY(library.favorite(id,true));QVERIFY(library.track(id).favorite);
        library.deferChanges(false);QCOMPARE(changes.size(),1);library.deferChanges(false);QCOMPARE(changes.size(),1);
    }
    void lyricsLookupCacheAndPlaybackStatePersist(){
        QTemporaryDir d;const auto root=d.filePath("cache-data");qint64 id=0;
        {
            Library l(root);QVERIFY(l.open());Track t;t.hash="cache-roundtrip";t.file="Music/cache.wav";t.title="缓存验证";t.artist="测试歌手";t.duration=10000;QFile media(l.absolute(t.file));QVERIFY(media.open(QIODevice::WriteOnly));media.write("fixture");media.close();id=l.insert(t);QVERIFY(id>0);QVERIFY(!l.lookupCache(id).fresh);
            const QJsonArray records{QJsonObject{{"trackName",t.title},{"plainLyrics","first\nsecond"}}};QVERIFY(l.saveLookupCache(id,records,false));auto cache=l.lookupCache(id);QVERIFY(cache.fresh&&!cache.noResult);QCOMPARE(cache.records.size(),1);QVERIFY(l.saveLookupCache(id,{},true));cache=l.lookupCache(id);QVERIFY(cache.fresh&&cache.noResult&&cache.records.isEmpty());QVERIFY(l.clearLookupCache(id));QVERIFY(!l.lookupCache(id).fresh);
            QVERIFY(l.saveLookupCache(id,records,false));for(const auto &name:QSqlDatabase::connectionNames()){const auto db=QSqlDatabase::database(name);if(db.databaseName()==l.absolute("relaxer.sqlite")){QSqlQuery q(db);QVERIFY(q.exec("UPDATE lyrics_lookup SET fetched=datetime('now','-31 days')"));}}
            QVERIFY(!l.lookupCache(id).fresh);QVERIFY(l.saveLookupCache(id,{},true));for(const auto &name:QSqlDatabase::connectionNames()){const auto db=QSqlDatabase::database(name);if(db.databaseName()==l.absolute("relaxer.sqlite")){QSqlQuery q(db);QVERIFY(q.exec("UPDATE lyrics_lookup SET fetched=datetime('now','-8 days')"));}}QVERIFY(!l.lookupCache(id).fresh);
            QVERIFY(l.saveLookupCache(id,records,false));QVERIFY(l.setLyricOffset(id,800));QCOMPARE(l.lyricOffset(id),800);QVERIFY(l.markInstrumental(id));QVERIFY(l.isInstrumental(id));
        }
        Library reopened(root);QVERIFY(reopened.open());const auto cache=reopened.lookupCache(id);QVERIFY(cache.fresh&&!cache.noResult);QCOMPARE(cache.records.size(),1);QCOMPARE(reopened.lyricOffset(id),800);QVERIFY(reopened.isInstrumental(id));QVERIFY(reopened.saveLyrics(id,"[00:00.00]first\n[00:02.00]second"));QVERIFY(!reopened.isInstrumental(id));QCOMPARE(reopened.lyricOffset(id),800);QVERIFY(reopened.setLyricOffset(id,5000));QCOMPARE(reopened.lyricOffset(id),5000);
    }

    void artworkBelongsToTrack() {
        QTemporaryDir dir;
        const auto root = dir.filePath("资料库");
        const auto red = dir.filePath("red.png"), blue = dir.filePath("blue.png");
        QVERIFY(makeColor(red, Qt::red));
        QVERIFY(makeColor(blue, Qt::blue));
        QVERIFY(makeColor(dir.filePath("cover.jpg"), Qt::green));
        for (int i = 0; i < 2; ++i) {
            const auto source = dir.filePath(QString("song-%1.mp3").arg(i));
            QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=duration=0.4", "-i", i ? blue : red, "-map", "0:a",
                               "-map", "1:v", "-c:a", "libmp3lame", "-c:v", "png", "-disposition:v:0",
                               "attached_pic", "-metadata:s:v:0", "comment=Cover (front)", source}));
            const auto result = Importer::importOne(root, source);
            QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
            QCOMPARE(result.track.coverOrigin, QString("embedded"));
            QVERIFY(artworkMatches(root, result.track, i ? QColor(Qt::blue) : QColor(Qt::red)));
        }
        // Front cover is the second image stream; the first is deliberately unrelated artwork.
        const auto multi = dir.filePath("multiple-pictures.mp3");
        QVERIFY(runFfmpeg({"-f",
                           "lavfi",
                           "-i",
                           "sine=duration=0.4",
                           "-i",
                           blue,
                           "-i",
                           red,
                           "-map",
                           "0:a",
                           "-map",
                           "1:v",
                           "-map",
                           "2:v",
                           "-c:a",
                           "libmp3lame",
                           "-c:v",
                           "png",
                           "-disposition:v:0",
                           "attached_pic",
                           "-disposition:v:1",
                           "attached_pic",
                           "-metadata:s:v:0",
                           "comment=Cover (back)",
                           "-metadata:s:v:1",
                           "comment=Cover (front)",
                           multi}));
        const auto result = Importer::importOne(root, multi);
        QVERIFY(result.error.isEmpty());
        QVERIFY(artworkMatches(root, result.track, Qt::red));
    }
    void sidecarAndManualArtwork() {
        QTemporaryDir dir;
        const auto root = dir.filePath("data");
        const auto first = dir.filePath("first.wav"), second = dir.filePath("second.wav");
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=frequency=440:duration=0.4", first}));
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=frequency=660:duration=0.4", second}));
        QVERIFY(makeColor(dir.filePath("cover.jpg"), Qt::red));
        QVERIFY(makeColor(dir.filePath("folder.png"), Qt::red));
        QVERIFY(makeColor(dir.filePath("first.png"), Qt::blue));
        auto one = Importer::importOne(root, first);
        auto two = Importer::importOne(root, second);
        QVERIFY(one.error.isEmpty() && two.error.isEmpty());
        QCOMPARE(one.track.coverOrigin, QString("sidecar"));
        QVERIFY(artworkMatches(root, one.track, Qt::blue));
        QVERIFY(two.track.cover.isEmpty());
        QCOMPARE(two.track.coverOrigin, QString("none"));
        // A stale legacy cache must not cause a cover to reappear on re-import.
        QVERIFY(makeColor(root + "/Artwork/" + two.track.hash + ".png", Qt::red));
        QVERIFY(Importer::importOne(root, second).track.cover.isEmpty());
        Library library(root);
        QVERIFY(library.open());
        auto legacy = one.track;
        legacy.cover = "Artwork/" + legacy.hash + ".png";
        legacy.coverOrigin.clear();
        const auto legacyId = library.insert(legacy);
        QCOMPARE(library.insert(one.track), legacyId);
        QCOMPARE(library.track(legacyId).coverOrigin, QString("sidecar"));
        QVERIFY(artworkMatches(root, library.track(legacyId), Qt::blue));
        const auto id = library.insert(one.track);
        const auto playlist = library.createPlaylist("Keep this playlist");
        QVERIFY(library.addToPlaylist(playlist, {id}));
        QVERIFY(library.favorite(id, true));
        auto session = library.startSession(id, 1000);
        QVERIFY(library.saveSession(session, 800, 80));
        const auto manual = dir.filePath("manual.png");
        QVERIFY(makeColor(manual, Qt::green));
        QVERIFY(library.updateArtwork(id, manual));
        const auto pending = Importer::repairArtwork(root, library.track(id));
        QVERIFY(!library.applyArtworkRepair(pending));
        QVERIFY(makeColor(dir.filePath("first.png"), Qt::red));
        one = Importer::importOne(root, first);
        QCOMPARE(library.insert(one.track), id);
        const auto retained = library.track(id);
        QCOMPARE(retained.coverOrigin, QString("manual"));
        QVERIFY(artworkMatches(root, retained, Qt::green));
        QCOMPARE(retained.plays, 1);
        QCOMPARE(retained.heard, 800);
        QVERIFY(retained.favorite);
        QCOMPARE(library.tracks(playlist).first().id, id);
    }
    void legacyArtworkMigration() {
        QTemporaryDir dir;
        const auto root = dir.filePath("data");
        const auto image = dir.filePath("correct.png"), source = dir.filePath("embedded.mp3");
        QVERIFY(makeColor(image, Qt::red));
        QVERIFY(
            runFfmpeg({"-f", "lavfi", "-i", "sine=duration=0.4", "-i", image, "-map", "0:a", "-map", "1:v",
                       "-c:a", "libmp3lame", "-c:v", "png", "-disposition:v:0", "attached_pic", source}));
        auto embedded = Importer::importOne(root, source).track;
        const auto plain = dir.filePath("plain.wav");
        QVERIFY(runFfmpeg({"-f", "lavfi", "-i", "sine=duration=0.4", plain}));
        auto noCover = Importer::importOne(root, plain).track;
        embedded.cover = "Artwork/" + embedded.hash + ".png";
        noCover.cover = "Artwork/" + noCover.hash + ".png";
        QVERIFY(makeColor(root + "/" + embedded.cover, Qt::blue));
        QVERIFY(makeColor(root + "/" + noCover.cover, Qt::blue));
        const QString connection = "old-schema-artwork";
        {
            auto old = QSqlDatabase::addDatabase("QSQLITE", connection);
            old.setDatabaseName(root + "/relaxer.sqlite");
            QVERIFY(old.open());
            QSqlQuery query(old);
            QVERIFY(query.exec("CREATE TABLE tracks(id INTEGER PRIMARY KEY,hash TEXT NOT NULL UNIQUE,"
                               "file TEXT NOT NULL,title TEXT,artist TEXT,album TEXT,cover TEXT,lyrics TEXT,"
                               "duration INTEGER DEFAULT 0,plays INTEGER DEFAULT 0,heard INTEGER DEFAULT 0,"
                               "favorite INTEGER DEFAULT 0,added TEXT DEFAULT CURRENT_TIMESTAMP)"));
            for (const auto &track : {embedded, noCover}) {
                query.prepare("INSERT INTO tracks(hash,file,title,cover,plays,heard,favorite) "
                              "VALUES(?,?,?,?,7,123456,1)");
                query.addBindValue(track.hash);
                query.addBindValue(track.file);
                query.addBindValue("Legacy title");
                query.addBindValue(track.cover);
                QVERIFY(query.exec());
            }
            QVERIFY(makeColor(root + "/Artwork/track-3.png", Qt::green));
            query.prepare(
                "INSERT INTO tracks(hash,file,title,cover,plays,heard,favorite) VALUES(?,?,?,?,7,123456,1)");
            query.addBindValue("old-manual");
            query.addBindValue(embedded.file);
            query.addBindValue("Manual title");
            query.addBindValue("Artwork/track-3.png");
            QVERIFY(query.exec());
            old.close();
        }
        QSqlDatabase::removeDatabase(connection);
        {
            Library library(root);
            QVERIFY(library.open());
            const auto tracks = library.tracks();
            QCOMPARE(tracks.size(), 3);
            const auto playlist = library.createPlaylist("Original playlist");
            QVERIFY(library.addToPlaylist(playlist, {tracks[0].id, tracks[1].id, tracks[2].id}));
            for (const auto &track : tracks) {
                if (track.hash == "old-manual") {
                    QCOMPARE(track.coverOrigin, QString("manual"));
                    QVERIFY(artworkMatches(root, track, Qt::green));
                    continue;
                }
                QVERIFY(track.coverOrigin.isEmpty());
                const auto repaired = Importer::repairArtwork(root, track);
                QVERIFY2(repaired.error.isEmpty(), qPrintable(repaired.error));
                QCOMPARE(library.applyArtworkRepair(repaired),track.hash==embedded.hash);
                QVERIFY(!library.applyArtworkRepair(repaired));
                const auto updated = library.track(track.id);
                if (track.hash == embedded.hash) {
                    QCOMPARE(updated.coverOrigin, QString("embedded"));
                    QVERIFY(artworkMatches(root, updated, Qt::red));
                } else {
                    QVERIFY(updated.coverOrigin.isEmpty());
                    QCOMPARE(updated.cover,track.cover); // No-source recovery preserves valid legacy data.
                }
                QCOMPARE(updated.title, QString("Legacy title"));
                QCOMPARE(updated.plays, 7);
                QCOMPARE(updated.heard, 123456);
                QVERIFY(updated.favorite);
                QVERIFY(QFile::exists(root + "/" + track.cover));
            }
            QCOMPARE(library.tracks(playlist).size(), 3);
            // A late repair result must not overwrite a new manual selection.
            auto late = Importer::repairArtwork(root, tracks[0]);
            QVERIFY(library.updateArtwork(tracks[0].id, image));
            QVERIFY(!library.applyArtworkRepair(late));
        }
        Library reopened(root);
        QVERIFY(reopened.open());
        QCOMPARE(reopened.totalPlays(), 21);
        QCOMPARE(reopened.totalHeard(), 370368);
        QCOMPARE(reopened.playlists().first().count, 3);
    }
    void failedArtworkRepairPreservesData() {
        QTemporaryDir dir;
        Library library(dir.path());
        QVERIFY(library.open());
        Track track;
        track.hash = "missing";
        track.file = "Music/missing.wav";
        track.cover = "Artwork/missing.png";
        const auto id = library.insert(track);
        auto result = Importer::repairArtwork(dir.path(), library.track(id));
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!library.applyArtworkRepair(result));
        QCOMPARE(library.track(id).cover, track.cover);
        QVERIFY(library.track(id).coverOrigin.isEmpty());
        std::atomic_bool cancelled{true};
        result = Importer::repairArtwork(dir.path(), library.track(id), &cancelled);
        QCOMPARE(result.error, QString("已取消"));
        QVERIFY(!library.applyArtworkRepair(result));
    }
    void libraryAndPlaylists() {
        QTemporaryDir dir;
        Library l(dir.path());
        QVERIFY(l.open());
        Track t;
        t.hash = "a";
        t.file = "Music/a.wav";
        t.title = "100%_song";
        t.artist = "歌手";
        auto id = l.insert(t);
        QVERIFY(id);
        QCOMPARE(l.insert(t), id);
        QCOMPARE(l.tracks().size(), 1);
        QCOMPARE(l.tracks(0, "%_").size(), 1);
        QCOMPARE(l.tracks(0, "absent").size(), 0);
        auto p = l.createPlaylist("我的歌单");
        auto p2 = l.createPlaylist("第二张");
        QVERIFY(p && p2);
        QVERIFY(l.addToPlaylist(p, {id, id}));
        QVERIFY(l.addToPlaylist(p2, {id}));
        QCOMPARE(l.tracks(p).size(), 1);
        QVERIFY(l.favorite(id, true));
        QCOMPARE(l.tracks(-1).size(), 1);
        QVERIFY(l.removeFromPlaylist(p, {id}));
        QCOMPARE(l.tracks(p).size(), 0);
        QCOMPARE(l.tracks(p2).size(), 1);
        QVERIFY(l.deletePlaylist(p2));
        QCOMPARE(l.tracks().size(), 1);
    }
    void reorder() {
        QTemporaryDir d;
        Library l(d.path());
        QVERIFY(l.open());
        QVector<qint64> ids;
        for (int i = 0; i < 4; ++i) {
            Track t;
            t.hash = QString::number(i);
            t.file = "Music/test.wav";
            ids << l.insert(t);
        }
        auto p = l.createPlaylist("排序");
        QVERIFY(l.addToPlaylist(p, ids));
        std::reverse(ids.begin(), ids.end());
        QVERIFY(l.reorder(p, ids));
        auto songs = l.tracks(p);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(songs[i].id, ids[i]);
        QVERIFY(!l.reorder(p, {ids[0]}));
        QVERIFY(!l.reorder(p, {ids[0], ids[0], ids[2], ids[3]}));
    }
    void stats_data() {
        QTest::addColumn<int>("threshold");
        QTest::newRow("70") << 70;
        QTest::newRow("80") << 80;
        QTest::newRow("100") << 100;
    }
    void stats() {
        QFETCH(int, threshold);
        QTemporaryDir d;
        Library l(d.path());
        QVERIFY(l.open());
        Track t;
        t.hash = "stats";
        t.file = "Music/a.wav";
        auto id = l.insert(t);
        auto s = l.startSession(id, 10000);
        QVERIFY(l.saveSession(s, threshold * 100 - 1, threshold));
        QCOMPARE(l.track(id).plays, 0);
        QVERIFY(l.saveSession(s, 1, threshold));
        QCOMPARE(l.track(id).plays, 1);
        QVERIFY(l.saveSession(s, 0, threshold));
        QCOMPARE(l.track(id).plays, 1);
        s.sought = true;
        QVERIFY(l.saveSession(s, 0, threshold));
        QCOMPARE(l.track(id).plays, 0);
        QVERIFY(l.saveSession(s, 10000, threshold, true));
        QCOMPARE(l.track(id).plays, 0);
        auto loop = l.startSession(id, 10000);
        QVERIFY(l.saveSession(loop, 10000, threshold, true));
        QCOMPARE(l.track(id).plays, 1);
        auto loop2 = l.startSession(id, 10000);
        QVERIFY(l.saveSession(loop2, 10000, threshold, true));
        QCOMPARE(l.track(id).plays, 2);
        QCOMPARE(l.totalPlays(), 2);
        QVERIFY(l.totalHeard() > 20000);
    }
    void reopenResume() {
        QTemporaryDir d;
        qint64 id, session;
        {
            Library l(d.path());
            QVERIFY(l.open());
            Track t;
            t.hash = "resume";
            t.file = "Music/a.wav";
            id = l.insert(t);
            auto s = l.startSession(id, 10000);
            session = s.id;
            QVERIFY(l.saveSession(s, 5000, 80));
            l.setSetting("threshold", 91);
            l.setSetting("queue", QJsonArray{double(id)});
        }
        Library l(d.path());
        QVERIFY(l.open());
        QCOMPARE(l.setting("threshold").toInt(), 91);
        auto s = l.session(session);
        QCOMPARE(s.heard, 5000);
        QVERIFY(l.saveSession(s, 3000, 80));
        QCOMPARE(l.track(id).plays, 1);
    }
    void invalidStats() {
        QTemporaryDir d;
        Library l(d.path());
        QVERIFY(l.open());
        Track t;
        t.hash = "zero";
        t.file = "Music/z.wav";
        auto id = l.insert(t);
        auto s = l.startSession(id, 0);
        QVERIFY(l.saveSession(s, 500000, 80));
        QCOMPARE(l.track(id).plays, 0);
        QCOMPARE(l.track(id).heard, 500000);
    }
    void lyrics() {
        auto l = Lyrics::parse("[ar:artist]\n[offset:-50]\n[00:01.25][00:03.250]same\n[00:02.00]next\n");
        QVERIFY(l.timed);
        QCOMPARE(l.lines.size(), 3);
        QCOMPARE(l.lines[0].time, 1200);
        QCOMPARE(l.lines[1].time, 1950);
        QCOMPARE(l.lines[2].time, 3200);
        QCOMPARE(l.indexAt(1199), -1);
        QCOMPARE(l.indexAt(1950), 1);
        auto plain = Lyrics::parse("one\ntwo");
        QVERIFY(!plain.timed);
        QCOMPARE(plain.lines.size(), 2);
    }
    void shuffle() {
        Queue q;
        q.mode = Mode::Shuffle;
        q.set({1, 2, 3, 4}, 1);
        QSet<qint64> round{q.current()};
        for (int i = 0; i < 3; ++i) {
            auto id = q.next();
            QVERIFY(!round.contains(id));
            round.insert(id);
        }
        QCOMPARE(round.size(), 4);
        auto last = q.current();
        QVERIFY(q.next() != last);
        auto next = q.current();
        QCOMPARE(q.previous(), last);
        QCOMPARE(q.next(), next);
        QSet<qint64> round2{q.current()};
        for (int i = 0; i < 3; ++i) {
            auto id = q.next();
            QVERIFY(!round2.contains(id));
            round2.insert(id);
        }
        QCOMPARE(round2.size(), 4);
        Queue one;
        one.mode = Mode::Shuffle;
        one.set({42}, 42);
        QCOMPARE(one.next(), 42);
    }
    void queueModes() {
        Queue q;
        q.set({1, 2, 2, 3}, 3);
        QCOMPARE(q.items.size(), 3);
        q.mode = Mode::Sequential;
        QCOMPARE(q.next(), 0);
        q.mode = Mode::ListLoop;
        QCOMPARE(q.next(), 1);
        q.mode = Mode::SingleLoop;
        QCOMPARE(q.next(false), 1);
        QCOMPARE(q.next(true), 2);
        q.set({});
        QCOMPARE(q.next(), 0);
    }
    void actualImport() {
        QTemporaryDir d;
        auto root = d.filePath("资料库");
        auto source = d.filePath("歌 曲 ' & % test.wav");
        QProcess ff;
        ff.start(toolPath("ffmpeg"),
                 {"-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=1.2", "-metadata",
                  "title=测试音乐", "-metadata", "artist=Relaxer Tests", "-y", source});
        QVERIFY(ff.waitForFinished(20000));
        QCOMPARE(ff.exitCode(), 0);
        QFile lrc(d.filePath("歌 曲 ' & % test.lrc"));
        QVERIFY(lrc.open(QIODevice::WriteOnly));
        lrc.write("[00:00.00]test lyric");
        lrc.close();
        auto r = Importer::importOne(root, source);
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QVERIFY(QFile::exists(source));
        QVERIFY(QFile::exists(root + "/" + r.track.file));
        QCOMPARE(r.track.title, QString("测试音乐"));
        QCOMPARE(r.track.artist, QString("Relaxer Tests"));
        QVERIFY(r.track.duration >= 1190 && r.track.duration <= 1210);
        QVERIFY(!r.track.lyrics.isEmpty());
        auto again = Importer::importOne(root, source);
        QCOMPARE(again.track.hash, r.track.hash);
        Library lib(root);
        QVERIFY(lib.open());
        QCOMPARE(lib.insert(r.track), lib.insert(again.track));
        QFile invalid(d.filePath("bad.mp3"));
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("not audio");
        invalid.close();
        QVERIFY(!Importer::importOne(root, invalid.fileName()).error.isEmpty());
    }
    void realPlaybackAndSeek() {
        if (audio_device_fixture::excluded()) QSKIP(audio_device_fixture::excludedReason);
        QTemporaryDir d;
        Library lib(d.filePath("data"));
        QVERIFY(lib.open());
        auto file = d.filePath("clock.wav");
        QProcess ff;
        ff.start(toolPath("ffmpeg"),
                 {"-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=3", "-y", file});
        QVERIFY(ff.waitForFinished(20000));
        QCOMPARE(ff.exitCode(), 0);
        const auto control = audio_device_fixture::probe(file);
        QVERIFY2(control.status != audio_device_fixture::Status::FixtureError, qPrintable(control.detail));
        if (control.status == audio_device_fixture::Status::EnvironmentBlocked) QSKIP(qPrintable(control.detail));
        auto result = Importer::importOne(lib.root(), file);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        auto id = lib.insert(result.track);
        QVERIFY(lib.setSetting("volume", 10));
        Playback p(&lib);
        QVERIFY(p.media()->audioOutput());
        QVERIFY(!p.media()->audioOutput()->device().isNull());
        QSignalSpy errors(&p, &Playback::error);
        p.setMode(Mode::Sequential);
        p.play({id});
        QTRY_VERIFY_WITH_TIMEOUT(p.position() > 600, 6000);
        p.toggle();
        auto paused = p.position();
        auto heard = lib.track(id).heard;
        QTest::qWait(450);
        QCOMPARE(p.position(), paused);
        QCOMPARE(lib.track(id).heard, heard);
        p.toggle();
        QTRY_COMPARE_WITH_TIMEOUT(lib.track(id).plays, 1, 6000);
        p.seek(500);
        QCOMPARE(lib.track(id).plays, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!p.playing(), 6000);
        QCOMPARE(lib.track(id).plays, 0);
        lib.setSetting("threshold", 100);
        p.play({id});
        QTRY_COMPARE_WITH_TIMEOUT(lib.track(id).plays, 1, 6500);
        QVERIFY(lib.track(id).heard >= 5800);
        QCOMPARE(errors.size(), 0);
    }
    void realResumeAndLoop() {
        if (audio_device_fixture::excluded()) QSKIP(audio_device_fixture::excludedReason);
        QTemporaryDir d;
        Library lib(d.filePath("data"));
        QVERIFY(lib.open());
        auto file = d.filePath("resume.wav");
        QProcess ff;
        ff.start(toolPath("ffmpeg"),
                 {"-v", "error", "-f", "lavfi", "-i", "sine=frequency=660:duration=2", "-y", file});
        QVERIFY(ff.waitForFinished(20000));
        QCOMPARE(ff.exitCode(), 0);
        const auto control = audio_device_fixture::probe(file);
        QVERIFY2(control.status != audio_device_fixture::Status::FixtureError, qPrintable(control.detail));
        if (control.status == audio_device_fixture::Status::EnvironmentBlocked) QSKIP(qPrintable(control.detail));
        auto result = Importer::importOne(lib.root(), file);
        QVERIFY(result.error.isEmpty());
        auto id = lib.insert(result.track);
        QVERIFY(lib.setSetting("volume", 10));
        qint64 position = 0;
        {
            Playback p(&lib);
            QVERIFY(p.media()->audioOutput());
            QVERIFY(!p.media()->audioOutput()->device().isNull());
            p.setMode(Mode::SingleLoop);
            p.play({id});
            QTRY_VERIFY_WITH_TIMEOUT(p.position() > 650, 6000);
            p.toggle();
            p.save();
            position = p.position();
            QCOMPARE(lib.track(id).plays, 0);
        }
        Playback restored(&lib);
        QVERIFY(restored.media()->audioOutput());
        QVERIFY(!restored.media()->audioOutput()->device().isNull());
        QSignalSpy errors(&restored, &Playback::error);
        restored.restore();
        QTRY_VERIFY_WITH_TIMEOUT(restored.position() >= position - 50, 4000);
        QVERIFY(!restored.playing());
        restored.toggle();
        QTRY_COMPARE_WITH_TIMEOUT(lib.track(id).plays, 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(lib.track(id).plays, 2, 6000);
        restored.toggle();
        QCOMPARE(lib.track(id).plays, 2);
        QCOMPARE(errors.size(), 0);
    }
};
QTEST_GUILESS_MAIN(Tests)
#include "tests.moc"
