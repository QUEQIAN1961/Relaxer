#pragma once
#include <QElapsedTimer>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlDatabase>

namespace relaxer {
// Opt-in measurements, never a second initialization path. Both UIs use this observer.
class UiRunDiagnostics final : public QObject {
  public:
    QElapsedTimer clock;
    QJsonObject result;
    UiRunDiagnostics() { clock.start(); }
    void mark(const QString &key) { result[key] = double(clock.nsecsElapsed()) / 1000000.; }
    void firstFrame() {
        if (!result.contains("firstFrameMs")) mark("firstFrameMs");
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
