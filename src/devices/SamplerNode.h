#pragma once
#include "devices/ISampler.h"
#include <array>
#include <iostream>
#include <params/Param.h>

static constexpr int kPads = 16;
static constexpr int kBanks = 4;


// Лёгкая обёртка над аудио-буфером семпла (non-interleaved float32)
struct SampleBuffer {
    int sr = 48000;
    int channels = 1;     // 1 или 2
    int frames = 0;
    // Память владеем через shared_ptr, чтобы безопасно свапать между потоками
    std::shared_ptr<float[]> dataL; // канал 0
    std::shared_ptr<float[]> dataR; // канал 1 (может быть nullptr, если mono)
};
using SampleBufferPtr = std::shared_ptr<SampleBuffer>;


// Регион/слайс внутри семпла
enum class LoopMode : uint8_t { None, Forward, PingPong };

struct SampleRegion {
    int start = 0;
    int end = 0;        // не включая end
    int loopStart = 0;
    int loopEnd = 0;    // не включая loopEnd
    LoopMode loop = LoopMode::None;
};

// Параметры пэда
struct PadDesc {
    SampleBufferPtr sample;
    SampleRegion region{0,0,0,0,LoopMode::None};
    int  rootNote = 60;     // для хроматического режима
    float gainLin = 1.f;    // линейный гейн (до панорамирования)
    float pan = 0.f;        // -1..+1 equal-power
    // флаги режимов
    std::atomic<bool> half{false};
    std::atomic<bool> rev{false};
    std::atomic<bool> drag{false};

    // 1) Нужен явный дефолтный конструктор
    PadDesc() = default;

    // 2) Копирующий конструктор: копируем атомики через load()
    PadDesc(const PadDesc& o)
            : sample(o.sample),
              region(o.region),
              rootNote(o.rootNote),
              gainLin(o.gainLin),
              pan(o.pan)
    {
        half.store(o.half.load(std::memory_order_relaxed), std::memory_order_relaxed);
        rev .store(o.rev .load(std::memory_order_relaxed), std::memory_order_relaxed);
        drag.store(o.drag.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    // 3) Копирующее присваивание: тоже через load/store
    PadDesc& operator=(const PadDesc& o) {
        if (this == &o) return *this;
        sample   = o.sample;
        region   = o.region;
        rootNote = o.rootNote;
        gainLin  = o.gainLin;
        pan      = o.pan;
        half.store(o.half.load(std::memory_order_relaxed), std::memory_order_relaxed);
        rev .store(o.rev .load(std::memory_order_relaxed), std::memory_order_relaxed);
        drag.store(o.drag.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }

    // 4) Перемещающий конструктор: атомики "переносятся" как копия через load()
    PadDesc(PadDesc&& o) noexcept
            : sample(std::move(o.sample)),
              region(o.region),
              rootNote(o.rootNote),
              gainLin(o.gainLin),
              pan(o.pan)
    {
        half.store(o.half.load(std::memory_order_relaxed), std::memory_order_relaxed);
        rev .store(o.rev .load(std::memory_order_relaxed), std::memory_order_relaxed);
        drag.store(o.drag.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    // 5) Перемещающее присваивание: аналогично
    PadDesc& operator=(PadDesc&& o) noexcept {
        if (this == &o) return *this;
        sample   = std::move(o.sample);
        region   = o.region;
        rootNote = o.rootNote;
        gainLin  = o.gainLin;
        pan      = o.pan;
        half.store(o.half.load(std::memory_order_relaxed), std::memory_order_relaxed);
        rev .store(o.rev .load(std::memory_order_relaxed), std::memory_order_relaxed);
        drag.store(o.drag.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }
};

inline SampleBufferPtr atomicLoadSample(const PadDesc& pad) {
    return std::atomic_load_explicit(&pad.sample, std::memory_order_acquire);
}

inline void atomicStoreSample(PadDesc& pad, SampleBufferPtr p) {
    std::atomic_store_explicit(&pad.sample, std::move(p), std::memory_order_release);
}


// Голос — это «живущая нота/звучание» конкретного пэда.
// Зачем нужен Voice?
//  - Мы можем воспроизводить один и тот же пэд многократно и одновременно (полифония).
//  - Каждый такой запуск хранит своё состояние: позиция чтения внутри семпла,
//    текущая скорость (rate), огибающая (A/R), панорама, флаги HALF/REV/DRAG,
//    петли и дрейф питча — независимо от других запусков.
//  - Когда приходит NoteOff — конкретный голос уходит в Release,
//    а когда огибающая опускается до нуля — голос освобождается.
//  - Такой объект позволяет не «ломать» уже звучащие удары при переключении банка/параметров.
struct Voice {
    bool active = false;
    int  padId = -1;      // откуда рожден
    int  bankId = 0;      // банк, в момент запуска
    int  midiNote = -1;   // для хроматического режима (иначе -1)
    SampleBufferPtr sample;
    // Позиция чтения в ФРЕЙМАХ (не в сэмплах) + скорость чтения в frames / sample
    double pos = 0.0;
    double rate = 1.0;

    // Огибающая (очень простая A/R)
    enum class EnvPhase { Attack, Sustain, Release, Off };
    EnvPhase envPhase = EnvPhase::Off;
    float env = 0.f;      // текущее значение 0..1
    float envA = 0.005f;  // прирост за сэмпл в Attack (≈5 мс при sr=48k)
    float envR = 0.002f;  // убыль за сэмпл в Release (≈24 мс при sr=48k)

    // Гейн/пан на момент старта (копируем из пэда под сглаживанием)
    float gainLin = 1.f;
    float pan = 0.f;

    // Слайс/луп (копия-слепок с пэда на момент старта)
    int start = 0, end = 0, loopStart = 0, loopEnd = 0;
    LoopMode loop = LoopMode::None;

    // Флаги режима, зафиксированные при старте (чтобы не прыгали в полёте)
    bool fHalf = false, fRev = false, fDrag = false;

    // Служебное
    bool pingpongForward = true; // для PingPong
};

// ===== Simple fixed-size voice pool =====
class PolyAllocator {
public:
    explicit PolyAllocator(int maxVoices);


    Voice* allocSteal();
    void noteOffPad(int padId);
    void noteOffChromatic(int midiNote);


    std::vector<Voice>& all();


private:
    std::vector<Voice> voices_;
};


// ===== SamplerNode (16 pads × banks, pad & chromatic modes) =====
class SamplerNode : public INode {
public:
    struct Config {
        int sr = 48000;
        int maxVoices = 64;
        int banks = kBanks;
    };


    explicit SamplerNode(const char* id);

    // 1) Регистрация своих параметров в сторадже (НЕ RT)
    void registerParams(IParameterStore& ps) override;
    // 2) Закэшить указатели на IParam* (после commitParams) (НЕ RT)
    void bindParams(IParameterStore& ps) override;
    // Данные региона сэмпла
    SampleRegion buildRegionFromParams(const PadDesc& pad, int b, int p) noexcept;

    const char* id() const override { return id_; }
    int  numInputs()  const override { return 0; }   // сэмплер — генератор
    int  numOutputs() const override { return 2; }   // стерео

    void prepare(const ProcessContext &ctx) override;
    void release() override;

    // Banks / pads
    void setCurrentBank(int id);
    int currentBank() const;
    static int trackOf(int bank, int pad);

    void loadSample(int bankId, int padId, const SampleBufferPtr& buf, const SampleRegion& rgn);


// Pad mode
    void noteOnPad(int padId, float velocity01);
    void noteOffPad(int padId);


// Chromatic mode
    void setChromaticPad(int padId);
    void setChromaticEnabled(bool on);
    void noteOnChromatic(int midiNote, float velocity01);
    void noteOffChromatic(int midiNote);


// Audio processing

    void process(AlchemyAudioBuffer& io, MidiBuffer& midi, const ProcessContext& ctx) override;


private:
    std::vector<float> scratchL_;
    std::vector<float> scratchR_;
    static constexpr int kPadsPerBank = kPads;
    const char* id_ = "sampler";
    bool inRangeBankPad(int bankId, int padId) const;
    static bool applyLoopOrStop(Voice& v); // returns true if still playing (looped), false → enter release/off

    Config cfg_;
    std::atomic<int> currentBank_{0};
    std::atomic<int> chromaticPad_{0};
    std::atomic<bool> chromaticOn_{false};


// banks × 16 pads
    std::vector<std::vector<PadDesc>> pads_;

    struct ParamRefs {
        int banks=0, pads=16;
        std::vector<IParam*> atk, rel, gain, pan, half, rev, drag;
        std::vector<IParam*> rStart, rEnd, rLStart, rLEnd, rMode;
        void resize(int b, int p) {
            banks=b; pads=p; const int N=b*p;
            auto init = [&](std::vector<IParam*>& v){ v.assign(N,nullptr); };
            init(atk); init(rel); init(gain); init(pan);
            init(half); init(rev); init(drag);
            init(rStart); init(rEnd); init(rLStart); init(rLEnd); init(rMode);
        }
        int idx(int b,int p) const { return b*pads + p; }
    } refs_;

    inline IParam*& ATK(int b,int p){ return refs_.atk[refs_.idx(b,p)]; }
    inline IParam*& REL(int b,int p){ return refs_.rel[refs_.idx(b,p)]; }
    inline IParam*& GAIN(int b,int p){ return refs_.gain[refs_.idx(b,p)]; }
    inline IParam*& PAN (int b,int p){ return refs_.pan [refs_.idx(b,p)]; }
    inline IParam*& HALF(int b,int p){ return refs_.half[refs_.idx(b,p)]; }
    inline IParam*& REV (int b,int p){ return refs_.rev [refs_.idx(b,p)]; }
    inline IParam*& DRAG(int b,int p){ return refs_.drag[refs_.idx(b,p)]; }
    inline IParam*& RSTART(int b,int p){ return refs_.rStart[refs_.idx(b,p)]; }
    inline IParam*& REND  (int b,int p){ return refs_.rEnd  [refs_.idx(b,p)]; }
    inline IParam*& RLSTART   (int b,int p){ return refs_.rLStart[refs_.idx(b,p)]; }
    inline IParam*& RLEND   (int b,int p){ return refs_.rLEnd  [refs_.idx(b,p)]; }
    inline IParam*& RMODE (int b,int p){ return refs_.rMode [refs_.idx(b,p)]; }

    PolyAllocator voices_;
};


// ====== Утилиты ======
inline void panEqualPower(float inL, float inR, float pan, float& outL, float& outR) {
    // pan -1..+1 → угол 0..π/2
    const float theta = (pan * 0.5f + 0.5f) * (float)M_PI_2;
    const float gL = std::cos(theta);
    const float gR = std::sin(theta);
    outL = inL * gL;
    outR = inR * gR;
}

inline float interpolate(float a, float b, float t){ return a + (b - a) * t; }
