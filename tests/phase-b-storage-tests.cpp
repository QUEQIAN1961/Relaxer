#include "integration-fixture.h"
#include "storage/appearance-store.h"
#include <QSqlError>
#include <QUuid>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace relaxer;
using namespace integration_fixture;
namespace {
QByteArray rawSetting(const QSqlDatabase &db, const QString &key) {
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE key=?");
    q.addBindValue(key);
    return q.exec() && q.next() ? q.value(0).toByteArray() : QByteArray();
}
bool sql(const QSqlDatabase &db, const QString &text) {
    QSqlQuery query(db);
    return query.exec(text);
}
QJsonObject unknown() {
    return {{"opaque", QJsonArray{true, 17, QJsonObject{{"未来", "保留"}}}}};
}
} // namespace
class PhaseBStorageTests final : public QObject {
    Q_OBJECT
  private slots:
    void historicalInitializationAndDataSurvive_data() {
        QTest::addColumn<int>("index");
        QTest::newRow("accepted-1.10-history") << 0;
        QTest::newRow("b9-r8-freeze") << 1;
    }
    void historicalInitializationAndDataSurvive() {
        QFETCH(int, index);
        QFile fixture(QStringLiteral(RELAXER_LEGACY_SQL_FIXTURE));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto original = QJsonDocument::fromJson(fixture.readAll()).array().at(index).toObject();
        QTemporaryDir dir;
        const auto root = dir.filePath("legacy data with spaces");
        QDir().mkpath(root);
        QVERIFY(wave(root + "/Music/legacy song.wav", 440));
        QVERIFY(wave(root + "/Music/second.wav", 441));
        QDir().mkpath(root + "/Lyrics");
        QFile lyrics(root + "/Lyrics/old bilingual.lrc");
        QVERIFY(lyrics.open(QIODevice::WriteOnly));
        const QByteArray bilingual =
            QStringLiteral("[re:relaxer-bilingual]\n[00:01.00]Hello world\n[00:01.00]你好世界").toUtf8();
        QCOMPARE(lyrics.write(bilingual), qint64(bilingual.size()));
        lyrics.close();
        QDir().mkpath(root + "/Artwork");
        QImage cover(8, 8, QImage::Format_RGB32);
        cover.fill(Qt::blue);
        QVERIFY(cover.save(root + "/Artwork/legacy.png"));
        const auto coverBytes = bytes(root + "/Artwork/legacy.png");
        const auto name = QUuid::createUuid().toString();
        QJsonObject theme = AppearanceStore::defaults();
        theme["id"] = "legacy-theme";
        theme["name"] = "旧主题";
        theme["left"] = "#123456";
        theme["extension"] = unknown();
        const QJsonObject ai{
            {"schemaVersion", 1},
            {"defaults", QJsonObject{{"providerId", "deepseek"}, {"modelId", "deepseek-flash"}}},
            {"targetLanguage", "zh-CN"},
            {"future", unknown()}};
        const QJsonObject settings{
            {"queue", QJsonArray{41, 42}},
            {"current", 41},
            {"position", 1200},
            {"activeSession", 77},
            {"activeThreshold", 80},
            {"mode", 3},
            {"volume", 37},
            {"theme", 3},
            {"appearanceHistory", QJsonObject{{"items", QJsonArray{theme}}, {"active", "legacy-theme"}}},
            {"aiServiceConfigV1", ai},
            {"future-setting", unknown()},
            {"null-extension", QJsonValue(QJsonValue::Null)}};
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", name);
            db.setDatabaseName(root + "/relaxer.sqlite");
            QVERIFY(db.open());
            for (const auto &statement : original["statements"].toArray())
                QVERIFY2(sql(db, statement.toString()), qPrintable(statement.toString()));
            QVERIFY(
                sql(db, "INSERT INTO "
                        "tracks(id,hash,file,title,artist,album,cover,lyrics,duration,plays,heard,favorite,"
                        "added,cover_origin) VALUES(41,'legacy-hash','Music/legacy song.wav','Legacy "
                        "Song','Legacy Artist','Legacy Album','Artwork/legacy.png','Lyrics/old "
                        "bilingual.lrc',10000,7,45000,1,'2021-02-03 04:05:06','manual')"));
            QVERIFY(sql(db, "INSERT INTO tracks(id,hash,file,title,duration,cover_origin) "
                            "VALUES(42,'legacy-second','Music/second.wav','Second',10000,'none')"));
            QVERIFY(sql(db, "INSERT INTO playlists(id,name,cover,tags,description) VALUES(7,'Legacy "
                            "Playlist','Artwork/legacy.png','[\"旧标签\"]','保留描述')"));
            QVERIFY(sql(db, "INSERT INTO playlist_tracks VALUES(7,42,0),(7,41,1)"));
            QVERIFY(sql(db, "INSERT INTO lyrics_playback(track,offset_ms,instrumental) VALUES(41,-250,0)"));
            QVERIFY(sql(db, "INSERT INTO sessions(id,track,heard,duration,sought,counted,finished) "
                            "VALUES(77,41,1600,10000,1,0,0)"));
            QVERIFY(sql(db, "INSERT INTO lyrics_lookup(track,payload,fetched,no_result) "
                            "VALUES(41,'[{\"trackName\":\"Legacy Song\",\"plainLyrics\":\"Hello "
                            "world\"}]',CURRENT_TIMESTAMP,0)"));
            for (auto it = settings.begin(); it != settings.end(); ++it) {
                QSqlQuery q(db);
                q.prepare("INSERT OR REPLACE INTO settings(key,value) VALUES(?,?)");
                q.addBindValue(it.key());
                q.addBindValue(
                    QString::fromUtf8(QJsonDocument(QJsonArray{it.value()}).toJson(QJsonDocument::Compact)));
                QVERIFY(q.exec());
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        QStringList schema;
        for (int reopen = 0; reopen < 2; ++reopen) {
            Library library(root);
            QVERIFY(library.open());
            if (!reopen)
                schema = import_fixture::schema(library);
            else
                QCOMPARE(import_fixture::schema(library), schema);
            const auto track = library.track(41);
            QCOMPARE(track.id, qint64(41));
            QCOMPARE(track.file, QString("Music/legacy song.wav"));
            QCOMPARE(track.cover, QString("Artwork/legacy.png"));
            QCOMPARE(track.lyrics, QString("Lyrics/old bilingual.lrc"));
            QCOMPARE(track.plays, qint64(7));
            QCOMPARE(track.heard, qint64(45000));
            QVERIFY(track.favorite);
            QCOMPARE(track.importedAt, QString("2021-02-03 04:05:06"));
            QCOMPARE(library.tracks(7).size(), 2);
            QCOMPARE(library.tracks(7)[0].id, qint64(42));
            QCOMPARE(library.tracks(7)[1].id, qint64(41));
            QCOMPARE(library.playlists()[0].tags, QStringList{"旧标签"});
            QCOMPARE(library.playlists()[0].description, QString("保留描述"));
            QCOMPARE(library.lyricOffset(41), -250);
            QVERIFY(library.lookupCache(41).fresh);
            QVERIFY(!library.lookupCache(41).records.isEmpty());
            for (auto it = settings.begin(); it != settings.end(); ++it)
                QCOMPARE(library.setting(it.key()), it.value());
            AppearanceStore appearance(&library);
            QCOMPARE(appearance.active()["id"].toString(), QString("legacy-theme"));
            QCOMPARE(appearance.active()["extension"].toObject(), unknown());
            AiRuntime runtime(&library, std::make_unique<FakeCredentialStore>());
            QCOMPARE(runtime.configuration().defaults.providerId, QString("deepseek"));
            QCOMPARE(runtime.configuration().extensions["future"].toObject(), unknown());
            PlaybackFacade player(&library, nullptr, true);
            player.restore();
            QTRY_VERIFY(player.seekable());
            QTRY_COMPARE(player.position(), qint64(1200));
            QVERIFY(!player.playing());
            QCOMPARE(player.current().id, qint64(41));
            QCOMPARE(player.volume(), 37);
            QCOMPARE(player.mode(), Mode::Shuffle);
            QCOMPARE(player.queue().items, QVector<qint64>({41, 42}));
            QCOMPARE(bytes(library.absolute(track.lyrics)), bilingual);
            QCOMPARE(bytes(library.absolute(track.cover)), coverBytes);
            QVERIFY(player.shutdown());
            QCOMPARE(library.setting("aiServiceConfigV1").toObject(), ai);
            QCOMPARE(library.setting("future-setting").toObject(), unknown());
            QCOMPARE(import_fixture::schema(library), schema);
        }
        qInfo().noquote() << "LEGACY_COMPAT" << original["commit"].toString()
                          << original["libraryOpenSha256"].toString()
                          << "ids=41,42 playlist=7 schema unchanged bytes preserved";
    }
    void sqliteFailureRollbackAndRetry_data() {
        QTest::addColumn<int>("failure");
        QTest::newRow("raise-abort") << 0;
        QTest::newRow("sqlite-busy") << 1;
        QTest::newRow("readonly-connection") << 2;
    }
    void sqliteFailureRollbackAndRetry() {
        QFETCH(int, failure);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        QVERIFY(library.setSettings(
            {{"queue", QJsonArray{41}}, {"current", 41}, {"position", 999}, {"activeSession", 77}}));
        PlaybackFacade player(&library, nullptr, true);
        auto db = import_fixture::database(library);
        QSignalSpy failed(&player, &PlaybackFacade::saveFailed);
        const auto name = QUuid::createUuid().toString();
        auto lock = QSqlDatabase::addDatabase("QSQLITE", name);
        lock.setDatabaseName(library.absolute("relaxer.sqlite"));
        QVERIFY(lock.open());
        if (failure == 0)
            QVERIFY(sql(db, "CREATE TEMP TRIGGER rc_abort BEFORE INSERT ON settings WHEN NEW.key='position' "
                            "BEGIN SELECT RAISE(ABORT,'RC controlled ABORT'); END"));
        if (failure == 1) {
            QVERIFY(sql(db, "PRAGMA busy_timeout=0"));
            QVERIFY(sql(lock, "BEGIN IMMEDIATE"));
        }
        if (failure == 2) {
            db.close();
            db.setConnectOptions("QSQLITE_OPEN_READONLY");
            QVERIFY(db.open());
        }
        QVERIFY(!player.save());
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(!failed.first().first().toString().isEmpty());
        QCOMPARE(library.setting("queue").toArray(), QJsonArray{41});
        QCOMPARE(library.setting("current").toInt(), 41);
        QCOMPARE(library.setting("position").toInt(), 999);
        QCOMPARE(library.setting("activeSession").toInt(), 77);
        if (failure == 0)
            QVERIFY(sql(db, "DROP TRIGGER rc_abort"));
        if (failure == 1)
            QVERIFY(sql(lock, "ROLLBACK"));
        if (failure == 2) {
            db.close();
            db.setConnectOptions({});
            QVERIFY(db.open());
        }
        QVERIFY(player.save());
        QVERIFY(library.setting("queue").toArray().isEmpty());
        QCOMPARE(library.setting("position").toInt(), 0);
        QVERIFY(player.shutdown());
        lock.close();
        lock = {};
        QSqlDatabase::removeDatabase(name);
    }
    void finalSaveFailureRemainsFrozenWithoutNewInteraction() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        QVERIFY(library.setSetting("position", 817));
        auto db = import_fixture::database(library);
        PlaybackFacade player(&library, nullptr, true);
        QSignalSpy saved(&player, &PlaybackFacade::saved);
        QVERIFY(sql(db, "CREATE TEMP TRIGGER rc_final_abort BEFORE INSERT ON settings BEGIN SELECT "
                        "RAISE(ABORT,'RC final ABORT'); END"));
        QVERIFY(!player.shutdown());
        QVERIFY(player.isShutdown());
        QCOMPARE(saved.size(), 1);
        QVERIFY(!saved[0][0].toBool());
        QCOMPARE(library.setting("position").toInt(), 817);
        QVERIFY(sql(db, "DROP TRIGGER rc_final_abort"));
        QVERIFY(!player.save());
        QVERIFY(!player.shutdown());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(library.setting("position").toInt(), 817);
        qInfo() << "D-R8-02 reproduced: durable prior snapshot intact; final snapshot not retried after "
                   "shutdown; no new modal contract";
    }
    void playbackDatabaseErrorDoesNotCorruptLibrary() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        PlaybackFacade player(&library, nullptr, true);
        player.play(ids);
        QTRY_VERIFY(player.playing());
        QTRY_VERIFY(player.seekable());
        player.toggle();
        QTRY_VERIFY(!player.playing());
        auto db = import_fixture::database(library);
        QVERIFY(sql(db, "CREATE TEMP TRIGGER rc_play_abort BEFORE INSERT ON settings BEGIN SELECT "
                        "RAISE(ABORT,'RC playing ABORT'); END"));
        QVERIFY(!player.save());
        QCOMPARE(library.tracks().size(), 3);
        QCOMPARE(player.queue().items, ids);
        QVERIFY(sql(db, "DROP TRIGGER rc_play_abort"));
        QVERIFY(player.save());
        for (const auto &track : library.tracks())
            QVERIFY(QFileInfo::exists(library.absolute(track.file)));
    }
    void lyricFileFailuresPreserveBytes_data() {
        QTest::addColumn<int>("failure");
        QTest::newRow("external-modification") << 0;
        QTest::newRow("database-refusal") << 1;
        QTest::newRow("exclusive-file-lock") << 2;
    }
    void lyricFileFailuresPreserveBytes() {
        QFETCH(int, failure);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        const QByteArray original = "[00:01.00]Hello world";
        QVERIFY(library.saveLyrics(ids[0], original));
        QVERIFY(library.saveLyrics(ids[1], "other song"));
        const auto path = library.absolute(library.track(ids[0]).lyrics);
        const auto other = library.absolute(library.track(ids[1]).lyrics);
        const auto snapshot = library.lyricsSaveSnapshot(ids[0]);
        const auto files = QDir(QFileInfo(path).absolutePath()).entryList(QDir::Files);
        QByteArray expected = original;
        auto db = import_fixture::database(library);
        if (failure == 0) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            expected = "external owner update";
            QCOMPARE(file.write(expected), qint64(expected.size()));
            file.close();
        }
        if (failure == 1)
            QVERIFY(sql(db, "CREATE TEMP TRIGGER rc_lyric_abort BEFORE UPDATE ON tracks BEGIN SELECT "
                            "RAISE(ABORT,'RC lyrics ABORT'); END"));
#ifdef Q_OS_WIN
        HANDLE handle = INVALID_HANDLE_VALUE;
        if (failure == 2) {
            handle = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()),
                                 GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            QVERIFY(handle != INVALID_HANDLE_VALUE);
        }
        const auto release = qScopeGuard([&] {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        });
#else
        if (failure == 2) {
            QFAIL("exclusive lock test requires Windows");
        }
#endif
        QVERIFY(!library.saveLyricsChecked(ids[0], "replacement lyrics", snapshot));
#ifdef Q_OS_WIN
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
#endif
        QCOMPARE(bytes(path), expected);
        QCOMPARE(bytes(other), QByteArray("other song"));
        QCOMPARE(QDir(QFileInfo(path).absolutePath()).entryList(QDir::Files), files);
        if (failure == 1)
            QVERIFY(sql(db, "DROP TRIGGER rc_lyric_abort"));
    }
};
QTEST_MAIN(PhaseBStorageTests)
#include "phase-b-storage-tests.moc"
