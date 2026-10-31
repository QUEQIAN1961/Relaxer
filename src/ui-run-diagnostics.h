#pragma once
#include <QElapsedTimer>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QTimer>
#include <functional>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace relaxer {
// Opt-in measurements, never a second initialization path. Both UIs use this observer.
class UiRunDiagnostics final : public QObject {
  public:
    QElapsedTimer clock;
    QJsonObject result;
    std::function<bool()> interactionProbe;
    UiRunDiagnostics() {
        clock.start();
#ifdef Q_OS_WIN
        LARGE_INTEGER now, frequency;
        QueryPerformanceCounter(&now); QueryPerformanceFrequency(&frequency);
        result["mainQpcMs"] = double(now.QuadPart) * 1000. / double(frequency.QuadPart);
#endif
    }
    void mark(const QString &key) { result[key] = double(clock.nsecsElapsed()) / 1000000.; }
    void firstFrame() {
        if (result.contains("firstFrameMs")) return;
        mark("firstFrameMs");
        if (interactionProbe) QTimer::singleShot(0, this, [this] { probeInteraction(); });
    }
    void probeInteraction() {
        const bool ok = interactionProbe();
        result["interactionProbeSucceeded"] = ok;
        if (ok) mark("interactiveMs");
        else if (clock.elapsed() - result["firstFrameMs"].toDouble() < 2000)
            QTimer::singleShot(10, this, [this] { probeInteraction(); });
    }
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Paint) firstFrame();
        return false;
    }
    bool write(const QString &path) {
        QSaveFile file(path);
        const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Indented);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    }
};
}
