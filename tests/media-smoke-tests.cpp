#include "media-smoke.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
using namespace relaxer;
class MediaSmokeTests : public QObject {
    Q_OBJECT
    static QString executable() {
        return QCoreApplication::applicationDirPath() + "/Relaxer"
#ifdef Q_OS_WIN
            ".exe"
#endif
            ;
    }
    static bool wav(const QString &path, int milliseconds = 2000) {
        const quint32 size = milliseconds * 16; // 8kHz, mono, signed 16-bit PCM.
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF",4);out<<quint32(36+size);out.writeRawData("WAVEfmt ",8);
        out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
        out.writeRawData("data",4);out<<size;out.writeRawData(QByteArray(size,'\0').constData(),size);
        return out.status()==QDataStream::Ok;
    }
private slots:
    void actualQtDecodeWithoutAudioServerOrDatabase() {
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto source=dir.filePath("音频验证 with spaces.wav");QVERIFY(wav(source));
        const auto data=dir.filePath("must-not-create-library");
        const auto result=runMediaSmokeProcess(executable(),{"--media-smoke",source,"--data",data},25000);
        QVERIFY2(mediaSmokeSucceeded(result),result.error.constData());
        QVERIFY(result.output.contains("RELAXER_MEDIA_DECODE_OK"));
        QVERIFY(!QFileInfo::exists(data)); // Verification never opens a library/lock in either process.
    }
    void nonexistentFileFailsAndCannotCreateReadyProof() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto result=runMediaSmokeProcess(executable(),{"--media-smoke",dir.filePath("missing.wav")},5000);
        QVERIFY(!result.timedOut);QVERIFY(result.exitCode!=0);QVERIFY(!mediaSmokeSucceeded(result));
        QVERIFY(result.error.contains("missing or unreadable"));
    }
    void invalidAndTooShortAudioFail_data() {
        QTest::addColumn<bool>("validFormat");QTest::newRow("corrupted-audio")<<false;QTest::newRow("insufficient-decoded-frames")<<true;
    }
    void invalidAndTooShortAudioFail() {
        QFETCH(bool,validFormat);QTemporaryDir dir;QVERIFY(dir.isValid());const auto source=dir.filePath("invalid.wav");
        if(validFormat)QVERIFY(wav(source,100));
        else {QFile file(source);QVERIFY(file.open(QIODevice::WriteOnly));file.write("this is not audio");file.close();}
        const auto result=runMediaSmokeProcess(executable(),{"--media-smoke",source},15000);
        QVERIFY2(!result.timedOut,result.error.constData());QVERIFY(result.exitCode!=0);QVERIFY(!mediaSmokeSucceeded(result));
    }
    void stuckWorkerIsTerminatedWithinDeadline() {
        QElapsedTimer elapsed;elapsed.start();
        const auto result=runMediaSmokeProcess(QCoreApplication::applicationFilePath(),{"--fixture-hang"},200);
        QVERIFY(result.timedOut);QCOMPARE(result.exitCode,6);QVERIFY(elapsed.elapsed()<3000);QVERIFY(!mediaSmokeSucceeded(result));
    }
    void zeroExitWithoutDecodedFramesIsNotSuccess() {
        const auto result=runMediaSmokeProcess(QCoreApplication::applicationFilePath(),{"--fixture-unproven-success"},3000);
        QCOMPARE(result.exitCode,0);QVERIFY(!result.timedOut);QVERIFY(!mediaSmokeSucceeded(result));
        MediaSmokeResult fake;fake.exitCode=0;fake.output="RELAXER_MEDIA_DECODE_OK {\"decoded_us\":500000,\"frames\":0}";
        QVERIFY(!mediaSmokeSucceeded(fake));
    }
    void immediateStartFailureDoesNotWaitOrBecomeSuccessful() {
        QProcess process;bool errorDelivered=false,exitDelivered=false;
        // Windows may emit FailedToStart inside start(), before the nested loop.
        const auto result=media_smoke_detail::supervise(process,[&] {
            errorDelivered=QMetaObject::invokeMethod(&process,"errorOccurred",Qt::DirectConnection,
                Q_ARG(QProcess::ProcessError,QProcess::FailedToStart));
            exitDelivered=QMetaObject::invokeMethod(&process,"finished",Qt::DirectConnection,
                Q_ARG(int,0),Q_ARG(QProcess::ExitStatus,QProcess::NormalExit));
        },400);
        QVERIFY(errorDelivered);QVERIFY(exitDelivered);
        QVERIFY(!result.timedOut);QCOMPARE(result.exitCode,5);
        QVERIFY(!result.error.isEmpty());QVERIFY(!mediaSmokeSucceeded(result));
    }
    void immediateExitDoesNotWaitForTheDeadline() {
        QProcess process;bool delivered=false;
        const auto result=media_smoke_detail::supervise(process,[&] {
            delivered=QMetaObject::invokeMethod(&process,"finished",Qt::DirectConnection,
                Q_ARG(int,0),Q_ARG(QProcess::ExitStatus,QProcess::NormalExit));
        },400);
        QVERIFY(delivered);QVERIFY(!result.timedOut);QCOMPARE(result.exitCode,0);
        QVERIFY(!mediaSmokeSucceeded(result)); // A zero exit still requires real PCM proof.
    }
    void failedProcessLaunchIsNotSuccess() {
        QTemporaryDir dir;const auto result=runMediaSmokeProcess(dir.filePath("nonexistent-program"),{},1000);
        QVERIFY(result.exitCode!=0);QVERIFY(!mediaSmokeSucceeded(result));QVERIFY(!result.timedOut);
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--fixture-hang")){QTimer::singleShot(10000,&app,&QCoreApplication::quit);return app.exec();}
    if(app.arguments().contains("--fixture-unproven-success"))return 0;
    // No sound server is available to these checks. Actual PCM decode remains mandatory.
    qputenv("PULSE_SERVER","unix:/relaxer-test-no-audio-server");
    MediaSmokeTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "media-smoke-tests.moc"
