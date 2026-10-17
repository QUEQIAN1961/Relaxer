#include "importer.h"
#include "midi-transport.h"
#include "playback-facade.h"
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace relaxer;
namespace {
QByteArray midi(int format = 0) {
    auto track = [](QByteArray bytes) {
        QByteArray out("MTrk");
        const quint32 n = bytes.size();
        for (int shift : {24, 16, 8, 0})
            out += char(n >> shift);
        return out + bytes;
    };
    const auto tempo = QByteArray::fromHex("00ff510307a120");
    const auto notes = QByteArray::fromHex("00c00000903c308740803c0000ff2f00");
    QByteArray header =
        QByteArray::fromHex(format == 0 ? "4d546864000000060000000101e0" : "4d546864000000060001000201e0");
    return header + (format == 0 ? track(tempo + notes)
                                 : track(tempo + QByteArray::fromHex("8740ff2f00")) + track(notes));
}
bool write(const QString &path, QByteArray data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}
struct FakeMidi {
    QStringList commands;
    QString failOn;
    quint32 code = 275;
    qint64 position = 0;
    bool playing = false;
    quint32 send(const QString &cmd, QString *result) {
        commands << cmd;
        if (!failOn.isEmpty() && cmd.startsWith(failOn))
            return code;
        if (cmd.endsWith(" length") && result)
            *result = "5000";
        if (cmd.endsWith(" position") && result)
            *result = QString::number(position);
        if (cmd.startsWith("play "))
            playing = true;
        if (cmd.startsWith("pause ") || cmd.startsWith("stop ") || cmd.startsWith("close "))
            playing = false;
        return 0;
    }
};
} // namespace
namespace relaxer {
class PlaybackDiagnostics {
  public:
    static void backend(Playback &p, FakeMidi &fake) {
        p.m_midiBackend = std::make_unique<MidiTransport>(
            p.m_library->root(), [&fake](auto c, auto r) { return fake.send(c, r); });
    }
    static bool sample(Playback &p, qint64 now, qint64 pos, bool finish = false) {
        return p.sampleAt(now, pos, true, finish, false);
    }
    static void clock(Playback &p) {
        p.m_lastWall = 0;
        p.m_lastPosition = 0;
    }
};
} // namespace relaxer
class MidiTests : public QObject {
    Q_OBJECT
  private slots:
    void nativeOpen_data() {
        QTest::addColumn<int>("format");
        QTest::addColumn<QString>("name");
        QTest::newRow("format0") << 0 << QString("a.mid");
        QTest::newRow("format1") << 1 << QString("format1.mid");
        QTest::newRow("relative") << 0 << QString("relative.mid");
        QTest::newRow("unicode-spaces") << 1 << QString("中文 文件夹/歌曲 空格.mid");
        QTest::newRow("import-hash") << 0 << QString("Music/") + QString(64, 'e') + ".mid";
        QTest::newRow("over260") << 1
                                 << (QString(80, 'a') + "/" + QString(80, 'b') + "/" + QString(80, 'c') +
                                     ".mid");
    }
    void nativeOpen() {
        QFETCH(int, format);
        QFETCH(QString, name);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(name);
        QVERIFY(write(path, midi(format)));
        MidiTransport transport(dir.path());
        const auto input =
            QString(QTest::currentDataTag()) == "relative" ? QDir::current().relativeFilePath(path) : path;
        QVERIFY2(transport.open(input),
                 qPrintable(transport.error() + " open=" + transport.openedPath() +
                            " chars=" + QString::number(transport.openedPath().size())));
        QVERIFY(transport.openedPath().size() < 128);
        QVERIFY(QFileInfo::exists(transport.openedPath()));
        QVERIFY(transport.duration() >= 990 && transport.duration() <= 1010);
        QVERIFY2(transport.close(), qPrintable(transport.error()));
        QVERIFY(!transport.isOpen());
        QVERIFY(transport.open(path));
        QVERIFY(transport.close());
        QFile source(path);
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), midi(format));
    }
    void fallbackCopyAndLifetime() {
        QTemporaryDir dir;
        const auto path = dir.filePath(QString(100, 'x') + ".mid");
        QVERIFY(write(path, midi()));
        FakeMidi fake;
        MidiTransport transport(
            dir.path(), [&](auto c, auto r) { return fake.send(c, r); }, [](auto p) { return p; });
        QVERIFY2(transport.open(path), qPrintable(transport.error()));
        const auto copy = transport.openedPath();
        QVERIFY(copy != path);
        QVERIFY(QFileInfo::exists(copy));
        QFile f(copy);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), midi());
        f.close();
        QVERIFY(transport.close());
        QVERIFY(!QFileInfo::exists(copy));
        QVERIFY(QFileInfo::exists(path));
    }
    void invalidFiles_data() {
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("not-midi") << QByteArray("not midi");
        QTest::newRow("truncated-chunk") << midi().left(20);
    }
    void invalidFiles() {
        QFETCH(QByteArray, data);
        QTemporaryDir dir;
        const auto path = dir.filePath("bad.mid");
        QVERIFY(write(path, data));
        MidiTransport transport(dir.path());
        QVERIFY(!transport.open(path));
        QVERIFY(!transport.isOpen());
        QVERIFY(!transport.error().isEmpty());
    }
    void missingAndBackendErrors() {
        QTemporaryDir dir;
        FakeMidi fake;
        MidiTransport transport(dir.path(), [&](auto c, auto r) { return fake.send(c, r); });
        QVERIFY(!transport.open(dir.filePath("missing.mid")));
        QVERIFY(fake.commands.isEmpty());
        const auto path = dir.filePath("ok.mid");
        QVERIFY(write(path, midi()));
        fake.failOn = "open ";
        QVERIFY(!transport.open(path));
        QCOMPARE(transport.errorCode(), quint32(275));
        QVERIFY(!transport.isOpen());
        fake.failOn.clear();
        QVERIFY(transport.open(path));
        fake.failOn = "play ";
        fake.code = 68;
        QVERIFY(!transport.play());
        QCOMPARE(transport.errorCode(), quint32(68));
        QVERIFY(!fake.playing);
        fake.failOn.clear();
        QVERIFY(transport.play());
        QVERIFY(transport.pause());
        QVERIFY(transport.seek(500));
        QVERIFY(transport.play());
        QVERIFY(transport.close());
        QFile log(dir.filePath("logs/midi.jsonl"));
        QVERIFY(log.open(QIODevice::ReadOnly));
        const auto data = log.readAll();
        QVERIFY(data.contains("275"));
        QVERIFY(data.contains("outputDevices"));
        QVERIFY(data.contains("openedPath"));
    }
    void importedPathsPlaybackAndStatistics() {
        QTemporaryDir dir;
        Library lib(dir.filePath("data"));
        QVERIFY(lib.open());
        const auto p0 = dir.filePath("源 音乐/first.mid"), p1 = dir.filePath("other/first.mid");
        QVERIFY(write(p0, midi(0)));
        QVERIFY(write(p1, midi(1)));
        const auto a = Importer::importOne(lib.root(), p0), b = Importer::importOne(lib.root(), p1);
        QVERIFY2(a.error.isEmpty(), qPrintable(a.error));
        QVERIFY(b.error.isEmpty());
        QVERIFY(a.track.file.startsWith("Music/"));
        QCOMPARE(QFileInfo(a.track.file).completeBaseName().size(), 64);
        QVERIFY(QFileInfo::exists(lib.absolute(a.track.file)));
        QVERIFY(a.track.hash != b.track.hash);
        const auto id0 = lib.insert(a.track), id1 = lib.insert(b.track);
        QVERIFY(id0 && id1);
        FakeMidi fake;
        Playback p(&lib, nullptr, true);
        PlaybackDiagnostics::backend(p, fake);
        QSignalSpy errors(&p, &Playback::error);
        p.play({id0, id1}, id0);
        QVERIFY(p.playing());
        QCOMPARE(p.duration(), qint64(5000));
        p.toggle();
        QVERIFY(!p.playing());
        p.toggle();
        QVERIFY(p.playing());
        p.seek(1000);
        QVERIFY(p.playing());
        p.next();
        QCOMPARE(p.current().id, id1);
        QVERIFY(p.playing());
        for (int n = 0; n < 10; ++n) {
            p.next();
            QVERIFY(p.playing());
        }
        p.play({id0}, id0);
        PlaybackDiagnostics::clock(p);
        QVERIFY(PlaybackDiagnostics::sample(p, 4500, 4500));
        QVERIFY(lib.track(id0).heard >= 4500);
        QCOMPARE(lib.track(id0).plays, qint64(1));
        QVERIFY(p.save());
        QVERIFY(p.shutdown());
        QVERIFY(!p.playing());
        QVERIFY(!fake.playing);
        QCOMPARE(errors.count(), 0);
        const auto last = fake.commands.size();
        p.toggle();
        p.next();
        QCOMPARE(fake.commands.size(), last);
    }
    void playbackFailureNeverReportsPlaying() {
        QTemporaryDir dir;
        Library lib(dir.path());
        QVERIFY(lib.open());
        Track t;
        t.hash = "midi";
        t.title = "bad backend";
        t.file = "Music/a.mid";
        QVERIFY(write(lib.absolute(t.file), midi()));
        const auto id = lib.insert(t);
        QVERIFY(id);
        FakeMidi fake;
        fake.failOn = "play ";
        Playback p(&lib, nullptr, true);
        PlaybackDiagnostics::backend(p, fake);
        QSignalSpy errors(&p, &Playback::error);
        p.play({id}, id);
        QVERIFY(!p.playing());
        QVERIFY(!fake.playing);
        QCOMPARE(errors.count(), 1);
        QVERIFY(p.shutdown());
    }
    void cacheFailureAndExclusiveFileLock() {
        QTemporaryDir dir;
        const auto path = dir.filePath(QString(100, 'x') + ".mid");
        QVERIFY(write(path, midi()));
        FakeMidi fake;
        MidiTransport badCache(
            dir.path(), [&](auto c, auto r) { return fake.send(c, r); },
            [](auto p) { return p + QString(150, 'x'); });
        QVERIFY(!badCache.open(path));
        QVERIFY(fake.commands.isEmpty());
        QVERIFY(badCache.error().contains("cache"));
        const auto native = QDir::toNativeSeparators(path).toStdWString();
        const auto file = CreateFileW(native.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(file != INVALID_HANDLE_VALUE);
        MidiTransport locked(dir.path());
        const bool opened = locked.open(path);
        CloseHandle(file);
        QVERIFY(!opened);
        QVERIFY(locked.error().contains("read"));
        QVERIFY(QFileInfo::exists(path));
    }
    void midiAudioSwitchAndEndOfTrack() {
        QTemporaryDir dir;
        Library lib(dir.path());
        QVERIFY(lib.open());
        Track mid;
        mid.hash = "mid";
        mid.file = "Music/test.mid";
        mid.title = "MIDI";
        QVERIFY(write(lib.absolute(mid.file), midi()));
        const auto a = lib.insert(mid);
        Track wav;
        wav.hash = "wav";
        wav.file = "Music/test.wav";
        wav.title = "WAV";
        wav.duration = 10000;
        const quint32 size = 160000;
        QByteArray wave;
        {
            QDataStream out(&wave, QIODevice::WriteOnly);
            out.setByteOrder(QDataStream::LittleEndian);
            out.writeRawData("RIFF", 4);
            out << quint32(size + 36);
            out.writeRawData("WAVEfmt ", 8);
            out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2)
                << quint16(16);
            out.writeRawData("data", 4);
            out << size;
            wave += QByteArray(size, '\0');
        }
        QVERIFY(write(lib.absolute(wav.file), wave));
        const auto b = lib.insert(wav);
        QVERIFY(a && b);
        FakeMidi fake;
        Playback p(&lib, nullptr, true);
        PlaybackDiagnostics::backend(p, fake);
        p.play({a, b}, a);
        QVERIFY(p.playing());
        fake.position = 4999;
        QTRY_COMPARE(p.current().id, b);
        QVERIFY(!fake.playing);
        QTRY_VERIFY(p.media()->source().toLocalFile().endsWith("test.wav"));
        p.previous();
        QCOMPARE(p.current().id, a);
        QVERIFY(fake.playing);
        p.next();
        QCOMPARE(p.current().id, b);
        QVERIFY(!fake.playing);
        p.play({a, b}, a);
        QVERIFY(fake.playing);
        QVERIFY(p.shutdown());
        QVERIFY(!fake.playing);
    }
};
QTEST_GUILESS_MAIN(MidiTests)
#include "midi-tests.moc"
