#pragma once
#include <cstdint>
#if defined(_WIN32)
# if defined(RELAXER_DSP_BUILD)
#  define RELAXER_DSP_API __declspec(dllexport)
# else
#  define RELAXER_DSP_API __declspec(dllimport)
# endif
#else
# define RELAXER_DSP_API __attribute__((visibility("default")))
#endif
namespace relaxer::audio {
constexpr int PeqBands=8;
struct PeqBand {double frequency=1000,gain=0,q=1;int type=0,enabled=1;};
struct PeqConfig {PeqBand bands[PeqBands];double preamp=0;int enabled=0,autoHeadroom=1;};
// Sample formats deliberately match QAudioFormat: UInt8, Int16, Int32, Float.
extern "C" {
RELAXER_DSP_API void relaxer_peq_publish(const PeqConfig *config);
RELAXER_DSP_API void *relaxer_peq_create();
RELAXER_DSP_API void relaxer_peq_destroy(void *processor);
RELAXER_DSP_API void relaxer_peq_reset(void *processor);
RELAXER_DSP_API void relaxer_peq_process(void *processor,void *samples,int frames,int channels,int sampleRate,int sampleFormat);
RELAXER_DSP_API double relaxer_peq_response(const PeqConfig *config,double frequency,int sampleRate,int includeHeadroom);
RELAXER_DSP_API double relaxer_peq_headroom(const PeqConfig *config,int sampleRate);
RELAXER_DSP_API void relaxer_peq_media_bridge(int available);
RELAXER_DSP_API int relaxer_peq_media_available();
RELAXER_DSP_API int relaxer_peq_sample_rate();
}
}
