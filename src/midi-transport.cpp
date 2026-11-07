#include "midi-transport.h"
#include "fluid-midi-backend.h"
#include <QElapsedTimer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
#include <QDebug>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <mmsystem.h>
// clang-format on
#endif

namespace relaxer {
namespace {
QString shortPath(const QString &path) {
#ifdef Q_OS_WIN
    const auto native = QDir::toNativeSeparators(path).toStdWString();
    const auto needed = GetShortPathNameW(native.c_str(), nullptr, 0);
    if (needed) {
        std::wstring buffer(needed, L'\0');
        const auto count = GetShortPathNameW(native.c_str(), buffer.data(), needed);
        if (count && count < needed)
            return QString::fromWCharArray(buffer.data(), int(count));
    }
#endif
    return QDir::toNativeSeparators(path);
}
} // namespace
MidiTransport::MidiTransport(QString root, Sender sender, PathResolver resolver, bool decodeOnly)
    : m_root(std::move(root)), m_alias("rm" + QUuid::createUuid().toString(QUuid::Id128)),
      m_sender(std::move(sender)), m_decodeOnly(decodeOnly),
      m_resolver(resolver ? std::move(resolver) : shortPath) {
    if (!m_sender) m_fluid = std::make_unique<FluidMidiBackend>();
}
MidiTransport::~MidiTransport() {
    if(!close())qWarning().noquote()<<"MIDI cleanup incomplete:"<<m_error<<"ownedCopy="<<m_temporaryPath;
}
void MidiTransport::diagnostic(const QString &root, const QJsonObject &event) {
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    QDir dir(root + "/logs");
    if (!dir.mkpath("."))
        return;
    QFile file(dir.filePath("midi.jsonl"));
    if (file.size() > 2 * 1024 * 1024) {
        QFile::remove(dir.filePath("midi.previous.jsonl"));
        file.rename(dir.filePath("midi.previous.jsonl"));
        file.setFileName(dir.filePath("midi.jsonl"));
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return;
    auto value = event;
    value["time"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!value.contains("backend")) value["backend"] = "FluidSynth";
    file.write(QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n');
}
void MidiTransport::clearError() const {
    m_error.clear();m_code=0;m_domain=ErrorDomain::None;m_stage.clear();
}
bool MidiTransport::localFailure(const QString &operation,const QString &message,LocalError code) const {
    return fail(operation,message,LocalErrorBase|quint32(code),ErrorDomain::Local);
}
bool MidiTransport::fail(const QString &operation, const QString &message, quint32 code,ErrorDomain domain) const {
    m_code = code;
    m_domain=domain;m_stage=operation;
    m_error =
        QString("MIDI %1 失败（%2）：%3。诊断：data/logs/midi.jsonl").arg(operation).arg(code).arg(message);
    diagnostic(m_root, {{"operation", operation},
                        {"code", double(code)},
                        {"errorDomain",domain==ErrorDomain::Local?"local":domain==ErrorDomain::NativeFluid?"fluidsynth":"native-mci"},
                        {"description", message},
                        {"originalPath", m_original},
                        {"openedPath", m_path},
                        {"exists", QFileInfo::exists(m_original)},
                        {"bytes", double(QFileInfo(m_original).size())}});
    return false;
}
bool MidiTransport::fluidFailure() const {
    const auto code = m_fluid->errorCode();
    return fail(m_fluid->errorStage(), m_fluid->error(), code ? code : LocalErrorBase | quint32(LocalError::Runtime),
                code ? ErrorDomain::NativeFluid : ErrorDomain::Local);
}
bool MidiTransport::send(const QString &operation, const QString &command, QString *result) const {
    const auto code = m_sender(command, result);
    if (code) {
        QString description = "系统 MIDI 后端错误";
#ifdef Q_OS_WIN
        wchar_t text[512] = {};
        if (mciGetErrorStringW(code, text, 512))
            description = QString::fromWCharArray(text);
#endif
        return fail(operation, description, code,ErrorDomain::NativeMci);
    }
    if (operation != "position")
        diagnostic(
            m_root,
            {{"operation", operation}, {"code", 0}, {"originalPath", m_original}, {"openedPath", m_path}});
    return true;
}
bool MidiTransport::open(const QString &path) {
    if (!close())
        return false;
    m_original = QFileInfo(path).absoluteFilePath();
    m_path.clear();
    clearError();
    QFile source(m_original);
    if (!source.open(QIODevice::ReadOnly))
        return localFailure("read", source.errorString(),LocalError::Read);
    // Let the native decoder validate events; reject empty/non-SMF input first.
    const auto header = source.peek(14);
    if (header.size() != 14 || header.left(4) != "MThd")
        return localFailure("header", "文件为空或不是 Standard MIDI File",LocalError::Header);
    if (m_fluid) {
        source.close();
        QElapsedTimer timer; timer.start();
        if (!m_fluid->open(m_original, m_decodeOnly || qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"))) return fluidFailure();
        m_path = m_original; m_duration = m_fluid->duration(); m_open = true; m_position = 0;
        diagnostic(m_root, {{"operation", "open"}, {"originalPath", m_original}, {"openedPath", m_path},
                            {"durationMs", double(m_duration)}, {"prepareMs", double(timer.elapsed())},
                            {"soundfont", "GeneralUser GS 2.0.3"}});
        return true;
    }
    m_path = m_resolver(m_original);
    // The legacy Windows sequencer rejects long element names (275), including
    // valid 64-character imported hashes. A Unicode API alone does not fix this.
    // Prefer 8.3 spelling; when unavailable keep an unchanged, owned short copy.
    if (m_path.size() >= 128) {
        m_copy = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/rmXXXXXX.mid");
        if (!m_copy->open())
            return localFailure("cache", m_copy->errorString(),LocalError::Cache);
        while (!source.atEnd()) {
            const auto bytes = source.read(1024 * 1024);
            if ((bytes.isEmpty() && source.error() != QFile::NoError) || m_copy->write(bytes) != bytes.size())
                return localFailure("cache", "无法创建 MIDI 临时副本",LocalError::Cache);
        }
        if (!m_copy->flush())
            return localFailure("cache", m_copy->errorString(),LocalError::Cache);
        // QTemporaryFile may retain its native handle after close(). Release
        // the creator before MCI opens it with the legacy driver's share mode.
        m_temporaryPath = m_copy->fileName();
        m_copy->setAutoRemove(false);
        m_copy.reset();
        m_path = m_resolver(m_temporaryPath);
        if (m_path.size() >= 128)
            return localFailure("cache", "系统临时目录路径过长，无法提供系统合成器兼容路径",LocalError::Cache);
    }
    source.close();
    int devices = -1;
#ifdef Q_OS_WIN
    devices = int(midiOutGetNumDevs());
#endif
    diagnostic(m_root, {{"operation", "prepare"},
                        {"originalPath", m_original},
                        {"openedPath", m_path},
                        {"exists", QFileInfo::exists(m_path)},
                        {"bytes", double(QFileInfo(m_path).size())},
                        {"outputDevices", devices},
                        {"temporaryCopy", !m_temporaryPath.isEmpty()},
                        {"cacheCreated", QFileInfo::exists(m_path)}});
    if (!send("open", QString("open \"%1\" type sequencer alias %2").arg(m_path, m_alias))) {
        cleanupCopy();
        return false;
    }
    m_open = true;
    QString length;
    if (!send("time-format", "set " + m_alias + " time format milliseconds") ||
        !send("length", "status " + m_alias + " length", &length)) {
        // Cleanup must not hide the operation that made open() fail.
        const auto message=m_error,stage=m_stage;const auto code=m_code;const auto domain=m_domain;
        close();m_error=message;m_stage=stage;m_code=code;m_domain=domain;
        return false;
    }
    bool ok = false;
    m_duration = length.toLongLong(&ok);
    if (!ok || m_duration <= 0) {
        localFailure("length", "MIDI 没有有效播放时长",LocalError::Duration);
        const auto message=m_error,stage=m_stage;const auto code=m_code;const auto domain=m_domain;
        close();m_error=message;m_stage=stage;m_code=code;m_domain=domain;
        return false;
    }
    m_position = 0;
    return true;
}
bool MidiTransport::play() {
    if (m_fluid) { if (!m_fluid->play()) return fluidFailure(); clearError(); return true; }
    if(!m_open||!send("play", "play " + m_alias))return false;
    clearError();return true;
}
bool MidiTransport::pause() {
    if (m_fluid) { if (!m_fluid->pause()) return fluidFailure(); clearError(); return true; }
    if(!m_open||!send("pause", "pause " + m_alias))return false;
    clearError();return true;
}
bool MidiTransport::seek(qint64 position) {
    if (m_fluid) { if (!m_fluid->seek(position)) return fluidFailure(); m_position = position; clearError(); return true; }
    if (!m_open || !send("seek", QString("seek %1 to %2").arg(m_alias).arg(position)))
        return false;
    m_position = position;
    clearError();
    return true;
}
bool MidiTransport::setVolume(int percent) {
    if (m_fluid && !m_fluid->setVolume(percent)) return fluidFailure();
    return true;
}
qint64 MidiTransport::position() const {
    if (m_fluid) {
        const auto position = m_fluid->position();
        if (m_fluid->errorStage() == "position") fluidFailure(); else m_position = position;
        return m_position;
    }
    QString result;
    if (m_open && send("position", "status " + m_alias + " position", &result)) {
        bool ok = false;
        const auto value = result.toLongLong(&ok);
        if (ok)
            m_position = value;
    }
    return m_position;
}
bool MidiTransport::cleanupCopy() {
    m_copy.reset();
    if(m_temporaryPath.isEmpty())return true;
    // Only the unique file created by this instance is owned. Keep its name
    // on failed removal so a later close/reopen/destructor can retry.
    if(QFile::remove(m_temporaryPath)||!QFileInfo::exists(m_temporaryPath)){
        m_temporaryPath.clear();return true;
    }
    diagnostic(m_root,{{"operation","cleanup"},{"ownedCopy",m_temporaryPath},{"deviceStillOpen",m_open},{"removed",false}});
    return false;
}
bool MidiTransport::close() {
    if (m_fluid) {
        const bool ok = m_fluid->close(); m_open = false; m_duration = 0; m_position = 0;
        if (!ok) return fluidFailure();
        clearError(); return true;
    }
    if (m_open) {
        send("stop", "stop " + m_alias);
        if (!send("close", "close " + m_alias)) {
            cleanupCopy();
            return false;
        }
    }
    m_open = false;
    m_duration = 0;
    m_position = 0;
    if(!cleanupCopy())return localFailure("cleanup","MIDI 临时副本仍被占用，将在后续关闭时重试",LocalError::Cleanup);
    clearError();
    return true;
}
} // namespace relaxer
