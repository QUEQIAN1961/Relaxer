#pragma once
#include <QJsonObject>
#include <QString>
#include <QTemporaryFile>
#include <functional>
#include <memory>

namespace relaxer {
// One Windows sequencer per Playback. No second player, queue or statistics.
class MidiTransport final {
  public:
    using Sender = std::function<quint32(const QString &, QString *)>;
    using PathResolver = std::function<QString(const QString &)>;
    explicit MidiTransport(QString root, Sender sender = {}, PathResolver resolver = {});
    ~MidiTransport();
    bool open(const QString &path);
    bool play();
    bool pause();
    bool seek(qint64 position);
    bool close();
    qint64 position() const;
    qint64 duration() const { return m_duration; }
    QString error() const { return m_error; }
    QString openedPath() const { return m_path; }
    quint32 errorCode() const { return m_code; }
    bool isOpen() const { return m_open; }
    static void diagnostic(const QString &root, const QJsonObject &event);

  private:
    QString m_root, m_alias, m_original, m_path, m_temporaryPath;
    mutable QString m_error;
    mutable quint32 m_code = 0;
    qint64 m_duration = 0;
    mutable qint64 m_position = 0;
    bool m_open = false;
    Sender m_sender;
    PathResolver m_resolver;
    std::unique_ptr<QTemporaryFile> m_copy;
    bool send(const QString &operation, const QString &command, QString *result = nullptr) const;
    bool fail(const QString &operation, const QString &message, quint32 code = 0) const;
};
} // namespace relaxer
