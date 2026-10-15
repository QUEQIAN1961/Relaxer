#include "playback.h"
#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

using namespace relaxer;

class QueuePlaybackTests : public QObject {
    Q_OBJECT
    static QVector<qint64> seed(Library &library) {
        QVector<qint64> ids;
        for (int i = 0; i < 7; ++i) {
            Track track;
            track.hash = QString("b2-queue-%1").arg(i);
            track.title = track.hash;
            track.file = QString("Music/queue-%1.wav").arg(i);
            track.duration = 30000;
            QFile file(library.absolute(track.file));
            if (!file.open(QIODevice::WriteOnly))
                return {};
            QDataStream out(&file);
            out.setByteOrder(QDataStream::LittleEndian);
            const quint32 bytes = 30000 * 16; // 30s, 8kHz, mono PCM.
            out.writeRawData("RIFF", 4);
            out << quint32(36 + bytes);
            out.writeRawData("WAVEfmt ", 8);
            out << quint32(16) << quint16(1) << quint16(1) << quint32(8000)
                << quint32(16000) << quint16(2) << quint16(16);
            out.writeRawData("data", 4);
            out << bytes;
            const QByteArray silence(bytes, '\0');
            out.writeRawData(silence.constData(), silence.size());
            if (out.status() != QDataStream::Ok)
                return {};
            file.close();
            const auto id = library.insert(track);
            if (!id)
                return {};
            ids << id;
        }
        return ids;
    }
    static QJsonArray jsonIds(const QVector<qint64> &ids) {
        QJsonArray result;
        for (auto id : ids)
            result << double(id);
        return result;
    }

  private slots:
    void restoresFiltersAndPersistsTheSameQueue() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVector<qint64> ids;
        {
            Library library(dir.filePath("data"));
            QVERIFY(library.open());
            ids = seed(library);
            QCOMPARE(ids.size(), 7);
            auto stored = jsonIds(ids);
            stored << double(ids[0]) << 999999.0 << 0.0 << -1.0;
            library.setSetting("queue", stored);
            library.setSetting("current", double(ids[3]));
            library.setSetting("mode", int(Mode::SingleLoop));
            Playback playback(&library, nullptr, true);
            playback.restore();
            QCOMPARE(playback.queue().items, ids);
            QCOMPARE(playback.current().id, ids[3]);
            QCOMPARE(playback.mode(), Mode::SingleLoop);
            QVERIFY(!playback.playing());
            playback.save();
            QCOMPARE(library.setting("queue").toArray(), jsonIds(ids));
            QCOMPARE(qint64(library.setting("current").toDouble()), ids[3]);
            QCOMPARE(library.setting("mode").toInt(), int(Mode::SingleLoop));
        }
        Library reopened(dir.filePath("data"));
        QVERIFY(reopened.open());
        Playback restored(&reopened, nullptr, true);
        restored.restore();
        QCOMPARE(restored.queue().items, ids);
        QCOMPARE(restored.current().id, ids[3]);
        QCOMPARE(restored.mode(), Mode::SingleLoop);
        QVERIFY(!restored.playing());
    }
    void jumpUsesTheDisplayedForecastAndRejectsInvalidIndices_data() {
        QTest::addColumn<int>("modeValue");
        QTest::newRow("sequential") << int(Mode::Sequential);
        QTest::newRow("list-loop") << int(Mode::ListLoop);
        QTest::newRow("single-loop") << int(Mode::SingleLoop);
        QTest::newRow("shuffle") << int(Mode::Shuffle);
    }
    void jumpUsesTheDisplayedForecastAndRejectsInvalidIndices() {
        QFETCH(int, modeValue);
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QCOMPARE(ids.size(), 7);
        Playback playback(&library, nullptr, true);
        playback.setMode(static_cast<Mode>(modeValue));
        playback.play(ids, ids[0]);
        QCOMPARE(playback.queue().items, ids);
        QCOMPARE(playback.current().id, ids[0]);
        const auto preview = playback.upcoming();
        QCOMPARE(playback.upcoming(), preview);
        playback.playUpcoming(-1);
        playback.playUpcoming(preview.size());
        QCOMPARE(playback.current().id, ids[0]);
        playback.playUpcoming(3);
        QCOMPARE(playback.current().id, preview[3]);
        QCOMPARE(playback.queue().current(), preview[3]);
        QCOMPARE(qint64(library.setting("current").toDouble()), preview[3]);
        if (static_cast<Mode>(modeValue) == Mode::Shuffle) {
            playback.previous();
            QCOMPARE(playback.current().id, preview[2]);
            QCOMPARE(playback.upcoming().first(), preview[3]);
            playback.next();
            QCOMPARE(playback.current().id, preview[3]);
        }
        playback.setMode(Mode::ListLoop);
        QCOMPARE(library.setting("mode").toInt(), int(Mode::ListLoop));
    }
    void forgettingTracksInvalidatesTheForecastAndSavesChanges() {
        QTemporaryDir dir;
        Library library(dir.filePath("data"));
        QVERIFY(library.open());
        const auto ids = seed(library);
        QCOMPARE(ids.size(), 7);
        library.setSetting("queue", jsonIds(ids));
        library.setSetting("current", double(ids[3]));
        library.setSetting("mode", int(Mode::Shuffle));
        Playback playback(&library, nullptr, true);
        playback.restore();
        playback.upcoming();
        playback.forget({ids[0], ids[6]});
        const QVector<qint64> remaining{ids[1], ids[2], ids[3], ids[4], ids[5]};
        QCOMPARE(playback.queue().items, remaining);
        QCOMPARE(playback.current().id, ids[3]);
        QCOMPARE(library.setting("queue").toArray(), jsonIds(remaining));
        const auto preview = playback.upcoming();
        for (auto id : preview)
            QVERIFY(remaining.contains(id));
        playback.forget({ids[3]});
        QCOMPARE(playback.current().id, ids[1]);
        QVERIFY(!playback.playing());
        QVERIFY(!playback.queue().items.contains(ids[3]));
        playback.forget(ids);
        QVERIFY(playback.queue().items.isEmpty());
        QCOMPARE(playback.current().id, qint64(0));
        QVERIFY(playback.upcoming().isEmpty());
        QVERIFY(library.setting("queue").toArray().isEmpty());
        QCOMPARE(library.setting("current").toDouble(), 0.0);
    }
};

QTEST_GUILESS_MAIN(QueuePlaybackTests)
#include "queue-playback-tests.moc"
