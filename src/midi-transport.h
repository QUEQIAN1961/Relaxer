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
    enum class ErrorDomain { None, Local, NativeMci };
    enum class LocalError : quint32 { None=0, Read=1, Header=2, Cache=3, Duration=4, Cleanup=5 };
    // Local encoding is interpreted only in ErrorDomain::Local. The separate
    // domain makes it unambiguous even for driver-specific native DWORDs.
    // Native values pass through unchanged; local values never reach MCI.
    static constexpr quint32 LocalErrorBase=0x80000000u;
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
    ErrorDomain errorDomain() const { return m_domain; }
    quint32 nativeErrorCode() const { return m_domain==ErrorDomain::NativeMci?m_code:0; }
    LocalError localError() const { return m_domain==ErrorDomain::Local?LocalError(m_code&~LocalErrorBase):LocalError::None; }
    QString errorStage() const { return m_stage; }
    QString ownedTemporaryPath() const { return m_temporaryPath; }
    bool isOpen() const { return m_open; }
    static void diagnostic(const QString &root, const QJsonObject &event);

  private:
    QString m_root, m_alias, m_original, m_path, m_temporaryPath;
    mutable QString m_error;
    mutable quint32 m_code = 0;
    mutable ErrorDomain m_domain=ErrorDomain::None;
    mutable QString m_stage;
    qint64 m_duration = 0;
    mutable qint64 m_position = 0;
    bool m_open = false;
    Sender m_sender;
    PathResolver m_resolver;
    std::unique_ptr<QTemporaryFile> m_copy;
    bool send(const QString &operation, const QString &command, QString *result = nullptr) const;
    bool fail(const QString &operation, const QString &message, quint32 code, ErrorDomain domain) const;
    bool localFailure(const QString &operation,const QString &message,LocalError code) const;
    bool cleanupCopy();
    void clearError() const;
};
} // namespace relaxer
