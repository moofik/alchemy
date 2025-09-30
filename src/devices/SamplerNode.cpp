#include "SamplerNode.h"
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include "utils/TrackPath.h"
#include "core/audio/INode.h"

PolyAllocator::PolyAllocator(int maxVoices)
        : voices_(maxVoices) {}

Voice* PolyAllocator::allocSteal() {
    for (auto& v : voices_) {
        if (!v.active || v.envPhase == Voice::EnvPhase::Off) {
            v = Voice{}; // сброс
            v.active = true;
            v.envPhase = Voice::EnvPhase::Attack;
            return &v;
        }
    }
    auto it = std::min_element(voices_.begin(), voices_.end(),
                               [](const Voice& a, const Voice& b){ return a.env < b.env; });
    *it = Voice{};
    it->active = true;
    it->envPhase = Voice::EnvPhase::Attack;
    return &(*it);
}

// Отпускаем все голоса, рожденные этим padId
void PolyAllocator::noteOffPad(int padId) {
    for (auto& v : voices_) {
        if (v.active && v.padId == padId && v.envPhase != Voice::EnvPhase::Release) {
            v.envPhase = Voice::EnvPhase::Release;
        }
    }
}

// Отпускаем все голоса, рожденные этим MIDI-нотом
void PolyAllocator::noteOffChromatic(int midiNote) {
    for (auto& v : voices_) {
        if (v.active && v.midiNote == midiNote && v.envPhase != Voice::EnvPhase::Release) {
            v.envPhase = Voice::EnvPhase::Release;
        }
    }
}

// Доступ ко всем голосам
std::vector<Voice>& PolyAllocator::all() {
    return voices_;
}
//

// SamplerNode.cpp (фрагмент)
static void addFloat(IParameterStore& ps, std::string id,
                     float mn, float mx, float def, float step=0.f) {
    ParamMeta m{std::move(id), /*name*/"", ParamType::kFloat, mn, mx, def, step};
    ps.add(std::make_unique<ParamFloat>(m));
}

void SamplerNode::registerParams(IParameterStore& ps) {
    const int pads = kPadsPerBank;
    for (int b=0; b<cfg_.banks; ++b) {
        for (int p=0; p<pads; ++p) {
            const int tr = trackOf(b,p); // твой маппинг bank/pad → trackId
            const std::string base = "track."+std::to_string(tr)+".";
            addFloat(ps, TrackPath::trackParam(tr, "gain"), 0.f, 2.f, 1.f);
            addFloat(ps, TrackPath::trackParam(tr, "pan"),  -1.f, 1.f, 0.f);
            addFloat(ps, TrackPath::trackParam(tr, "env.attackMs"), 0.f, 2000.f, 5.f, 0.1f);
            addFloat(ps, TrackPath::trackParam(tr, "env.releaseMs"),0.f, 5000.f, 50.f, 0.1f);
            addFloat(ps, TrackPath::trackParam(tr, "mode.half"), 0.f, 1.f, 0.f, 1.f);
            addFloat(ps, TrackPath::trackParam(tr, "mode.reverse"), 0.f, 1.f, 0.f, 1.f);
            addFloat(ps, TrackPath::trackParam(tr, "mode.drag"), 0.f, 1.f, 0.f, 1.f);
            // region в кадрах
            addFloat(ps, TrackPath::trackParam(tr, "region.start"), 0.f, 1e9f, 0.f, 1.f);
            addFloat(ps, TrackPath::trackParam(tr, "region.end"),   0.f, 1e9f, 0.f, 1.f);
            addFloat(ps, TrackPath::trackParam(tr, "region.loopStart"), 0.f, 1e9f, 0.f, 1.f);
            addFloat(ps, TrackPath::trackParam(tr, "region.loopEnd"),   0.f, 1e9f, 0.f, 1.f);
            addFloat(ps, TrackPath::trackParam(tr, "region.loopMode"),  0.f, 2.f, 0.f, 1.f); // enum: 0/1/2
        }
    }
}

void SamplerNode::bindParams(IParameterStore& ps) {
    std::cout << "[bindParams] banks=" << cfg_.banks
              << " pads=" << kPadsPerBank << "\n";

    refs_.resize(cfg_.banks, kPadsPerBank);

    std::cout << "[bindParams] N=" << (cfg_.banks * kPadsPerBank)
              << " atk.size=" << refs_.atk.size() << "\n";

    const int pads = kPadsPerBank;
    for (int b=0; b<cfg_.banks; ++b) {
        for (int p=0; p<pads; ++p) {
            const int tr = trackOf(b,p);
            const std::string base = "track."+std::to_string(tr)+".";
            ATK(b,p) = ps.find(TrackPath::trackParam(tr, "env.attackMs"));
            REL(b,p) = ps.find(TrackPath::trackParam(tr, "env.releaseMs"));
            GAIN(b,p) = ps.find(TrackPath::trackParam(tr, "gain"));
            PAN(b,p) = ps.find(TrackPath::trackParam(tr, "pan"));
            HALF(b,p) = ps.find(TrackPath::trackParam(tr, "mode.half"));
            REV(b,p) = ps.find(TrackPath::trackParam(tr, "mode.reverse"));
            DRAG(b,p) = ps.find(TrackPath::trackParam(tr, "mode.drag"));
            RSTART(b,p) = ps.find(TrackPath::trackParam(tr, "region.start"));
            REND(b,p) = ps.find(TrackPath::trackParam(tr, "region.end"));
            RLSTART(b,p) = ps.find(TrackPath::trackParam(tr, "region.loopStart"));
            RLEND(b,p) = ps.find(TrackPath::trackParam(tr, "region.loopEnd"));
            RMODE(b,p) = ps.find(TrackPath::trackParam(tr, "region.loopMode"));
        }
    }
    std::cout << "[bindParams] ATK(0,0)=" << (void*)ATK(0,0) << "\n";

}

SampleRegion SamplerNode::buildRegionFromParams(const PadDesc& pad, int b, int p) noexcept
{
    SampleRegion r = pad.region; // начнём с текущего (чтобы были адекватные дефолты)
    const int total = (pad.sample ? pad.sample->frames : 0);
    if (total <= 1) return r;

    int start    = (int) RSTART(b, p)->getFloat();
    int end      = (int) REND(b, p)->getFloat();
    int loopSt   = (int) RLSTART(b, p)->getFloat();
    int loopEn   = (int) RLEND(b, p)->getFloat();
    int modeI    = (int) std::lround(RMODE(b,p)->getFloat());

    // упорядочим и почистим
    if (end <= start) end = std::clamp(start+1, 1, total);
    loopSt = std::clamp(loopSt, start, end-1);
    loopEn = std::clamp(loopEn, loopSt+1, end);
    LoopMode mode = LoopMode::None;
    if      (modeI == 1) mode = LoopMode::Forward;
    else if (modeI == 2) mode = LoopMode::PingPong;

    r.start = start; r.end = end;
    r.loopStart = loopSt; r.loopEnd = loopEn;
    r.loop = mode;
    return r;
}

SamplerNode::SamplerNode(const char* id)
: pads_(4, std::vector<PadDesc>(kPadsPerBank)), voices_(64) {
    id_ = id;
}

void SamplerNode::prepare(const ProcessContext &ctx) {
    const int n = ctx.blockSize;
    scratchL_.clear(); scratchR_.clear();
    scratchL_.resize(n, 0.f);
    scratchR_.resize(n, 0.f);
}

void SamplerNode::release() {}

void SamplerNode::setCurrentBank(int id) {
    currentBank_.store(std::clamp(id, 0, (int)pads_.size()-1), std::memory_order_relaxed);
}

int SamplerNode::currentBank() const { return currentBank_.load(std::memory_order_relaxed); }

void SamplerNode::loadSample(int bankId, int padId, const SampleBufferPtr& buf, const SampleRegion& rgn) {
    if (!inRangeBankPad(bankId, padId)) return;
    auto& pad = pads_[bankId][padId];
    atomicStoreSample(pad, buf);
    std::cout << "SAMPLE LOADED";
    pad.region = rgn; // region — обычная копия; менять его из UI лучше тоже через «двухфазный» путь
    // Небольшая страховка корректности
    pad.region.start = std::max(0, pad.region.start);
    pad.region.end   = std::max(pad.region.start+1, pad.region.end);
    pad.region.loopStart = std::clamp(pad.region.loopStart, pad.region.start, pad.region.end-1);
    pad.region.loopEnd   = std::clamp(pad.region.loopEnd,   pad.region.loopStart+1, pad.region.end);
}


void SamplerNode::noteOnPad(int padId, float velocity01) {
    const int bank = currentBank();

    if (!inRangeBankPad(bank, padId)) return;
    auto& pad = pads_[bank][padId];

    auto sample = atomicLoadSample(pad);
    if (!sample || sample->frames <= 0) return;

    Voice* v = voices_.allocSteal();
    v->active = true;
    v->padId = padId;
    v->bankId = bank;
    v->midiNote = -1; // не хроматический
    v->sample = sample;

    // Копируем слайс/луп и флаги на момент запуска
    v->start = std::min((int) RSTART(bank, padId)->getFloat(), sample->frames-1);
    v->end   = std::max((int) REND(bank, padId)->getFloat(), sample->frames);

    v->loopStart = std::clamp((int) RLSTART(bank, padId)->getFloat(), v->start, v->end-1);
    v->loopEnd   = std::clamp((int) RLEND(bank, padId)->getFloat(), v->loopStart+1, v->end);

    auto loopMode = RMODE(bank, padId)->getFloat();

    if      (loopMode == 1) v->loop = LoopMode::Forward;
    else if (loopMode == 2) v->loop = LoopMode::PingPong;
    else v->loop = LoopMode::None;

    v->fHalf = (bool) HALF(bank, padId)->getFloat();
    v->fRev  = (bool) REV(bank, padId)->getFloat();
    v->fDrag = (bool) DRAG(bank, padId)->getFloat();

    // Позиция старта
    v->pos = v->fRev ? (double)(v->end - 1) : (double)v->start;

    // Скорость чтения: 1 фрейм за сэмпл (pitch = 0st). HALF/REV учтём в process().
    v->rate = 1.0;

    // Гейн/пан + простая velocity-модуляция
    v->gainLin = GAIN(bank, padId)->getFloat() * std::clamp(velocity01, 0.f, 1.f);
    v->pan = std::clamp(PAN(bank, padId)->getFloat(), -1.f, 1.f);

    // Огибающая — старт в Attack
    v->envPhase = Voice::EnvPhase::Attack;
    v->env = 0.f;
    // Нормируем envA/envR по sr (чтобы 5 мс/25 мс примерно сохранялись)
    const float aMs = ATK(bank, padId)->getFloat(), rMs = REL(bank, padId)->getFloat();
    v->envA = std::max(1e-6f, (1.f / std::max(1.f, cfg_.sr * (aMs/1000.f))));
    v->envR = std::max(1e-6f, (1.f / std::max(1.f, cfg_.sr * (rMs/1000.f))));
}

void SamplerNode::noteOffPad(int padId) { voices_.noteOffPad(padId); }

// --- Хроматический режим (упрощённая версия) ---
void SamplerNode::setChromaticPad(int padId) { chromaticPad_.store(std::clamp(padId,0,kPadsPerBank-1)); }
void SamplerNode::setChromaticEnabled(bool on) { chromaticOn_.store(on); }

void SamplerNode::noteOnChromatic(int midiNote, float velocity01) {
    if (!chromaticOn_.load()) return;
    const int bank = currentBank();
    const int padId = chromaticPad_.load();
    if (!inRangeBankPad(bank, padId)) return;

    auto& pad = pads_[bank][padId];
    auto sample = atomicLoadSample(pad);
    if (!sample || sample->frames <= 0) return;

    Voice* v = voices_.allocSteal();
    v->active = true;
    v->padId = padId;
    v->bankId = bank;
    v->midiNote = midiNote;
    v->sample = sample;

    v->start = std::min((int) RSTART(bank, padId)->getFloat(), sample->frames-1);
    v->end   = std::min((int) REND(bank, padId)->getFloat(), sample->frames);
    v->loopStart = std::clamp((int) RLSTART(bank, padId)->getFloat(), v->start, v->end-1);
    v->loopEnd   = std::clamp((int) RLEND(bank, padId)->getFloat(), v->loopStart+1, v->end);

    auto loopMode = RMODE(bank, padId)->getFloat();

    if      (loopMode == 1) v->loop = LoopMode::Forward;
    else if (loopMode == 2) v->loop = LoopMode::PingPong;
    else v->loop = LoopMode::None;

    v->fHalf = (bool) HALF(bank, padId)->getFloat();
    v->fRev  = (bool) REV(bank, padId)->getFloat();
    v->fDrag = (bool) DRAG(bank, padId)->getFloat();

    v->pos  = v->fRev ? (double)(v->end - 1) : (double)v->start;
    v->rate = 1.0;

    v->gainLin = pad.gainLin * std::clamp(velocity01, 0.f, 1.f);
    v->pan = std::clamp(pad.pan, -1.f, 1.f);

    // pitch → rate (±48 st допустим)
    const int semis = std::clamp(midiNote - pad.rootNote, -48, 48);
    v->rate *= std::pow(2.0, semis / 12.0);

    v->envPhase = Voice::EnvPhase::Attack;
    v->env = 0.f;
    const float aMs = 5.f, rMs = 25.f;
    v->envA = std::max(1e-6f, (1.f / std::max(1.f, cfg_.sr * (aMs/1000.f))));
    v->envR = std::max(1e-6f, (1.f / std::max(1.f, cfg_.sr * (rMs/1000.f))));
}

void SamplerNode::noteOffChromatic(int midiNote){ voices_.noteOffChromatic(midiNote); }

// --- Аудио-процессинг ---
void SamplerNode::process(AlchemyAudioBuffer& /*out*/, MidiBuffer& /*midi*/, const ProcessContext& ctx) {
    const int n    = ctx.blockSize;
    const int bank = currentBank_.load(std::memory_order_relaxed);

    // идём по пэдам, чтобы сделать ровно ОДИН addDry на пэд за блок
    for (int pad = 0; pad < kPadsPerBank; ++pad) {
        bool padHasAudio = false;

        // очистить скретч под текущий пэд
        std::fill(scratchL_.begin(), scratchL_.begin() + n, 0.f);
        std::fill(scratchR_.begin(), scratchR_.begin() + n, 0.f);

        // пробегаем все активные голоса ЭТОГО пэда и суммируем в скретч
        for (auto& v : voices_.all()) {
            if (!v.active || !v.sample || v.envPhase == Voice::EnvPhase::Off) continue;
            if (v.padId != pad || v.bankId != bank) continue;

            auto& pd = pads_[bank][pad];

            const SampleRegion newR = buildRegionFromParams(pd, bank, pad);

            const bool changed =
                    (newR.start != pd.region.start) || (newR.end != pd.region.end) ||
                    (newR.loopStart != pd.region.loopStart) || (newR.loopEnd != pd.region.loopEnd) ||
                    (newR.loop != pd.region.loop);

            if (changed) {
                pd.region = newR;
            }

            const auto* smp = v.sample.get();
            const float* sL = smp->dataL.get();
            const float* sR = (smp->channels > 1 && smp->dataR) ? smp->dataR.get() : nullptr;

            const double halfK = v.fHalf ? 0.5 : 1.0;
            const double dragK = v.fDrag ? 0.1 : 1.0;   // MVP
            const double dirK  = v.fRev  ? -1.0 : 1.0;

            for (int i = 0; i < n; ++i) {
                // --- огибающая A/R ---
                switch (v.envPhase) {
                    case Voice::EnvPhase::Attack:
                        v.env += v.envA;
                        if (v.env >= 1.f) { v.env = 1.f; v.envPhase = Voice::EnvPhase::Sustain; }
                        break;
                    case Voice::EnvPhase::Sustain: break;
                    case Voice::EnvPhase::Release:
                        v.env -= v.envR;
                        if (v.env <= 0.f) { v.env = 0.f; v.envPhase = Voice::EnvPhase::Off; }
                        break;
                    case Voice::EnvPhase::Off: break;
                }
                if (v.envPhase == Voice::EnvPhase::Off) break;

                // --- границы/луп ---
                if (v.pos < v.start || v.pos >= v.end - 1) {
                    if (!applyLoopOrStop(v)) {
                        break;
                    }
                }

                // --- линейная интерполяция ---
                const int   p0 = (int)std::floor(v.pos);
                const int   p1 = std::min(p0 + 1, v.end - 1);
                const float t  = (float)(v.pos - p0);

                const float l = interpolate(sL[p0], sL[p1], t);
                const float r = sR ? interpolate(sR[p0], sR[p1], t) : l; // mono→stereo

                // --- гейн*огибающая + панорама ---
                const float gain = v.gainLin * v.env;
                float l2, r2;
                panEqualPower(l * gain, r * gain, v.pan, l2, r2);

                // --- сумма в скретч ---
                scratchL_[i] += l2;
                scratchR_[i] += r2;
                padHasAudio = true;

                // --- шаг позиции ---
                const double step = v.rate * halfK * dragK * dirK;
                v.pos += step;

                if (v.pos < v.start || v.pos >= v.end) {
                    if (!applyLoopOrStop(v)) break;
                }
            }
        }

        // если пэд звучал в этом блоке — отдаём его DRY в соответствующий трек
        if (padHasAudio && ctx.tracks) {

            const int trackId = trackOf(bank, pad);

            ctx.tracks->addDry(trackId, scratchL_.data(), scratchR_.data(), n);
        }
    }
}

// Проверка валидности индексов
bool SamplerNode::inRangeBankPad(int bankId, int padId) const {
    return bankId >= 0 && bankId < (int)pads_.size()
           && padId >= 0 && padId < kPadsPerBank;
}

// Луп/стоп логика
bool SamplerNode::applyLoopOrStop(Voice& v) {
    if (v.loop == LoopMode::None) {
        if (v.envPhase != Voice::EnvPhase::Release)
            v.envPhase = Voice::EnvPhase::Release;
        v.pos = std::clamp(v.pos, (double)v.start, (double)(v.end - 1));
        return false;
    }
    if (v.loop == LoopMode::Forward) {
        if (v.pos >= v.end) v.pos = (double)v.loopStart;
        if (v.pos <  v.start) v.pos = (double)(v.end - 1);
        return true;
    }
    if (v.loop == LoopMode::PingPong) {
        if (v.pos >= v.loopEnd) {
            v.pos = (double)v.loopEnd - 1;
            v.pingpongForward = false;
        } else if (v.pos < v.loopStart) {
            v.pos = (double)v.loopStart;
            v.pingpongForward = true;
        }
        return true;
    }
    return false;
}

int SamplerNode::trackOf(const int bank, int pad) {
    return bank * kPadsPerBank + pad;
}