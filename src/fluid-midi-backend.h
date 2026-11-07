#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

namespace relaxer {
class FluidMidiBackend final {
public:
    FluidMidiBackend();
    ~FluidMidiBackend();
    bool open(const QString &path, bool decodeOnly = false);
    bool play();
    bool pause();
    bool seek(qint64 milliseconds);
    bool setVolume(int percent);
    bool close();
    qint64 position() const;
    qint64 duration() const;
    QByteArray readPcm(int frames);
    QString error() const;
    QString errorStage() const;
    quint32 errorCode() const { return 0; }
private:
    struct State;
    std::unique_ptr<State> m;
};
}
