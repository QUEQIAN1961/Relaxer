#include "bass-midi-backend.h"
#include "playback.h"
#include "importer.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace relaxer;
namespace {
QByteArray chunk(const QByteArray &events) {
    QByteArray out("MTrk");
    for (int shift : {24,16,8,0}) out += char(quint32(events.size()) >> shift);
    return out + events;
}
QByteArray score(bool dense = false) {
    QByteArray events = QByteArray::fromHex("00ff510307a12000c000");
    if (!dense) events += QByteArray::fromHex("00903c648740803c0000ff2f00");
    else {
        for (int beat=0;beat<16;++beat) {
            for(int note=36;note<100;++note) {
                events += beat && note==36 ? QByteArray::fromHex("8360") : QByteArray(1,'\0');
                events += char(0x90); events += char(note); events += char(70);
            }
            for(int note=36;note<100;++note) {
                events += note==36 ? QByteArray::fromHex("8360") : QByteArray(1,'\0');
                events += char(0x80); events += char(note); events += char(0);
            }
        }
        events += QByteArray::fromHex("00ff2f00");
    }
    return QByteArray::fromHex("4d546864000000060000000101e0") + chunk(events);
}
bool write(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data)==data.size();
}
double energy(const QByteArray &pcm) {
    double sum=0;
    for(qsizetype i=0;i+4<=pcm.size();i+=4) {
        float value; memcpy(&value,pcm.constData()+i,4);
        if(!std::isfinite(value)) return -1;
        sum+=double(value)*value;
    }
    return sum;
}
}
class BassMidiTests : public QObject {
    Q_OBJECT
private slots:
    void realPcmRenderingAndSeekUseUnicodeLongPaths() {
        QTemporaryDir dir;
        const auto path=dir.filePath(QString(90,'a')+"/"+QString(90,'b')+"/中文 日本語 " + QString(80,'c')+".mid");
        const auto original=score(); QVERIFY(write(path,original));
        BassMidiBackend backend;
        QVERIFY2(backend.open(path,true),qPrintable(backend.error()));
        QCOMPARE(backend.duration(),qint64(1000));
        QVERIFY(backend.setVolume(30));
        const auto pcm=backend.readPcm(12000); QCOMPARE(pcm.size(),12000*8);
        QVERIFY(energy(pcm)>0.001); QVERIFY(backend.position()>=249 && backend.position()<=251);
        QVERIFY(backend.seek(500)); QVERIFY(qAbs(backend.position()-500)<=1);
        QVERIFY(energy(backend.readPcm(12000))>0.001);
        QVERIFY(backend.close()); QVERIFY(backend.close());
#ifdef Q_OS_WIN
        QVERIFY(!GetModuleHandleW(L"bassmidi.dll")); QVERIFY(!GetModuleHandleW(L"bass.dll"));
#endif
        QFile input(path); QVERIFY(input.open(QIODevice::ReadOnly)); QCOMPARE(input.readAll(),original);
    }
    void simultaneousStreamsHaveIndependentOwnership() {
        QTemporaryDir dir; const auto path=dir.filePath("sample.mid"); QVERIFY(write(path,score()));
        BassMidiBackend first,second;
        QVERIFY2(first.open(path,true),qPrintable(first.error()));
        QVERIFY2(second.open(path,true),qPrintable(second.error()));
        QVERIFY(energy(first.readPcm(4800))>0.001); QCOMPARE(second.position(),qint64(0));
        QVERIFY(first.close());
        QVERIFY(energy(second.readPcm(4800))>0.001); QVERIFY(second.position()>0);
        QVERIFY(second.close()); QVERIFY2(first.open(path,true),qPrintable(first.error())); QVERIFY(first.close());
    }
    void denseScoreRendersWithinRealtimeBudget() {
        QTemporaryDir dir; const auto path=dir.filePath("dense.mid"); QVERIFY(write(path,score(true)));
        BassMidiBackend backend; QElapsedTimer clock; clock.start();
        QVERIFY2(backend.open(path,true),qPrintable(backend.error()));
        const auto prepare=clock.elapsed(); clock.restart(); qint64 bytes=0; double sum=0;
        for(;;) { const auto pcm=backend.readPcm(4096); if(pcm.isEmpty())break; bytes+=pcm.size(); const auto e=energy(pcm); QVERIFY(e>=0);sum+=e; }
        const auto render=clock.elapsed(); const double seconds=double(bytes)/(48000*8);
        QVERIFY(seconds>7); QVERIFY(sum>0.01); QVERIFY(render<seconds*1000);
        QCOMPARE(qRound64(seconds*1000),backend.duration());
        qInfo().noquote()<<"BASS_MIDI_RENDER_EVIDENCE"<<QJsonDocument(QJsonObject{{"prepareMs",double(prepare)},{"renderMs",double(render)},{"audioSeconds",seconds},{"energy",sum}}).toJson(QJsonDocument::Compact);
        QVERIFY(backend.close());
    }
    void invalidFileAndFailedOpenRemainClosable() {
        QTemporaryDir dir; const auto path=dir.filePath("bad.mid"); QVERIFY(write(path,"not a MIDI file"));
        BassMidiBackend backend; QVERIFY(!backend.open(path,true)); QVERIFY(!backend.error().isEmpty());
        QVERIFY(backend.close()); QVERIFY(write(path,score()));
        QVERIFY2(backend.open(path,true),qPrintable(backend.error())); QVERIFY(backend.close());
        QVERIFY(!backend.play()); QVERIFY(!backend.error().isEmpty()); QVERIFY(backend.close());
    }
    void nativePlaybackPauseSeekResume() {
        if (!qEnvironmentVariableIsSet("RELAXER_MIDI_AUDIO_TEST")) QSKIP("Explicit native output test; offline PCM tests run above");
        QTemporaryDir dir; const auto path=dir.filePath("native.mid"); QVERIFY(write(path,score(true)));
        BassMidiBackend backend; QVERIFY2(backend.open(path),qPrintable(backend.error()));
        QVERIFY(backend.setVolume(0)); QVERIFY(backend.play()); QTest::qWait(450);
        QVERIFY(backend.position()>100); QVERIFY(backend.pause()); const auto paused=backend.position();
        QTest::qWait(120); const auto afterPause=backend.position();
        QVERIFY2(qAbs(afterPause-paused)<20,qPrintable(QString("paused=%1 after=%2").arg(paused).arg(afterPause)));
        QVERIFY(backend.seek(2000)); QVERIFY(qAbs(backend.position()-2000)<20);
        QVERIFY(backend.play()); QTest::qWait(200); QVERIFY(backend.position()>2100);
        QVERIFY(backend.close());
    }
    void nativePlayerQueueSwitchesBetweenMidiAndAudio() {
        if (!qEnvironmentVariableIsSet("RELAXER_MIDI_AUDIO_TEST")) QSKIP("Explicit native output integration test");
        QTemporaryDir dir; Library library(dir.filePath("data")); QVERIFY(library.open());
        QVERIFY(library.setSetting("volume",0));
        const auto first=dir.filePath("源 音乐/first.mid"), second=dir.filePath("源 音乐/second.mid");
        QVERIFY(write(first,score())); QVERIFY(write(second,score(true)));
        const auto a=Importer::importOne(library.root(),first), b=Importer::importOne(library.root(),second);
        QVERIFY2(a.error.isEmpty(),qPrintable(a.error)); QVERIFY(b.error.isEmpty());
        const auto firstId=library.insert(a.track), secondId=library.insert(b.track); QVERIFY(firstId); QVERIFY(secondId);
        QByteArray wave; QDataStream pcm(&wave,QIODevice::WriteOnly);pcm.setByteOrder(QDataStream::LittleEndian);
        pcm.writeRawData("RIFF",4);pcm<<quint32(320036);pcm.writeRawData("WAVEfmt ",8);
        pcm<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
        pcm.writeRawData("data",4);pcm<<quint32(320000);pcm.writeRawData(QByteArray(320000,'\0').constData(),320000);
        const auto audio=dir.filePath("audio.wav"); QVERIFY(write(audio,wave));
        const auto c=Importer::importOne(library.root(),audio); QVERIFY2(c.error.isEmpty(),qPrintable(c.error));
        const auto audioId=library.insert(c.track); QVERIFY(audioId);
        Playback player(&library); QSignalSpy errors(&player,&Playback::error);
        player.play({firstId,secondId,audioId},firstId); QVERIFY(player.playing());
        QTest::qWait(250); QVERIFY(player.position()>50);
        player.toggle(); const auto paused=player.position(); QTest::qWait(120); QCOMPARE(player.position(),paused);
        player.seek(500); QCOMPARE(player.position(),qint64(500)); player.toggle();
        QTRY_COMPARE(player.current().id,secondId); QVERIFY(player.playing());
        player.next(); QTRY_COMPARE(player.current().id,audioId);
        QTRY_VERIFY(player.media()->source().toLocalFile().endsWith(".wav"));
#ifdef Q_OS_WIN
        QVERIFY(!GetModuleHandleW(L"bassmidi.dll")); QVERIFY(!GetModuleHandleW(L"bass.dll"));
#endif
        player.previous(); QCOMPARE(player.current().id,secondId); QVERIFY(player.playing());
        QVERIFY(player.save()); QVERIFY(player.shutdown()); QVERIFY(!player.playing()); QCOMPARE(errors.size(),0);
        QFile original(first); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(),score());
    }
};
QTEST_GUILESS_MAIN(BassMidiTests)
#include "bass-midi-tests.moc"
