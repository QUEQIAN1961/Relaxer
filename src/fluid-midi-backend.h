#pragma once
#include <QByteArray>
#include <QAudioDevice>
#include <QString>
#include <QJsonObject>
#include <memory>

namespace relaxer {
class FluidMidiBackend final {
public:
    explicit FluidMidiBackend(QString soundFontPath = {}, QJsonObject synthesis = {});
    static QString validateSoundFont(const QString &path);
    ~FluidMidiBackend();
    bool open(const QString &path, bool decodeOnly = false);
    bool play();
    bool pause();
    bool seek(qint64 milliseconds);
    bool setVolume(int percent);
    bool setOutputDevice(const QAudioDevice &device);
    bool close();
    qint64 position() const;
    qint64 duration() const;
    QByteArray readPcm(int frames);
    QString soundFontPath() const;
    QString soundFontWarning() const;
    QString error() const;
    QString errorStage() const;
    quint32 errorCode() const { return 0; }
private:
    struct State;
    std::unique_ptr<State> m;
    QAudioDevice m_outputDevice;
    const QString m_soundFontPath;
    const QJsonObject m_synthesis;
};
}
