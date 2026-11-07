#include "fluid-midi-backend.h"
#include <QAudioSink>
#include <QCoreApplication>
#include <QFile>
#include <QLibrary>
#include <QMediaDevices>
#include <QMutex>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace relaxer {
namespace {
constexpr int Rate = 48000;
// FluidSynth's public C ABI; load only when a MIDI is opened.
struct Api {
    QLibrary library;
#define API(member, result, ...) using member##Type = result (*)(__VA_ARGS__); member##Type member = nullptr
    API(newSettings, void *);
    API(deleteSettings, void, void *);
    API(setNum, int, void *, const char *, double);
    API(setInt, int, void *, const char *, int);
    API(setString, int, void *, const char *, const char *);
    API(newSynth, void *, void *);
    API(deleteSynth, void, void *);
    API(loadFont, int, void *, const char *, int);
    API(newPlayer, void *, void *);
    API(deletePlayer, void, void *);
    API(addMemory, int, void *, const void *, size_t);
    API(play, int, void *);
    API(stop, int, void *);
    API(seek, int, void *, int);
    API(write, int, void *, int, void *, int, int, void *, int, int);
    API(gain, void, void *, float);
    API(reset, int, void *);
#undef API
    bool load(QString &error) {
        library.setFileName(QCoreApplication::applicationDirPath() + "/libfluidsynth-3.dll");
        library.setLoadHints(QLibrary::ResolveAllSymbolsHint);
        if (!library.load()) { error = QStringLiteral("无法加载 FluidSynth：") + library.errorString(); return false; }
#define RESOLVE(member, name) member = reinterpret_cast<member##Type>(library.resolve(name)); if (!member) { error = QStringLiteral("FluidSynth 接口不匹配：") + name; return false; }
        RESOLVE(newSettings, "new_fluid_settings"); RESOLVE(deleteSettings, "delete_fluid_settings");
        RESOLVE(setNum, "fluid_settings_setnum"); RESOLVE(setInt, "fluid_settings_setint"); RESOLVE(setString, "fluid_settings_setstr");
        RESOLVE(newSynth, "new_fluid_synth"); RESOLVE(deleteSynth, "delete_fluid_synth"); RESOLVE(loadFont, "fluid_synth_sfload");
        RESOLVE(newPlayer, "new_fluid_player"); RESOLVE(deletePlayer, "delete_fluid_player"); RESOLVE(addMemory, "fluid_player_add_mem");
        RESOLVE(play, "fluid_player_play"); RESOLVE(stop, "fluid_player_stop"); RESOLVE(seek, "fluid_player_seek");
        RESOLVE(write, "fluid_synth_write_float"); RESOLVE(gain, "fluid_synth_set_gain"); RESOLVE(reset, "fluid_synth_system_reset");
#undef RESOLVE
        return true;
    }
    ~Api() { library.unload(); }
};
// Only read SMF timing metadata here. FluidSynth parses and plays all MIDI
// events, including controllers and SysEx. The tempo map converts UI ms to ticks.
struct Tempo { quint64 tick; quint32 micros; double time = 0; };
struct Timing {
    std::vector<Tempo> tempos{{0, 500000}};
    quint16 division = 0;
    quint64 lastTick = 0;
    double totalMicros = 0;
    double microsPerTick = 0;
    bool parse(const QByteArray &bytes) {
        const auto *d = reinterpret_cast<const uchar *>(bytes.constData());
        const qsizetype size = bytes.size();
        auto u16 = [&](qsizetype p) { return quint16((d[p] << 8) | d[p+1]); };
        auto u32 = [&](qsizetype p) { return (quint32(d[p]) << 24) | (quint32(d[p+1]) << 16) | (quint32(d[p+2]) << 8) | d[p+3]; };
        if (size < 14 || bytes.left(4) != "MThd" || u32(4) < 6 || u16(8) > 1) return false;
        division = u16(12); if (!division) return false;
        if (division & 0x8000) {
            const int fps = -qint8(division >> 8), subframes = division & 255;
            if (!subframes || (fps != 24 && fps != 25 && fps != 29 && fps != 30)) return false;
            microsPerTick = 1e6 / ((fps == 29 ? 30000. / 1001 : fps) * subframes);
        }
        qsizetype p = 8 + qint64(u32(4)); int tracks = 0;
        while (p + 8 <= size && tracks < u16(10)) {
            const quint32 length = u32(p+4); if (length > quint64(size-p-8)) return false;
            const qsizetype end = p + 8 + length; const bool track = bytes.mid(p,4) == "MTrk"; p += 8;
            if (!track) { p = end; continue; }
            ++tracks; quint64 tick = 0; int running = 0;
            auto vlq = [&](quint32 &value) {
                value = 0;
                for (int i=0; i<4 && p<end; ++i) { const int b=d[p++]; value=(value<<7)|(b&127); if (!(b&128)) return true; }
                return false;
            };
            while (p < end) {
                quint32 delta; if (!vlq(delta) || p >= end) return false; tick += delta;
                int status = d[p] & 128 ? d[p++] : running; if (status < 0x80) return false;
                if (status == 0xff || status == 0xf0 || status == 0xf7) {
                    int meta = -1; if (status == 0xff) { if (p >= end) return false; meta=d[p++]; }
                    quint32 n; if (!vlq(n) || n > quint64(end-p)) return false;
                    if (meta == 0x51 && n == 3) { const quint32 tempo=(quint32(d[p])<<16)|(d[p+1]<<8)|d[p+2]; if (!tempo) return false; tempos.push_back({tick,tempo}); }
                    p += n; if (meta == 0x2f) break;
                } else {
                    if (status >= 0xf0) return false;
                    running = status; p += (status & 0xe0) == 0xc0 ? 1 : 2; if (p > end) return false;
                }
            }
            lastTick = std::max(lastTick,tick); p = end;
        }
        if (tracks != u16(10) || !lastTick || lastTick > INT_MAX) return false;
        std::stable_sort(tempos.begin(),tempos.end(),[](const Tempo &a,const Tempo &b){return a.tick<b.tick;});
        for (size_t i=1; i<tempos.size(); ++i) tempos[i].time=tempos[i-1].time+(tempos[i].tick-tempos[i-1].tick)*double(tempos[i-1].micros)/division;
        const auto &last=tempos.back(); totalMicros=microsPerTick ? lastTick*microsPerTick : last.time+(lastTick-last.tick)*double(last.micros)/division;
        return totalMicros > 0;
    }
    int tickAt(qint64 ms) const {
        const double us = ms*1000.;
        if (microsPerTick) return int(us/microsPerTick);
        const Tempo *tempo=&tempos.front();
        for (const auto &t:tempos) { if (t.time>us) break; tempo=&t; }
        return int(tempo->tick+(us-tempo->time)*division/tempo->micros);
    }
};
class PcmSource final : public QIODevice {
public:
    std::function<QByteArray(int)> render;
    PcmSource() { open(QIODevice::ReadOnly); }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 16384 + QIODevice::bytesAvailable(); }
    qint64 readData(char *out,qint64 bytes) override {
        const auto pcm=render(int(bytes/4)); if (pcm.isEmpty()) return 0;
        auto *target=reinterpret_cast<qint16 *>(out); const auto *source=reinterpret_cast<const float *>(pcm.constData());
        for (qsizetype i=0;i<pcm.size()/4;++i) target[i]=qint16(std::clamp(source[i],-1.f,1.f)*32767);
        return pcm.size()/2;
    }
    qint64 writeData(const char *,qint64) override { return -1; }
};
}
struct FluidMidiBackend::State {
    Api api;
    void *settings=nullptr, *synth=nullptr, *player=nullptr;
    QByteArray midi, pendingPcm;
    Timing timing;
    std::unique_ptr<PcmSource> source;
    std::unique_ptr<QAudioSink> sink;
    mutable QMutex mutex;
    qint64 frames=0, totalFrames=0, transportMs=0, sinkStartUs=0;
    int volume=65;
    bool playing=false, decodeOnly=false;
    QString error, stage;
    bool fail(const QString &where,const QString &message) {stage=where;error=message;return false;}
    QByteArray render(int count) {
        QMutexLocker lock(&mutex);
        count=int(std::min<qint64>(count,totalFrames-frames)); if (count<=0) return {};
        QByteArray pcm(count*8,Qt::Uninitialized);
        const auto cached=std::min(pcm.size(),pendingPcm.size());
        if(cached){memcpy(pcm.data(),pendingPcm.constData(),cached);pendingPcm.remove(0,cached);}
        const int rest=count-int(cached/8);
        if (rest && api.write(synth,rest,pcm.data()+cached,0,2,pcm.data()+cached,1,2)!=0) {fail("render",QStringLiteral("FluidSynth 音频渲染失败"));return {};}
        frames+=count; return pcm;
    }
    ~State() {
        if (sink) sink->stop(); sink.reset(); source.reset();
        if (player) api.deletePlayer(player);
        if (synth) api.deleteSynth(synth);
        if (settings) api.deleteSettings(settings);
    }
};
FluidMidiBackend::FluidMidiBackend():m(std::make_unique<State>()) {}
FluidMidiBackend::~FluidMidiBackend()=default;
bool FluidMidiBackend::open(const QString &path,bool decodeOnly) {
    close(); m->decodeOnly=decodeOnly;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return m->fail("open",file.errorString());
    m->midi=file.readAll();
    if (!m->timing.parse(m->midi)) return m->fail("header",QStringLiteral("MIDI 格式或时间信息无效（支持 SMF 0/1）"));
    m->totalFrames=qRound64(m->timing.totalMicros*Rate/1e6);
    if (!m->api.load(m->error)) {m->stage="initialize";return false;}
    m->settings=m->api.newSettings();
    if (!m->settings) return m->fail("initialize",QStringLiteral("无法创建 FluidSynth 设置"));
    m->api.setNum(m->settings,"synth.sample-rate",Rate);
    m->api.setNum(m->settings,"synth.gain",.5*m->volume/100.);
    m->api.setInt(m->settings,"synth.polyphony",256);
    m->api.setString(m->settings,"player.timing-source","sample");
    m->synth=m->api.newSynth(m->settings);
    if (!m->synth) return m->fail("initialize",QStringLiteral("无法创建 FluidSynth 合成器"));
    const auto font=(QCoreApplication::applicationDirPath()+"/midi/GeneralUser-GS.sf2").toUtf8();
    if (m->api.loadFont(m->synth,font.constData(),1)<0) return m->fail("soundfont",QStringLiteral("无法加载 GeneralUser GS 音色库"));
    m->player=m->api.newPlayer(m->synth);
    if (!m->player || m->api.addMemory(m->player,m->midi.constData(),size_t(m->midi.size()))!=0 || m->api.play(m->player)!=0)
        return m->fail("open",QStringLiteral("FluidSynth 无法读取此 MIDI 文件"));
    // The file player loads tracks at its first sample-clock callback. Prime
    // that callback so seeking works before Play, retaining every audio sample.
    m->pendingPcm.resize(64*8);
    if(m->api.write(m->synth,64,m->pendingPcm.data(),0,2,m->pendingPcm.data(),1,2)!=0)
        return m->fail("open",QStringLiteral("无法初始化 MIDI 采样时钟"));
    if (!decodeOnly) {
        const auto device=QMediaDevices::defaultAudioOutput();
        QAudioFormat format;format.setSampleRate(Rate);format.setChannelCount(2);format.setSampleFormat(QAudioFormat::Int16);
        if (device.isNull() || !device.isFormatSupported(format)) return m->fail("initialize",QStringLiteral("MIDI 输出设备不支持 48 kHz 立体声"));
        m->source=std::make_unique<PcmSource>();m->source->render=[state=m.get()](int count){return state->render(count);};
        m->sink=std::make_unique<QAudioSink>(device,format);m->sink->setBufferSize(Rate*4/10);
    }
    return true;
}
bool FluidMidiBackend::play() {
    if (!m->player) return m->fail("play",QStringLiteral("MIDI 尚未打开"));
    if (m->playing) return true;
    if (m->sink) {if (m->sink->state()==QAudio::SuspendedState)m->sink->resume();else m->sink->start(m->source.get());m->sinkStartUs=m->sink->processedUSecs();}
    m->playing=true;return true;
}
bool FluidMidiBackend::pause() {
    if (!m->player) return m->fail("pause",QStringLiteral("MIDI 尚未打开"));
    const auto at=position(); if (m->sink)m->sink->suspend();m->transportMs=at;m->playing=false;return true;
}
bool FluidMidiBackend::seek(qint64 milliseconds) {
    if (!m->player) return m->fail("seek",QStringLiteral("MIDI 尚未打开"));
    const bool resume=m->playing; if (m->sink)m->sink->reset();
    QMutexLocker lock(&m->mutex);
    milliseconds=std::clamp<qint64>(milliseconds,0,duration());
    m->api.reset(m->synth);
    if (m->api.seek(m->player,m->timing.tickAt(milliseconds))!=0) return m->fail("seek",QStringLiteral("无法跳转 MIDI 进度"));
    m->pendingPcm.clear();
    m->frames=qRound64(milliseconds*Rate/1000.);m->transportMs=milliseconds;m->playing=false;
    lock.unlock();if (resume)return play();return true;
}
bool FluidMidiBackend::setVolume(int percent) {
    QMutexLocker lock(&m->mutex);m->volume=std::clamp(percent,0,100);
    if (m->synth)m->api.gain(m->synth,.5f*m->volume/100);return true;
}
bool FluidMidiBackend::close() {const int volume=m->volume;m=std::make_unique<State>();m->volume=volume;return true;}
qint64 FluidMidiBackend::position() const {
    if (!m->player)return 0;
    if (m->decodeOnly) {QMutexLocker lock(&m->mutex);return qRound64(m->frames*1000./Rate);}
    return std::clamp<qint64>(m->transportMs+(m->playing?(m->sink->processedUSecs()-m->sinkStartUs)/1000:0),0,duration());
}
qint64 FluidMidiBackend::duration() const {return qRound64(m->totalFrames*1000./Rate);}
QByteArray FluidMidiBackend::readPcm(int frames) {return m->player&&m->decodeOnly&&frames>0&&frames<=Rate?m->render(frames):QByteArray();}
QString FluidMidiBackend::error() const {return m->error;}
QString FluidMidiBackend::errorStage() const {return m->stage;}
}
