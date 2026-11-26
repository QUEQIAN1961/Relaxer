#include "peq-api.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <limits>
#include <mutex>
#include <new>
#include <type_traits>
namespace relaxer::audio {
namespace {
constexpr double Pi=3.14159265358979323846;
constexpr int MaxChannels=32;
struct AtomicBand {std::atomic<double> frequency{1000},gain{0},q{1};std::atomic<int> type{0},enabled{1};};
struct SharedConfig {
    std::atomic<std::uint64_t> revision{0};std::mutex writer;
    AtomicBand bands[PeqBands];std::atomic<double> preamp{0};std::atomic<int> enabled{0},autoHeadroom{1};
} shared;
std::atomic<int> mediaAvailable{0},lastRate{48000};
double finite(double x,double fallback){return std::isfinite(x)?x:fallback;}
PeqConfig sane(PeqConfig c){
    c.preamp=std::clamp(finite(c.preamp,0),-24.,12.);c.enabled=!!c.enabled;c.autoHeadroom=!!c.autoHeadroom;
    for(auto &b:c.bands){b.frequency=std::clamp(finite(b.frequency,1000),20.,20000.);b.gain=std::clamp(finite(b.gain,0),-18.,18.);b.q=std::clamp(finite(b.q,1),.1,18.);b.type=std::clamp(b.type,0,5);b.enabled=!!b.enabled;}return c;
}
bool snapshot(PeqConfig &c,std::uint64_t &revision){
    // Bounded attempts: audio work never waits for a settings writer.
    for(int attempt=0;attempt<3;++attempt){
        const auto begin=shared.revision.load(std::memory_order_acquire);if(begin&1)continue;
        PeqConfig value;value.preamp=shared.preamp.load();value.enabled=shared.enabled.load();value.autoHeadroom=shared.autoHeadroom.load();
        for(int i=0;i<PeqBands;++i){auto &b=value.bands[i];auto &s=shared.bands[i];b.frequency=s.frequency.load();b.gain=s.gain.load();b.q=s.q.load();b.type=s.type.load();b.enabled=s.enabled.load();}
        if(begin==shared.revision.load(std::memory_order_acquire)){c=value;revision=begin;return true;}
    }return false;
}
struct Coeff {double b0=1,b1=0,b2=0,a1=0,a2=0;};
Coeff coefficients(const PeqBand &b,int rate){
    if(!b.enabled||(b.gain==0&&b.type<3))return {};
    const double w=2*Pi*std::min(b.frequency,rate*.475)/rate,s=std::sin(w),c=std::cos(w),alpha=s/(2*b.q),A=std::pow(10.,b.gain/40.),beta=2*std::sqrt(A)*alpha;
    double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
    // RBJ Audio EQ Cookbook: normalized second-order sections.
    switch(b.type){
    case 0:b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;break;
    case 1:b0=A*((A+1)-(A-1)*c+beta);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-beta);a0=(A+1)+(A-1)*c+beta;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-beta;break;
    case 2:b0=A*((A+1)+(A-1)*c+beta);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-beta);a0=(A+1)-(A-1)*c+beta;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-beta;break;
    case 3:b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
    case 4:b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
    case 5:b0=1;b1=-2*c;b2=1;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
    }
    return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
}
double rawResponse(const PeqConfig &c,double frequency,int rate){
    const double w=2*Pi*std::min(frequency,rate*.499)/rate;const auto z=std::polar(1.,-w);double db=0;
    for(const auto &b:c.bands){const auto k=coefficients(b,rate);const auto h=(k.b0+k.b1*z+k.b2*z*z)/(1.+k.a1*z+k.a2*z*z);db+=20*std::log10(std::max(1e-15,std::abs(h)));}return db;
}
double headroom(const PeqConfig &c,int rate){
    if(!c.enabled||!c.autoHeadroom)return 0;
    double peak=0;const double top=std::min(20000.,rate*.475);
    for(int i=0;i<=384;++i)peak=std::max(peak,rawResponse(c,20*std::pow(top/20,i/384.),rate));
    for(const auto &b:c.bands)peak=std::max(peak,rawResponse(c,b.frequency,rate));
    return std::max(0.,peak+c.preamp);
}
struct Delay{double z1=0,z2=0;};
struct Chain {
    Coeff filters[PeqBands];Delay delay[PeqBands][MaxChannels];double gain=1;bool bypass=true;
    void setup(const PeqConfig &c,int rate){
        bypass=!c.enabled;gain=bypass?1:std::pow(10.,(c.preamp-headroom(c,rate))/20.);
        for(int i=0;i<PeqBands;++i)filters[i]=bypass?Coeff{}:coefficients(c.bands[i],rate);
        for(auto &band:delay)for(auto &d:band)d={};
    }
    double sample(double x,int ch){
        if(bypass)return x;x*=gain;
        for(int i=0;i<PeqBands;++i){const auto &k=filters[i];auto &d=delay[i][ch];const double y=k.b0*x+d.z1;d.z1=k.b1*x-k.a1*y+d.z2;d.z2=k.b2*x-k.a2*y;x=y;
            if(!std::isfinite(x)){d={};x=0;}
            if(std::abs(d.z1)<1e-28)d.z1=0;if(std::abs(d.z2)<1e-28)d.z2=0;
        }return x;
    }
};
struct Processor {
    Chain current,next;PeqConfig config;std::uint64_t revision=~std::uint64_t(0),nextRevision=0;
    int rate=0,channels=0,format=0,fade=0,fadeLength=1;
    void reset(){rate=0;fade=0;revision=~std::uint64_t(0);}
};
template<class T> double readSample(const T *p,int index){
    if constexpr(std::is_same_v<T,float>)return p[index];
    else if constexpr(std::is_same_v<T,std::uint8_t>)return (int(p[index])-128)/128.;
    else return double(p[index])/-double(std::numeric_limits<T>::min());
}
template<class T> void writeSample(T *p,int index,double value){
    if constexpr(std::is_same_v<T,float>)p[index]=float(std::clamp(value,-double(std::numeric_limits<float>::max()),double(std::numeric_limits<float>::max())));
    else if constexpr(std::is_same_v<T,std::uint8_t>)p[index]=std::uint8_t(std::clamp(std::round(value*128+128),0.,255.));
    else p[index]=T(std::clamp(std::round(value*-double(std::numeric_limits<T>::min())),double(std::numeric_limits<T>::min()),double(std::numeric_limits<T>::max())));
}
template<class T> void processSamples(Processor &p,T *data,int frames){
    for(int f=0;f<frames;++f){const double blend=p.fade?1-double(p.fade)/p.fadeLength:0;
        for(int ch=0;ch<p.channels;++ch){const int i=f*p.channels+ch;const double x=finite(readSample(data,i),0);double y=p.current.sample(x,ch);if(p.fade)y+=(p.next.sample(x,ch)-y)*blend;writeSample(data,i,y);}
        if(p.fade&&!--p.fade){p.current=p.next;p.revision=p.nextRevision;}
    }
}
}
extern "C" void relaxer_peq_publish(const PeqConfig *value){
    if(!value)return;const auto c=sane(*value);std::lock_guard lock(shared.writer);shared.revision.fetch_add(1,std::memory_order_acq_rel);
    shared.preamp.store(c.preamp);shared.enabled.store(c.enabled);shared.autoHeadroom.store(c.autoHeadroom);
    for(int i=0;i<PeqBands;++i){const auto &b=c.bands[i];auto &s=shared.bands[i];s.frequency.store(b.frequency);s.gain.store(b.gain);s.q.store(b.q);s.type.store(b.type);s.enabled.store(b.enabled);}
    shared.revision.fetch_add(1,std::memory_order_release);
}
extern "C" void *relaxer_peq_create(){return new(std::nothrow)Processor;}
extern "C" void relaxer_peq_destroy(void *p){delete static_cast<Processor *>(p);}
extern "C" void relaxer_peq_reset(void *p){if(p)static_cast<Processor *>(p)->reset();}
extern "C" void relaxer_peq_process(void *instance,void *samples,int frames,int channels,int rate,int format){
    if(!instance||!samples||frames<=0||channels<1||channels>MaxChannels||rate<8000||rate>384000||format<1||format>4)return;
    auto &p=*static_cast<Processor *>(instance);PeqConfig config=p.config;std::uint64_t revision=p.revision;snapshot(config,revision);
    if(p.rate!=rate||p.channels!=channels||p.format!=format){p.rate=rate;p.channels=channels;p.format=format;p.config=config;p.current.setup(config,rate);p.revision=revision;p.fade=0;p.fadeLength=std::max(1,rate/50);lastRate.store(rate);}
    else if(!p.fade&&revision!=p.revision){p.next.setup(config,rate);p.nextRevision=revision;p.config=config;p.fade=p.fadeLength;}
    // Settled bypass is byte-for-byte transparent, including floating headroom.
    if(!p.fade&&p.current.bypass)return;
    switch(format){case 1:processSamples(p,static_cast<std::uint8_t *>(samples),frames);break;case 2:processSamples(p,static_cast<std::int16_t *>(samples),frames);break;case 3:processSamples(p,static_cast<std::int32_t *>(samples),frames);break;case 4:processSamples(p,static_cast<float *>(samples),frames);break;}
}
extern "C" double relaxer_peq_response(const PeqConfig *c,double hz,int rate,int includeHeadroom){
    if(!c||rate<8000||hz<=0)return 0;const auto config=sane(*c);if(!config.enabled)return 0;return rawResponse(config,hz,rate)+config.preamp-(includeHeadroom?headroom(config,rate):0);
}
extern "C" double relaxer_peq_headroom(const PeqConfig *c,int rate){return c&&rate>=8000?headroom(sane(*c),rate):0;}
extern "C" void relaxer_peq_media_bridge(int available){mediaAvailable.store(available);}
extern "C" int relaxer_peq_media_available(){return mediaAvailable.load();}
extern "C" int relaxer_peq_sample_rate(){return lastRate.load();}
}
