#include "bass-midi-backend.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <cmath>
#include <limits>

namespace relaxer {
namespace {
// Public BASS 2.4 C ABI. Resolve at first MIDI open, not at application startup.
using Handle = quint32;
using Bytes = quint64;
constexpr quint32 Unicode = 0x80000000u, Decode = 0x200000, Float = 0x100, Prescan = 0x20000;
struct SoundFont { Handle font; int preset, bank; };
struct Engine {
    QLibrary bass, midi;
    int device = -1;
    Handle font = 0;
    QString error;
    quint32 code = 0;
    bool initialized = false;
#define API(member, result, ...) using member##Type = result (*)(__VA_ARGS__); member##Type member = nullptr
    API(init, int, int, quint32, quint32, void *, const void *);
    API(freeDevice, int);
    API(getDevice, quint32);
    API(setDevice, int, quint32);
    API(config, int, quint32, quint32);
    API(version, quint32);
    API(midiVersion, quint32);
    API(lastError, int);
    API(create, Handle, quint32, const void *, Bytes, Bytes, quint32, quint32);
    API(freeStream, int, Handle);
    API(fontInit, Handle, const void *, quint32);
    API(fontFree, int, Handle);
    API(setFonts, int, Handle, const SoundFont *, quint32);
    API(preload, int, Handle);
    API(length, Bytes, Handle, quint32);
    API(position, Bytes, Handle, quint32);
    API(seconds, double, Handle, Bytes);
    API(bytes, Bytes, Handle, double);
    API(seek, int, Handle, Bytes, quint32);
    API(play, int, Handle, int);
    API(pause, int, Handle);
    API(attribute, int, Handle, quint32, float);
    API(data, quint32, Handle, void *, quint32);
#undef API
    ~Engine() {
        if (font && fontFree) fontFree(font);
        if (initialized) { setDevice(quint32(device)); freeDevice(); }
        midi.unload(); bass.unload();
    }
    bool load(bool decodeOnly) {
        const auto root = QCoreApplication::applicationDirPath();
        bass.setFileName(root + "/bass.dll"); midi.setFileName(root + "/bassmidi.dll");
        bass.setLoadHints(QLibrary::ResolveAllSymbolsHint); midi.setLoadHints(QLibrary::ResolveAllSymbolsHint);
        if (!bass.load() || !midi.load()) {
            error = QStringLiteral("缺少或无法加载 BASS MIDI 运行库：") + (bass.isLoaded() ? midi.errorString() : bass.errorString());
            return false;
        }
#define RESOLVE(lib, member, name) member = reinterpret_cast<member##Type>(lib.resolve(name)); if (!member) { error = QStringLiteral("MIDI 运行库接口不匹配：") + name; return false; }
        RESOLVE(bass, init, "BASS_Init"); RESOLVE(bass, freeDevice, "BASS_Free");
        RESOLVE(bass, getDevice, "BASS_GetDevice"); RESOLVE(bass, setDevice, "BASS_SetDevice");
        RESOLVE(bass, config, "BASS_SetConfig"); RESOLVE(bass, version, "BASS_GetVersion");
        RESOLVE(bass, lastError, "BASS_ErrorGetCode"); RESOLVE(bass, freeStream, "BASS_StreamFree");
        RESOLVE(bass, length, "BASS_ChannelGetLength"); RESOLVE(bass, position, "BASS_ChannelGetPosition");
        RESOLVE(bass, seconds, "BASS_ChannelBytes2Seconds"); RESOLVE(bass, bytes, "BASS_ChannelSeconds2Bytes");
        RESOLVE(bass, seek, "BASS_ChannelSetPosition"); RESOLVE(bass, play, "BASS_ChannelPlay");
        RESOLVE(bass, pause, "BASS_ChannelPause"); RESOLVE(bass, attribute, "BASS_ChannelSetAttribute");
        RESOLVE(bass, data, "BASS_ChannelGetData");
        RESOLVE(midi, midiVersion, "BASS_MIDI_GetVersion"); RESOLVE(midi, create, "BASS_MIDI_StreamCreateFile");
        RESOLVE(midi, fontInit, "BASS_MIDI_FontInit"); RESOLVE(midi, fontFree, "BASS_MIDI_FontFree");
        RESOLVE(midi, setFonts, "BASS_MIDI_StreamSetFonts"); RESOLVE(midi, preload, "BASS_MIDI_StreamLoadSamples");
#undef RESOLVE
        if ((version() >> 16) != 0x204 || (midiVersion() >> 16) != 0x204) {
            error = QStringLiteral("需要兼容的 BASS / BASSMIDI 2.4 运行库"); return false;
        }
        // A music player needs buffering, not live-instrument latency.
        config(0, 300); config(1, 20);
        if (!init(decodeOnly ? 0 : -1, 48000, 0, nullptr, nullptr)) {
            code = quint32(lastError()); error = QStringLiteral("无法初始化 MIDI 音频输出"); return false;
        }
        initialized = true; device = int(getDevice());
        const auto path = QDir::toNativeSeparators(root + "/midi/GeneralUser-GS.sf2");
        if (!QFileInfo::exists(path)) { error = QStringLiteral("缺少 MIDI 音色库 midi/GeneralUser-GS.sf2"); return false; }
        font = fontInit(path.utf16(), Unicode | 0x20000); // memory-map uncompressed SF2 samples
        if (!font) { code = quint32(lastError()); error = QStringLiteral("无法加载 GeneralUser GS 音色库"); return false; }
        return true;
    }
};
std::shared_ptr<Engine> acquire(bool decodeOnly) {
    static QMutex mutex;
    static std::weak_ptr<Engine> engines[2];
    QMutexLocker lock(&mutex);
    auto engine = engines[decodeOnly ? 0 : 1].lock();
    if (!engine) {
        engine = std::make_shared<Engine>();
        if (engine->load(decodeOnly)) engines[decodeOnly ? 0 : 1] = engine;
    }
    return engine;
}
QString nativePath(const QString &path) {
    auto result = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    if (!result.startsWith("\\\\?\\")) result = result.startsWith("\\\\") ? "\\\\?\\UNC\\" + result.mid(2) : "\\\\?\\" + result;
#endif
    return result;
}
}
struct BassMidiBackend::State {
    std::shared_ptr<Engine> engine;
    Handle stream = 0;
    qint64 duration = 0;
    int volume = 65;
    bool decodeOnly = false;
    bool playing = false;
    qint64 pausedPosition = 0;
    mutable QString error, stage;
    mutable quint32 code = 0;
    bool fail(const QString &where, const QString &message) const {
        stage = where; error = message; code = engine ? quint32(engine->lastError()) : 0; return false;
    }
    void clearError() { error.clear(); stage.clear(); code = 0; }
};
BassMidiBackend::BassMidiBackend() : m(std::make_unique<State>()) {}
BassMidiBackend::~BassMidiBackend() { close(); }
bool BassMidiBackend::open(const QString &path, bool decodeOnly) {
    if (!close()) return false;
    m->clearError(); m->decodeOnly = decodeOnly;
    auto engine = acquire(decodeOnly);
    if (!engine->error.isEmpty()) { m->error = engine->error; m->code = engine->code; m->stage = "initialize"; return false; }
    m->engine = engine; engine->setDevice(quint32(engine->device));
    const auto native = nativePath(path);
    m->stream = engine->create(0, native.utf16(), 0, 0, Unicode | Float | Prescan | (decodeOnly ? Decode : 0), 48000);
    if (!m->stream) m->fail("open", QStringLiteral("BASSMIDI 无法读取此 MIDI 文件"));
    else {
        const SoundFont font{engine->font, -1, 0};
        if (!engine->setFonts(m->stream, &font, 1)) m->fail("soundfont", QStringLiteral("无法设置 MIDI 音色"));
        else if (!engine->preload(m->stream)) m->fail("preload", QStringLiteral("无法预加载 MIDI 所需采样"));
        else {
            const auto bytes = engine->length(m->stream, 0);
            const double duration = bytes == std::numeric_limits<Bytes>::max() ? 0 : engine->seconds(m->stream, bytes);
            if (!std::isfinite(duration) || duration <= 0) m->fail("length", QStringLiteral("MIDI 没有有效时长"));
            else { m->duration = qRound64(duration * 1000); if (setVolume(m->volume)) return true; }
        }
    }
    const auto error = m->error, stage = m->stage; const auto code = m->code;
    close(); m->error = error; m->stage = stage; m->code = code; return false;
}
bool BassMidiBackend::play() {
    if (!m->stream) return m->fail("play", QStringLiteral("MIDI 尚未打开"));
    if (!m->engine->play(m->stream, false)) return m->fail("play", QStringLiteral("无法开始 MIDI 播放"));
    m->playing = true;
    m->clearError(); return true;
}
bool BassMidiBackend::pause() {
    const auto stoppedAt = position();
    if (!m->stream || !m->engine->pause(m->stream)) return m->fail("pause", QStringLiteral("无法暂停 MIDI"));
    // The device can still drain a short queued buffer after Pause returns.
    // Preserve the user's transport position while paused, including after Seek.
    m->pausedPosition = stoppedAt; m->playing = false;
    m->clearError(); return true;
}
bool BassMidiBackend::seek(qint64 milliseconds) {
    if (!m->stream || !m->engine->seek(m->stream, m->engine->bytes(m->stream, qBound<qint64>(0, milliseconds, m->duration) / 1000.), 0))
        return m->fail("seek", QStringLiteral("无法跳转 MIDI 播放进度"));
    m->pausedPosition = qBound<qint64>(0, milliseconds, m->duration);
    m->clearError(); return true;
}
bool BassMidiBackend::setVolume(int percent) {
    m->volume = qBound(0, percent, 100);
    if (m->stream && !m->engine->attribute(m->stream, 2, m->volume / 100.f)) return m->fail("volume", QStringLiteral("无法设置 MIDI 音量"));
    return true;
}
bool BassMidiBackend::close() {
    bool ok = true;
    if (m->stream && !m->engine->freeStream(m->stream)) ok = m->fail("close", QStringLiteral("无法释放 MIDI 通道"));
    m->stream = 0; m->duration = 0; m->playing = false; m->pausedPosition = 0; m->engine.reset();
    if (ok) m->clearError();
    return ok;
}
qint64 BassMidiBackend::position() const {
    if (!m->stream) return 0;
    if (!m->decodeOnly && !m->playing) return m->pausedPosition;
    const auto bytes = m->engine->position(m->stream, 0);
    if (bytes == std::numeric_limits<Bytes>::max()) { m->fail("position", QStringLiteral("无法读取 MIDI 进度")); return 0; }
    return qRound64(m->engine->seconds(m->stream, bytes) * 1000);
}
qint64 BassMidiBackend::duration() const { return m->duration; }
QByteArray BassMidiBackend::readPcm(int frames) {
    if (!m->stream || !m->decodeOnly || frames <= 0 || frames > 48000) return {};
    QByteArray output(frames * 2 * int(sizeof(float)), Qt::Uninitialized);
    const auto read = m->engine->data(m->stream, output.data(), quint32(output.size()));
    if (read == std::numeric_limits<quint32>::max()) return {};
    output.resize(read); return output;
}
QString BassMidiBackend::error() const { return m->error; }
QString BassMidiBackend::errorStage() const { return m->stage; }
quint32 BassMidiBackend::errorCode() const { return m->code; }
}
