#include "utils/WavLoader.h"
#define DR_WAV_IMPLEMENTATION
#include "third_party/dr_wav.h"
#include <vector>
#include <cstring>

SampleBufferPtr LoadWavToSampleBuffer(const std::string& path, bool forceStereo = false) {
    drwav wav{};
    if (!drwav_init_file(&wav, path.c_str(), nullptr)) return nullptr;

    const uint32_t inCh  = wav.channels ? wav.channels : 1;
    const uint32_t useCh = (forceStereo ? 2u : (inCh >= 2 ? 2u : 1u));

    std::vector<float> interleaved;
    interleaved.resize((size_t)wav.totalPCMFrameCount * inCh);
    drwav_uint64 got = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, interleaved.data());
    drwav_uninit(&wav);
    if (got == 0) return nullptr;

    auto sb = std::make_shared<SampleBuffer>();
    sb->sr       = (int)wav.sampleRate;
    sb->channels = (int)useCh;
    sb->frames   = (int)got;

    sb->dataL.reset(new float[sb->frames], std::default_delete<float[]>());

    if (useCh == 2) {
        sb->dataR.reset(new float[sb->frames], std::default_delete<float[]>());
    } else {
        sb->dataR = std::shared_ptr<float[]>(); // пустой smart pointer (== nullptr внутри)
    }

    const float* in = interleaved.data();
    float* L = sb->dataL.get();
    float* R = (useCh == 2 ? sb->dataR.get() : nullptr);

    if (inCh == 1) {
        // Mono source
        // Копируем в L всегда; если forceStereo или useCh==2, дублируем в R.
        std::memcpy(L, in, sizeof(float) * (size_t)sb->frames);
        if (R) std::memcpy(R, in, sizeof(float) * (size_t)sb->frames);
    } else {
        // Многоканальный источник: берём первые два канала (L/R)
        for (int i = 0; i < sb->frames; ++i) {
            const size_t base = (size_t)i * inCh;
            L[i] = in[base + 0];
            L[i] = in[base + 0];
            if (R) R[i] = in[base + 1];
        }
    }
    return sb;
}
