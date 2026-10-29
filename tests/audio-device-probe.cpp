// Qt-only environment control: deliberately links no Relaxer implementation.
#include <QCoreApplication>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <memory>

static void log(const QString &text) { QTextStream(stdout) << text << Qt::endl; }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2) return 64;
    const auto devices = QMediaDevices::audioOutputs();
    const auto device = QMediaDevices::defaultAudioOutput();
    log("Qt=" + QString::fromLatin1(qVersion()) + " outputCount=" + QString::number(devices.size()));
    for (const auto &item : devices) {
        const auto format = item.preferredFormat();
        log("device=" + item.description() + " id=" + QString::fromLatin1(item.id().toHex()) +
            " preferredRate=" + QString::number(format.sampleRate()) +
            " channels=" + QString::number(format.channelCount()));
    }
    if (device.isNull()) { log("ENVIRONMENT BLOCKED: no default output"); return 77; }
    int result = 0;
    {
        // The player must be destroyed before the output; the parent process
        // bounds destruction too, since a driver can stall after app.exec().
        auto output = std::make_unique<QAudioOutput>(device);
        auto player = std::make_unique<QMediaPlayer>();
        output->setVolume(0.05f);
        player->setAudioOutput(output.get());
        log("attachedOutput=" + output->device().description());
        int phase = 0;
        QObject::connect(player.get(), &QMediaPlayer::positionChanged, &app, [&](qint64 position) {
            if (phase == 0 && position >= 300) {
                phase = 1;
                player->pause();
                const auto paused = player->position();
                QTimer::singleShot(150, &app, [&, paused] {
                    if (player->playbackState() != QMediaPlayer::PausedState || player->position() != paused) {
                        log("CONTROL pause failed"); app.exit(80); return;
                    }
                    player->setPosition(700);
                    phase = 2;
                    player->play();
                });
            } else if (phase == 2 && position >= 1100) {
                phase = 3;
                player->stop();
                player->setSource({});
                player->setAudioOutput(nullptr);
                log("CONTROL play/pause/seek/resume/stop completed");
                app.quit();
            }
        });
        QObject::connect(player.get(), &QMediaPlayer::errorOccurred, &app, [&](auto, const QString &message) {
            log("CONTROL error=" + message); app.exit(80);
        });
        QTimer::singleShot(10000, &app, [&] { log("CONTROL watchdog expired"); app.exit(81); });
        player->setSource(QUrl::fromLocalFile(app.arguments()[1]));
        player->play();
        result = app.exec();
        if (phase != 3 && result == 0) result = 80;
        log("CONTROL event loop returned=" + QString::number(result));
    }
    if (result == 0) log("CONTROL CLEAN EXIT");
    return result;
}
