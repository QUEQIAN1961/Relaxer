#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QDebug>

namespace audio_device_fixture {
inline bool excluded() { return qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS"); }
inline constexpr const char *excludedReason =
    "HARDWARE NOT RUN: deterministic suite excludes device output; run scripts/run-audio-device-tests.py";
enum class Status { Available, EnvironmentBlocked, FixtureError };
struct Result { Status status; QString detail; };
inline Result probe(const QString &file) {
    const auto executable = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
        "relaxer_audio_device_probe.exe"
#else
        "relaxer_audio_device_probe"
#endif
    );
    QProcess process;
    process.setProgram(executable);
    process.setArguments({file});
    process.start();
    if (!process.waitForStarted(5000))
        return {Status::FixtureError, "Audio control executable could not start: " + process.errorString()};
    const bool finished = process.waitForFinished(15000);
    if (!finished) { process.kill(); process.waitForFinished(5000); }
    const auto detail = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
    qInfo().noquote() << "Independent Qt audio control:\n" << detail;
    if (!finished)
        return {Status::EnvironmentBlocked, "ENVIRONMENT/Qt backend BLOCKED: independent control timed out (including teardown)"};
    if (process.exitStatus() != QProcess::NormalExit)
        return {Status::EnvironmentBlocked, "ENVIRONMENT/Qt backend BLOCKED: independent control crashed; see control log"};
    if (process.exitCode() == 0 && detail.contains("CONTROL CLEAN EXIT") &&
        !detail.contains("QAudioFormat not supported") && !detail.contains("Failed to start audio"))
        return {Status::Available, detail};
    if (process.exitCode() == 64)
        return {Status::FixtureError, "Invalid audio control invocation: " + detail};
    return {Status::EnvironmentBlocked, "ENVIRONMENT/Qt backend BLOCKED: independent control did not validate output; see control log"};
}
// Only the separate layout contract uses this scope. It cannot make a physical
// audio case pass, and restores the caller's device policy on every exit path.
class ScopedHeadless {
    QByteArray old = qgetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");
    bool existed = qEnvironmentVariableIsSet("RELAXER_SKIP_AUDIO_DEVICE_TESTS");
  public:
    ScopedHeadless() { qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS", "1"); }
    ~ScopedHeadless() {
        if (existed) qputenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS", old);
        else qunsetenv("RELAXER_SKIP_AUDIO_DEVICE_TESTS");
    }
};
} // namespace audio_device_fixture
