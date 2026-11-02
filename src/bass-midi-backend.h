#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

namespace relaxer {
// One MIDI stream; the lazily acquired engine/font are shared only while needed.
class BassMidiBackend final {
public:
    BassMidiBackend();
    ~BassMidiBackend();
    bool open(const QString &path, bool decodeOnly = false);
    bool play();
    bool pause();
    bool seek(qint64 milliseconds);
    bool setVolume(int percent);
    bool close();
    qint64 position() const;
    qint64 duration() const;
    QByteArray readPcm(int frames); // Float32 stereo, 48000 Hz; offline verification.
    QString error() const;
    QString errorStage() const;
    quint32 errorCode() const;
private:
    struct State;
    std::unique_ptr<State> m;
};
}
