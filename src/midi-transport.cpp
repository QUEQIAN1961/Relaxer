#include "midi-transport.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
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
quint32 nativeSend(const QString &command, QString *result) {
#ifdef Q_OS_WIN
    wchar_t buffer[512] = {};
    const auto code = mciSendStringW(command.toStdWString().c_str(), buffer, 512, nullptr);
    if (result)
        *result = QString::fromWCharArray(buffer);
    return code;
#else
    Q_UNUSED(command);
    Q_UNUSED(result);
    return 1;
#endif
}
} // namespace
MidiTransport::MidiTransport(QString root, Sender sender, PathResolver resolver)
    : m_root(std::move(root)), m_alias("rm" + QUuid::createUuid().toString(QUuid::Id128)),
      m_sender(sender ? std::move(sender) : nativeSend),
      m_resolver(resolver ? std::move(resolver) : shortPath) {}
MidiTransport::~MidiTransport() {
    close();
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
    value["backend"] = "Windows MCI sequencer";
    file.write(QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n');
}
bool MidiTransport::fail(const QString &operation, const QString &message, quint32 code) const {
    m_code = code;
    m_error =
        QString("MIDI %1 失败（%2）：%3。诊断：data/logs/midi.jsonl").arg(operation).arg(code).arg(message);
    diagnostic(m_root, {{"operation", operation},
                        {"code", double(code)},
                        {"description", message},
                        {"originalPath", m_original},
                        {"openedPath", m_path},
                        {"exists", QFileInfo::exists(m_original)},
                        {"bytes", double(QFileInfo(m_original).size())}});
    return false;
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
        return fail(operation, description, code);
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
    m_error.clear();
    m_code = 0;
    QFile source(m_original);
    if (!source.open(QIODevice::ReadOnly))
        return fail("read", source.errorString());
    // Let the native decoder validate events; reject empty/non-SMF input first.
    const auto header = source.peek(14);
    if (header.size() != 14 || header.left(4) != "MThd")
        return fail("header", "文件为空或不是 Standard MIDI File");
    m_path = m_resolver(m_original);
    // The legacy Windows sequencer rejects long element names (275), including
    // valid 64-character imported hashes. A Unicode API alone does not fix this.
    // Prefer 8.3 spelling; when unavailable keep an unchanged, owned short copy.
    if (m_path.size() >= 128) {
        m_copy = std::make_unique<QTemporaryFile>(QDir::tempPath() + "/rmXXXXXX.mid");
        if (!m_copy->open())
            return fail("cache", m_copy->errorString());
        while (!source.atEnd()) {
            const auto bytes = source.read(1024 * 1024);
            if ((bytes.isEmpty() && source.error() != QFile::NoError) || m_copy->write(bytes) != bytes.size())
                return fail("cache", "无法创建 MIDI 临时副本");
        }
        if (!m_copy->flush())
            return fail("cache", m_copy->errorString());
        // QTemporaryFile may retain its native handle after close(). Release
        // the creator before MCI opens it with the legacy driver's share mode.
        m_temporaryPath = m_copy->fileName();
        m_copy->setAutoRemove(false);
        m_copy.reset();
        m_path = m_resolver(m_temporaryPath);
        if (m_path.size() >= 128)
            return fail("cache", "系统临时目录路径过长，无法提供系统合成器兼容路径");
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
        close();
        return false;
    }
    m_open = true;
    QString length;
    if (!send("time-format", "set " + m_alias + " time format milliseconds") ||
        !send("length", "status " + m_alias + " length", &length)) {
        close();
        return false;
    }
    bool ok = false;
    m_duration = length.toLongLong(&ok);
    if (!ok || m_duration <= 0) {
        fail("length", "MIDI 没有有效播放时长");
        close();
        return false;
    }
    m_position = 0;
    return true;
}
bool MidiTransport::play() {
    return m_open && send("play", "play " + m_alias);
}
bool MidiTransport::pause() {
    return m_open && send("pause", "pause " + m_alias);
}
bool MidiTransport::seek(qint64 position) {
    if (!m_open || !send("seek", QString("seek %1 to %2").arg(m_alias).arg(position)))
        return false;
    m_position = position;
    return true;
}
qint64 MidiTransport::position() const {
    QString result;
    if (m_open && send("position", "status " + m_alias + " position", &result)) {
        bool ok = false;
        const auto value = result.toLongLong(&ok);
        if (ok)
            m_position = value;
    }
    return m_position;
}
bool MidiTransport::close() {
    if (m_open) {
        send("stop", "stop " + m_alias);
        if (!send("close", "close " + m_alias))
            return false;
    }
    m_open = false;
    m_duration = 0;
    m_position = 0;
    m_copy.reset();
    if (!m_temporaryPath.isEmpty()) {
        QFile::remove(m_temporaryPath);
        m_temporaryPath.clear();
    }
    return true;
}
} // namespace relaxer
