#include "UiFacade.h"
#include <cmath>
#include <utility>

// ----------------- ctor -----------------
UiFacade::UiFacade(IParameterStore& store, IEventBus& bus)
        : store_(store), bus_(bus) {}

// ----------------- helpers: track id -----------------
std::string UiFacade::trackPrefix(int track) {
    if (track < 0) return "master";
    return "track." + std::to_string(track);
}

std::string UiFacade::makeTrackParamId(int track, std::string_view suffix) {
    std::string s;
    s.reserve(32 + suffix.size());
    s += trackPrefix(track);
    s += '.';
    s += suffix;
    return s;
}

// ----------------- param helpers -----------------
IParam* UiFacade::findParam(const std::string& id) const {
    return store_.find(id); // может вернуть nullptr — вызывающая сторона молча игнорирует
}

float UiFacade::clampByMeta(const ParamMeta& m, float v) {
    if (m.type == ParamType::kBool) return v >= 0.5f ? 1.f : 0.f;
    if (v < m.min) v = m.min;
    if (v > m.max) v = m.max;
    if (m.step > 0.f) {
        const float n = std::round((v - m.min) / m.step);
        v = m.min + n * m.step;
        v = std::clamp(v, m.min, m.max);
    }
    return v;
}

float UiFacade::map01ToByMeta(const ParamMeta& m, float k01) {
    k01 = std::clamp(k01, 0.f, 1.f);
    if (m.type == ParamType::kBool) return k01 >= 0.5f ? 1.f : 0.f;
    return m.min + (m.max - m.min) * k01;
}

float UiFacade::mapTo01ByMeta(const ParamMeta& m, float v) {
    if (m.type == ParamType::kBool) return v > 0.5f ? 1.f : 0.f;
    if (m.max == m.min) return 0.f;
    return (v - m.min) / (m.max - m.min);
}

// ----------------- params API -----------------
void UiFacade::setParamRaw(const std::string& id, float value) {
    if (auto* p = findParam(id)) {
        const auto& meta = p->meta();
        p->setFloat(clampByMeta(meta, value));   // сглаживание — внутри реализации IParam::setFloat() или в DSP
        store_.notify(id, p->getFloat());        // уведомить UI/контроллеров (если подписаны)
    }
}

void UiFacade::setParam01(const std::string& id, float k01) {
    if (auto* p = findParam(id)) {
        const auto& meta = p->meta();
        p->setFloat(clampByMeta(meta, map01ToByMeta(meta, k01)));
        store_.notify(id, p->getFloat());
    }
}

void UiFacade::modParamRaw(const std::string& id, float delta) {
    if (auto* p = findParam(id)) {
        const auto& meta = p->meta();
        const float cur  = p->getFloat();
        p->setFloat(clampByMeta(meta, cur + delta));
        store_.notify(id, p->getFloat());
    }
}

void UiFacade::toggleParam(const std::string& id) {
    if (auto* p = findParam(id)) {
        const auto& meta = p->meta();
        if (meta.type == ParamType::kBool) {
            const float cur = p->getFloat();
            p->setFloat(cur > 0.5f ? 0.f : 1.f);
        } else {
            // Для небулевых: простой «двухпозиционный» тоггл вокруг дефолта/макс.
            const float cur = p->getFloat();
            const float mid = meta.min + 0.5f * (meta.max - meta.min);
            p->setFloat(cur >= mid ? meta.def : meta.max);
        }
        store_.notify(id, p->getFloat());
    }
}

// ----------------- transport / pads / notes -----------------
void UiFacade::play()  { bus_.publishFromUI(Event{EvTransport{true}});  }
void UiFacade::stop()  { bus_.publishFromUI(Event{EvTransport{false}}); }

void UiFacade::padPress(int pad, bool on) {
    bus_.publishFromUI(Event{EvPadPressed{pad, on}});
}

void UiFacade::noteOn(int note, float vel01) {
    vel01 = std::clamp(vel01, 0.f, 1.f);
    bus_.publishFromUI(Event{EvNoteOn{note, vel01}});
}

void UiFacade::noteOff(int note) {
    bus_.publishFromUI(Event{EvNoteOff{note}});
}

void UiFacade::loadSampleToPad(int pad, const std::string& path) {
    bus_.publishFromUI(Event{EvLoadSample{pad, path}});
}

// ----------------- sequencer -----------------
void UiFacade::setBpm(float bpm) {
    if (bpm <= 0.f) return;
    bus_.publishFromUI(Event{EvSeqSetBpm{bpm}});
}

void UiFacade::setSwing(float k01) {
    k01 = std::clamp(k01, 0.f, 1.f);
    bus_.publishFromUI(Event{EvSeqSetSwing{k01}});
}

void UiFacade::setPattern(int index) {
    bus_.publishFromUI(Event{EvSeqSetPattern{index}});
}

// ----------------- macros per-track -----------------
void UiFacade::setMacroOnTrack(int track, MacroId m, float k01) {
    k01 = std::clamp(k01, 0.f, 1.f);
    switch (m) {
        case MacroId::Ritual: applyRitualOnTrack(track, k01); break;
        case MacroId::Drift:  applyDriftOnTrack (track, k01); break;
        case MacroId::Fog:    applyFogOnTrack   (track, k01); break;
        case MacroId::Bleed:  applyBleedOnTrack (track, k01); break;
        case MacroId::Doom:   applyDoomOnTrack  (track, k01); break;
    }
    // Обновим "ручку" макроса для UI/сохранения состояния
    const std::string knobId =
            (m == MacroId::Ritual) ? makeTrackParamId(track, SID_MAC_RIT) :
            (m == MacroId::Drift)  ? makeTrackParamId(track, SID_MAC_DRF) :
            (m == MacroId::Fog)    ? makeTrackParamId(track, SID_MAC_FOG) :
            (m == MacroId::Bleed)  ? makeTrackParamId(track, SID_MAC_BLD) :
            makeTrackParamId(track, SID_MAC_DOM);
    setParamRaw(knobId, k01);
}

// --- helpers для чтения дефолтов (если параметр не зарегистрирован — fallback) ---
static float defOf(UiFacade& ui, const std::string& id, float fallback) {
    if (auto* p = ui.findParam(id)) return p->meta().def;
    return fallback;
}

// --- реализация раскладок ---
void UiFacade::applyRitualOnTrack(int track, float k) {
    // decay↑, drift↑, reverb send↑, saturation↑
    const std::string idEnvD    = makeTrackParamId(track, SID_ENV_D);
    const std::string idDriftD  = makeTrackParamId(track, SID_DRIFT_D);
    const std::string idRevSend = makeTrackParamId(track, SID_REV_SEND);
    const std::string idSat     = makeTrackParamId(track, SID_SAT);

    const float baseD = defOf(*this, idEnvD, 200.f);

    setParamRaw(idEnvD,    mix(baseD, baseD + 900.f, k));
    setParamRaw(idDriftD,  mix(0.f,   0.7f,          k));
    setParamRaw(idRevSend, mix(0.f,   0.5f,          k));
    setParamRaw(idSat,     mix(0.f,   0.35f,         k));
}

void UiFacade::applyDriftOnTrack(int track, float k) {
    const std::string idDriftD = makeTrackParamId(track, SID_DRIFT_D);
    const std::string idDriftR = makeTrackParamId(track, SID_DRIFT_R);

    setParamRaw(idDriftD, mix(0.f,   1.f,  k));
    setParamRaw(idDriftR, mix(0.1f,  1.f,  k));
}

void UiFacade::applyFogOnTrack(int track, float k) {
    // Проксируем FOG в reverb send (позже можно добавить LP/smear)
    const std::string idRevSend = makeTrackParamId(track, SID_REV_SEND);
    setParamRaw(idRevSend, mix(0.f, 0.8f, k));
}

void UiFacade::applyBleedOnTrack(int track, float k) {
    const std::string idSat = makeTrackParamId(track, SID_SAT);
    setParamRaw(idSat, mix(0.f, 0.8f, k));
}

void UiFacade::applyDoomOnTrack(int track, float k) {
    // Длиннее release (более «тёмный» хвост)
    const std::string idEnvR = makeTrackParamId(track, SID_ENV_R);
    const float baseR = defOf(*this, idEnvR, 250.f);
    setParamRaw(idEnvR, mix(baseR, baseR + 1500.f, k));
}
