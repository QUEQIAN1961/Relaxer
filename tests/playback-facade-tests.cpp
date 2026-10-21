#include "playback-facade.h"
#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QScopeGuard>
#include <QSqlError>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

using namespace relaxer;
namespace relaxer {
class PlaybackDiagnostics {
  public:
    static void session(Playback &p, qint64 id, qint64 duration, int threshold, bool sought = false) {
        p.m_timer.stop();
        p.m_session = p.m_library->startSession(id, duration);
        p.m_session.sought = sought;
        p.m_loading = false;
        p.m_threshold = threshold;
        p.m_lastPosition = 0;
        p.m_lastWall = 0;
    }
    static bool sample(Playback &p, qint64 wall, qint64 pos, bool playing, bool finished = false) {
        return p.sampleAt(wall, pos, playing, finished, false);
    }
    static Session session(const Playback &p) { return p.m_session; }
    static void error(Playback &p) { p.onError("controlled decode failure"); }
};
} // namespace relaxer
namespace {
QVector<qint64> seed(Library &library, int count = 3) {
    QVector<qint64> ids;
    for (int n = 0; n < count; ++n) {
        Track t;
        t.title = QString("B9 %1").arg(n);
        t.hash = t.title;
        t.file = QString("Music/b9-%1.wav").arg(n);
        t.duration = 10000;
        QFile file(library.absolute(t.file));
        if (!file.open(QIODevice::WriteOnly))
            return {};
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);
        constexpr quint32 bytes = 160000;
        out.writeRawData("RIFF", 4);
        out << quint32(36 + bytes);
        out.writeRawData("WAVEfmt ", 8);
        out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2)
            << quint16(16);
        out.writeRawData("data", 4);
        out << bytes;
        const QByteArray silence(bytes, '\0');
        out.writeRawData(silence.constData(), silence.size());
        file.close();
        if (out.status() != QDataStream::Ok)
            return {};
        ids << library.insert(t);
    }
    return ids;
}
class AuditDatabase {
    QString name = QUuid::createUuid().toString();

  public:
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    explicit AuditDatabase(const Library &library) {
        db.setDatabaseName(library.root() + "/relaxer.sqlite");
        db.open();
    }
    ~AuditDatabase() {
        db.close();
        db = {};
        QSqlDatabase::removeDatabase(name);
    }
    bool exec(const QString &sql) {
        QSqlQuery q(db);
        return q.exec(sql);
    }
    qint64 count() {
        QSqlQuery q(db);
        if (!q.exec("SELECT COUNT(*) FROM b9_writes") || !q.next())
            return -1;
        return q.value(0).toLongLong();
    }
};
} // namespace
class PlaybackFacadeTests : public QObject {
    Q_OBJECT
  private slots:
    void emptyLibraryAndSnapshotAreReadOnly() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        PlaybackFacade facade(&library, nullptr, true);
        QSignalSpy snapshots(&facade, &PlaybackFacade::snapshotChanged);
        QCOMPARE(facade.snapshot().track.id, qint64(0));
        QCOMPARE(facade.snapshot().duration, qint64(0));
        QVERIFY(!facade.seekable());
        facade.seek(900);
        facade.next();
        facade.previous();
        facade.toggle();
        QCOMPARE(facade.current().id, qint64(0));
        QVERIFY(!facade.playing());
        for (int n = 0; n < 20; ++n)
            QVERIFY(facade.snapshot().queue.isEmpty());
        QVERIFY(facade.upcoming().isEmpty());
        QVERIFY(facade.shutdown());
        QVERIFY(facade.isShutdown());
        QVERIFY(!snapshots.isEmpty());
    }
    void volumeAndMode_data() {
        QTest::addColumn<int>("volume");
        QTest::addColumn<int>("expected");
        QTest::newRow("mute") << 0 << 0;
        QTest::newRow("max") << 100 << 100;
        QTest::newRow("clamp-low") << -2 << 0;
        QTest::newRow("clamp-high") << 105 << 100;
    }
    void volumeAndMode() {
        QFETCH(int, volume);
        QFETCH(int, expected);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        PlaybackFacade facade(&library, nullptr, true);
        facade.setVolume(volume);
        QCOMPARE(facade.volume(), expected);
        QCOMPARE(library.setting("volume").toInt(), expected);
        QCOMPARE(facade.snapshot().volume, expected);
        for (int mode = 0; mode < 4; ++mode) {
            facade.setMode(Mode(mode));
            QCOMPARE(facade.snapshot().mode, Mode(mode));
            QCOMPARE(library.setting("mode").toInt(), mode);
        }
    }
    void queueCommandsAndForecast_data() {
        QTest::addColumn<int>("mode");
        for (int n = 0; n < 4; ++n)
            QTest::newRow(qPrintable(QString::number(n))) << n;
    }
    void queueCommandsAndForecast() {
        QFETCH(int, mode);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QCOMPARE(ids.size(), 3);
        PlaybackFacade facade(&library, nullptr, true);
        facade.setMode(Mode(mode));
        facade.play(ids, ids[0]);
        QCOMPARE(facade.current().id, ids[0]);
        QCOMPARE(facade.snapshot().queue, ids);
        const auto forecast = facade.upcoming();
        QVERIFY(!forecast.isEmpty());
        for (int n = 0; n < 10; ++n)
            facade.snapshot(); // Must not consume/generate a different bag.
        QCOMPARE(facade.upcoming(), forecast);
        facade.playUpcoming(-1);
        QCOMPARE(facade.current().id, ids[0]);
        facade.playUpcoming(0);
        QCOMPARE(facade.current().id, forecast[0]);
        facade.previous();
        QVERIFY(ids.contains(facade.current().id));
        facade.play(ids, ids.last());
        facade.next();
        if (mode == int(Mode::Sequential))
            QVERIFY(!facade.playing());
        else
            QVERIFY(ids.contains(facade.current().id));
        const auto removed = facade.current().id;
        QVERIFY(library.remove({removed}));
        facade.forget({removed});
        QVERIFY(!facade.queue().items.contains(removed));
        QVERIFY(facade.current().id != removed);
    }
    void realPcmPauseSeekAndRestore() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        {
            PlaybackFacade facade(&library, nullptr, true);
            QSignalSpy tracks(&facade, &PlaybackFacade::trackChanged);
            QSignalSpy positions(&facade, &PlaybackFacade::positionChanged);
            facade.play(ids, ids[0]);
            QTRY_VERIFY(facade.playing());
            QTRY_VERIFY(facade.seekable());
            QTRY_COMPARE(facade.duration(), qint64(10000));
            facade.toggle();
            QTRY_VERIFY(!facade.playing());
            const auto heard = library.totalHeard();
            QTest::qWait(280);
            QCOMPARE(library.totalHeard(), heard);
            facade.seek(1800);
            QTRY_COMPARE(facade.position(), qint64(1800));
            QVERIFY(facade.roundStatus().contains("跳转"));
            facade.toggle();
            QTRY_VERIFY(facade.playing());
            facade.toggle();
            QTRY_VERIFY(!facade.playing());
            facade.seek(1800);
            QVERIFY(facade.save());
            QVERIFY(!tracks.isEmpty());
            QVERIFY(!positions.isEmpty());
        }
        PlaybackFacade restored(&library, nullptr, true);
        restored.restore();
        QTRY_VERIFY(restored.seekable());
        QTRY_COMPARE(restored.position(), qint64(1800));
        QCOMPARE(restored.current().id, ids[0]);
        QVERIFY(!restored.playing());
        QCOMPARE(restored.queue().items, ids);
    }
    void statisticsThreshold_data() {
        QTest::addColumn<int>("threshold");
        QTest::addColumn<bool>("sought");
        for (int threshold : {70, 80, 100})
            for (bool sought : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2").arg(threshold).arg(sought))) << threshold << sought;
    }
    void statisticsThreshold() {
        QFETCH(int, threshold);
        QFETCH(bool, sought);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        PlaybackFacade facade(&library, nullptr, true);
        auto &core = *facade.backend();
        PlaybackDiagnostics::session(core, ids[0], 10000, threshold, sought);
        const qint64 edge = threshold * 100;
        QVERIFY(PlaybackDiagnostics::sample(core, edge - 1, edge - 1, true));
        QCOMPARE(library.totalPlays(), qint64(0));
        QVERIFY(PlaybackDiagnostics::sample(core, edge, edge, true));
        QCOMPARE(library.totalPlays(), qint64(sought ? 0 : 1));
        QVERIFY(PlaybackDiagnostics::sample(core, edge + 1000, edge, false));
        QCOMPARE(library.totalHeard(), edge);
        QVERIFY(PlaybackDiagnostics::sample(core, edge + 2000, edge, false, true));
        QCOMPARE(library.totalPlays(), qint64(sought ? 0 : 1));
        QCOMPARE(library.totalHeard(), edge);
    }
    void statisticsBoundsAndCompleteRound() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        PlaybackFacade facade(&library, nullptr, true);
        auto &core = *facade.backend();
        PlaybackDiagnostics::session(core, ids[0], 10000, 100);
        QVERIFY(PlaybackDiagnostics::sample(core, 1, 5000, true));
        QCOMPARE(library.totalHeard(), qint64(81));
        QVERIFY(PlaybackDiagnostics::sample(core, 2, 2000, true));
        QCOMPARE(library.totalHeard(), qint64(81));
        PlaybackDiagnostics::session(core, ids[1], 10000, 100);
        QVERIFY(PlaybackDiagnostics::sample(core, 9800, 9800, true, true));
        QCOMPARE(library.track(ids[1]).plays, qint64(1));
        QVERIFY(PlaybackDiagnostics::sample(core, 9800, 9800, false, true));
        QCOMPARE(library.track(ids[1]).plays, qint64(1));
    }
    void periodicFinalSaveAndNoLateWrites() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        AuditDatabase audit(library);
        QVERIFY(audit.db.isOpen());
        QVERIFY(audit.exec("CREATE TABLE b9_writes(id INTEGER)"));
        QVERIFY(audit.exec("CREATE TRIGGER b9_save AFTER INSERT ON settings WHEN NEW.key='queue' BEGIN "
                           "INSERT INTO b9_writes VALUES(1); END"));
        QVERIFY(audit.exec("CREATE TRIGGER b9_update AFTER UPDATE ON settings WHEN NEW.key='queue' BEGIN "
                           "INSERT INTO b9_writes VALUES(1); END"));
        auto facade = std::make_unique<PlaybackFacade>(&library, nullptr, true);
        QPointer<Playback> core = facade->backend();
        auto timers = facade->findChildren<QTimer *>("playbackPersistenceTimer");
        QCOMPARE(timers.size(), 1);
        QCOMPARE(timers[0]->interval(), 4000);
        QCOMPARE(timers[0]->thread(), library.thread());
        timers[0]->setInterval(20);
        QSignalSpy saved(facade.get(), &PlaybackFacade::saved);
        QTRY_VERIFY(saved.size() >= 2);
        QCOMPARE(audit.count(), qint64(saved.size()));
        QVERIFY(facade->shutdown());
        const auto count = audit.count();
        QCOMPARE(count, qint64(saved.size()));
        QVERIFY(!timers[0]->isActive());
        QVERIFY(facade->shutdown());
        facade->toggle();
        facade->setVolume(2);
        facade->restore();
        facade->save();
        // Queued backend notifications after shutdown cannot write sessions or settings.
        QMetaObject::invokeMethod(core->media(), "durationChanged", Qt::QueuedConnection,
                                  Q_ARG(qint64, 9999));
        QTest::qWait(80);
        QCOMPARE(audit.count(), count);
        facade.reset();
        QVERIFY(!core);
        QCOMPARE(audit.count(), count);
        PlaybackFacade rebuilt(&library, nullptr, true);
        QVERIFY(rebuilt.save());
        QCOMPARE(audit.count(), count + 1);
    }
    void failedSnapshotRollsBackAndRetries() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        QVERIFY(library.setSettings(
            {{"queue", QJsonArray{42}}, {"current", 42}, {"position", 17}, {"activeSession", 9}}));
        PlaybackFacade facade(&library, nullptr, true);
        AuditDatabase audit(library);
        QSignalSpy failed(&facade, &PlaybackFacade::saveFailed);
        QVERIFY(audit.exec("CREATE TRIGGER b9_fail BEFORE UPDATE ON settings WHEN NEW.key='position' BEGIN "
                           "SELECT RAISE(ABORT,'controlled failure'); END"));
        QVERIFY(!facade.save());
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().first().toString().contains("controlled failure"));
        QCOMPARE(library.setting("queue").toArray(), QJsonArray{42});
        QCOMPARE(library.setting("activeSession").toInt(), 9);
        QVERIFY(audit.exec("DROP TRIGGER b9_fail"));
        QVERIFY(facade.save());
        QVERIFY(library.setting("queue").toArray().isEmpty());
    }
    void errorsSkipAndShutdownCancelsDelayedAdvance_data() {
        QTest::addColumn<bool>("shutdown");
        QTest::newRow("skip") << false;
        QTest::newRow("shutdown-before-skip") << true;
    }
    void errorsSkipAndShutdownCancelsDelayedAdvance() {
        QFETCH(bool, shutdown);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QVERIFY(QFile::remove(library.absolute(library.track(ids[0]).file)));
        PlaybackFacade facade(&library, nullptr, true);
        facade.setMode(Mode::Sequential);
        QSignalSpy errors(&facade, &PlaybackFacade::error);
        facade.play(ids, ids[0]);
        QTRY_VERIFY(!errors.isEmpty());
        if (shutdown) {
            QVERIFY(facade.shutdown());
            QTest::qWait(200);
            QCOMPARE(facade.current().id, ids[0]);
        } else {
            QTRY_COMPARE(facade.current().id, ids[1]);
            QTRY_VERIFY(facade.playing());
        }
    }
    void invalidMediaAndRapidCommands() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QFile bad(library.absolute(library.track(ids[0]).file));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("invalid WAV");
        bad.close();
        PlaybackFacade facade(&library, nullptr, true);
        facade.setMode(Mode::Sequential);
        QSignalSpy errors(&facade, &PlaybackFacade::error);
        facade.play(ids, ids[0]);
        QTRY_VERIFY(!errors.isEmpty());
        QTRY_COMPARE(facade.current().id, ids[1]);
        for (int n = 0; n < 8; ++n) {
            facade.play(ids, ids[1 + n % 2]);
            facade.toggle();
            facade.toggle();
        }
        QCOMPARE(facade.current().id, ids[2]);
        QVERIFY(facade.shutdown());
        QTRY_VERIFY(!facade.playing());
        QVERIFY(facade.backend()->media()->source().isEmpty());
        QVERIFY(!facade.backend()->media()->audioOutput());
    }
    void isolatedPlaybackDoesNotConstructPhysicalOutput() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto previous = qgetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");
        const bool wasSet = qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS");
        const auto restore = qScopeGuard([&] {
            if (wasSet) qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS", previous);
            else qunsetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");
        });
        qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS", "1");
        PlaybackFacade facade(&library);
        QVERIFY(facade.findChildren<QAudioOutput *>().isEmpty());
        QVERIFY(!facade.backend()->media()->audioOutput());
        facade.setVolume(37);
        QCOMPARE(facade.volume(), 37);
        QCOMPARE(library.setting("volume").toInt(), 37);
        QVERIFY(facade.shutdown());
        qunsetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");
        PlaybackFacade decodeOnly(&library, nullptr, true);
        QVERIFY(decodeOnly.findChildren<QAudioOutput *>().isEmpty());
        QVERIFY(!decodeOnly.backend()->media()->audioOutput());
        QVERIFY(decodeOnly.shutdown());
    }
    void endOfMediaKeepsQueueRules_data() {
        QTest::addColumn<int>("mode");
        for (int n = 0; n < 4; ++n)
            QTest::newRow(qPrintable(QString::number(n))) << n;
    }
    void endOfMediaKeepsQueueRules() {
        QFETCH(int, mode);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        PlaybackFacade facade(&library, nullptr, true);
        facade.setMode(Mode(mode));
        facade.play(ids, ids.last());
        QTRY_VERIFY(facade.seekable());
        QMetaObject::invokeMethod(facade.backend()->media(), "mediaStatusChanged", Qt::DirectConnection,
                                  Q_ARG(QMediaPlayer::MediaStatus, QMediaPlayer::EndOfMedia));
        if (mode == int(Mode::SingleLoop))
            QCOMPARE(facade.current().id, ids.last());
        else if (mode == int(Mode::ListLoop))
            QCOMPARE(facade.current().id, ids.first());
        else if (mode == int(Mode::Sequential))
            QVERIFY(!facade.playing());
        else
            QVERIFY(ids.contains(facade.current().id));
    }
    void shutdownFailureIsReportedOnce() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        PlaybackFacade facade(&library, nullptr, true);
        AuditDatabase audit(library);
        QVERIFY(audit.exec("CREATE TRIGGER b9_fail_final BEFORE INSERT ON settings BEGIN SELECT "
                           "RAISE(ABORT,'final save failure'); END"));
        QSignalSpy saved(&facade, &PlaybackFacade::saved), failed(&facade, &PlaybackFacade::saveFailed);
        QVERIFY(!facade.shutdown());
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(saved.size(), 1);
        QVERIFY(!saved[0][0].toBool());
        QVERIFY(!facade.shutdown());
        QCOMPARE(saved.size(), 1);
        QVERIFY(facade.isShutdown());
    }
    void immediateDeletionFromNotificationIsSafe() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        auto *facade = new PlaybackFacade(&library, nullptr, true);
        QPointer<PlaybackFacade> guard = facade;
        QPointer<Playback> core = facade->backend();
        connect(facade, &PlaybackFacade::snapshotChanged, this, [facade] { delete facade; });
        facade->play(ids);
        QTRY_VERIFY(!guard);
        QVERIFY(!core);
    }
    void signalOrderAndDestruction() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QObject owner;
        auto *facade = new PlaybackFacade(&library, &owner, true);
        QPointer<PlaybackFacade> front = facade;
        QPointer<Playback> core = facade->backend();
        QStringList order;
        connect(facade, &PlaybackFacade::snapshotChanged, this, [&](const PlaybackSnapshot &value) {
            order << "snapshot";
            QVERIFY(value.queue.isEmpty() || value.queue.contains(value.track.id));
        });
        connect(facade, &PlaybackFacade::trackChanged, this, [&] { order << "track"; });
        facade->play(ids);
        QVERIFY(order.contains("track"));
        QVERIFY(order.contains("snapshot"));
        QVERIFY(order.indexOf("track") < order.lastIndexOf("snapshot"));
        connect(facade, &PlaybackFacade::snapshotChanged, facade, &QObject::deleteLater);
        facade->setVolume(10);
        QTRY_VERIFY(!front);
        QVERIFY(!core);
    }
};
QTEST_GUILESS_MAIN(PlaybackFacadeTests)
#include "playback-facade-tests.moc"
