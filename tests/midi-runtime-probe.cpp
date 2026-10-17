// Explicit native device probe, never registered as an automatic CTest.
#include "importer.h"
#include "playback-facade.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QTextStream>
#include <QThread>
using namespace relaxer;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 3)
        return 64;
    Library library(args[2]);
    QString error;
    if (!library.open(&error))
        return 65;
    const auto imported = Importer::importOne(library.root(), args[1]);
    if (!imported.error.isEmpty())
        return 66;
    const auto id = library.insert(imported.track);
    if (!id)
        return 67;
    PlaybackFacade playback(&library);
    QStringList errors;
    QObject::connect(&playback, &PlaybackFacade::error, [&](const auto &e) { errors << e; });
    auto wait = [&](int milliseconds) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < milliseconds) {
            app.processEvents();
            QThread::msleep(5);
        }
    };
    playback.play({id}, id);
    wait(700);
    const auto first = playback.position();
    const bool started = playback.playing() && first > 0;
    playback.toggle();
    const auto paused = playback.position();
    wait(180);
    const bool pause = !playback.playing() && qAbs(playback.position() - paused) < 50;
    playback.toggle();
    wait(350);
    const bool resumed = playback.playing() && playback.position() > paused;
    playback.seek(2000);
    wait(150);
    const bool sought = playback.position() >= 2000;
    const auto duration = playback.duration();
    const bool saved = playback.save(), closed = playback.shutdown();
    QJsonObject result{
        {"importedHash", imported.track.hash},
        {"managedFile", imported.track.file},
        {"duration", double(duration)},
        {"firstPosition", double(first)},
        {"startedAndAdvanced", started},
        {"pausedWithoutAdvancing", pause},
        {"resumed", resumed},
        {"seeked", sought},
        {"saved", saved},
        {"shutdown", closed},
        {"playingAfterShutdown", playback.playing()},
        {"errors", QJsonArray::fromStringList(errors)},
        {"audibleOutput",
         "MANUAL-VERIFICATION-REQUIRED; API success and position do not prove physical sound"}};
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Indented);
    return started && pause && resumed && sought && saved && closed && errors.isEmpty() ? 0 : 1;
}
