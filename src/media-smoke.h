#pragma once
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>
#include <QTimer>
#include <cstdlib>

namespace relaxer {
struct MediaSmokeResult {
    int exitCode = 6;
    bool timedOut = false;
    QByteArray output, error;
};
// This supervisor never enumerates audio devices or constructs a multimedia backend.
// A native decoder/driver hang therefore cannot block its event-loop timeout.
namespace media_smoke_detail {
template<class StartProcess>
inline MediaSmokeResult supervise(QProcess &process, StartProcess startProcess, int timeoutMs) {
    MediaSmokeResult result;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    bool completed = false;
    const auto complete = [&](int code, bool timedOut = false) {
        if (completed) return;
        completed = true;
        result.exitCode = code;
        result.timedOut = timedOut;
        loop.quit();
    };
    QObject::connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &loop,
                     [&](int code, QProcess::ExitStatus status) {
        complete(status == QProcess::NormalExit ? code : 5);
    });
    QObject::connect(&process, &QProcess::errorOccurred, &loop, [&](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !completed) {
            result.error = process.errorString().toUtf8();
            complete(5);
        }
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
        if (completed) return;
        complete(6, true);
        process.kill();
    });
    deadline.start(qMax(1, timeoutMs));
    startProcess();
    // Windows can emit FailedToStart before start() returns. quit() before
    // exec() is not a stored completion: do not enter a new loop after a result.
    if (!completed) loop.exec();
    deadline.stop();
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(1000);
    }
    if (process.isOpen()) {
        result.output = process.readAllStandardOutput();
        result.error += process.readAllStandardError();
    }
    if (result.timedOut) {
        result.exitCode = 6;
        result.error += "\nQt audio decoder worker timed out; worker terminated.\n";
    }
    return result;
}
} // namespace media_smoke_detail
inline MediaSmokeResult runMediaSmokeProcess(const QString &program, const QStringList &arguments,
                                            int timeoutMs = 20000) {
    QProcess process;
    return media_smoke_detail::supervise(process, [&] { process.start(program, arguments); }, timeoutMs);
}
inline bool mediaSmokeSucceeded(const MediaSmokeResult &result) {
    if (result.timedOut || result.exitCode != 0) return false;
    const QByteArray prefix("RELAXER_MEDIA_DECODE_OK ");
    for (const auto &line : result.output.split('\n')) {
        if (!line.startsWith(prefix)) continue;
        const auto proof = QJsonDocument::fromJson(line.mid(prefix.size())).object();
        if (proof["decoded_us"].toDouble() >= 500000 && proof["frames"].toDouble() > 0 &&
            proof["sample_rate"].toInt() > 0 && proof["channels"].toInt() > 0 &&
            proof["audio_device_required"].isBool() && !proof["audio_device_required"].toBool()) return true;
    }
    return false;
}
// Only the short-lived --media-smoke-worker process enters this function.
// It opens no library/database and writes no persistent files. After a real PCM
// result is flushed, OS process cleanup avoids re-entering Qt backend shutdown,
// which can itself hang with some multimedia drivers. The GUI never uses _Exit.
[[noreturn]] inline void mediaSmokeWorker(const QString &source) {
    auto finish = [](int code, const QString &message) {
        QTextStream stream(code == 0 ? stdout : stderr);
        stream << message << '\n';
        stream.flush();
        std::_Exit(code);
    };
    const QFileInfo info(source);
    if (!info.isFile() || !info.isReadable()) finish(5, "Audio verification input is missing or unreadable.");
    QTextStream(stderr) << "Media verification: initializing Qt decoder (no audio output device).\n" << Qt::flush;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, QCoreApplication::instance(), [&] {
        finish(6, "Qt decoder produced less than 500 ms of valid PCM before its deadline.");
    });
    deadline.start(10000);
    QAudioDecoder decoder;
    if (!decoder.isSupported()) finish(5, "Qt audio decoding backend is unavailable.");
    qint64 frames = 0, decodedUs = 0;
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),
                     QCoreApplication::instance(), [&](QAudioDecoder::Error) {
        finish(5, "Qt audio decode failed: " + decoder.errorString());
    });
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, QCoreApplication::instance(), [&] {
        const auto buffer = decoder.read();
        if (!buffer.isValid() || !buffer.format().isValid() || buffer.frameCount() <= 0 || buffer.byteCount() <= 0)
            finish(5, "Qt decoder returned an invalid PCM buffer.");
        frames += buffer.frameCount();
        decodedUs += buffer.duration();
        if (decodedUs < 500000) return;
        const QJsonObject proof{{"decoded_us", double(decodedUs)}, {"frames", double(frames)},
                                {"sample_rate", buffer.format().sampleRate()},
                                {"channels", buffer.format().channelCount()},
                                {"audio_device_required", false}};
        finish(0, "RELAXER_MEDIA_DECODE_OK " + QString::fromUtf8(QJsonDocument(proof).toJson(QJsonDocument::Compact)));
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, QCoreApplication::instance(), [&] {
        finish(5, "Qt decoder finished without producing 500 ms of valid PCM.");
    });
    // QFile preserves Unicode paths on every platform; older FFmpeg plugins can
    // misinterpret non-ASCII filenames when opening a URL themselves.
    QFile input(info.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly)) finish(5, "Audio verification input could not be opened.");
    decoder.setSourceDevice(&input);
    QTimer::singleShot(0, &decoder, &QAudioDecoder::start);
    QCoreApplication::exec();
    finish(6, "Qt audio verification event loop stopped without a decoding result.");
    std::_Exit(6);
}
} // namespace relaxer
