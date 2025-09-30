#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <algorithm>
#include <cassert>

#include "params/Param.h"   // IParameterStore, IParam, ParamMeta, ParamType
#include "bus/EventBus.h"   // IEventBus и using Event=std::variant<...>

// ------------------------------------------------------------
// UiFacade — единая точка входа для любого фронтенда (ImGui/Qt/консоль/MIDI).
// Разделение обязанностей:
//   • Разовые действия → через EventBus (EvTransport, EvPadPressed, EvNoteOn/Off,
//     EvLoadSample, EvSeqSet*).
//   • Непрерывные параметры → через IParameterStore (атомики), кламп по ParamMeta.
//   • Макросы применяются PER-TRACK (включая master=-1), пишем нужные параметры
//     по строковым id с префиксом трека: "track.N.*" или "master.*".
// ------------------------------------------------------------
class UiFacade {
public:
    explicit UiFacade(IParameterStore& store, IEventBus& bus);

    // ---------- Параметры ----------
    // Физическое значение (мс/дБ/множители) — клампим согласно ParamMeta
    void setParamRaw(const std::string& id, float value);

    // Нормализованное 0..1 → перевод в физический диапазон по ParamMeta
    void setParam01(const std::string& id, float k01);

    // Изменение на delta (в физических единицах)
    void modParamRaw(const std::string& id, float delta);

    // Тоггл для булевых (храним как 0/1 в float) или "переключение" для небулевых
    void toggleParam(const std::string& id);

    // ---------- Макросы по трекам ----------
    // track >= 0 → соответствующий трек; track == -1 → мастер-трек.
    void setMacroOnTrack(int track, MacroId m, float k01);

    IParam*       findParam(const std::string& id) const;

    // ---------- Транспорт / пэды / ноты ----------
    void play();                              // EvTransport{true}
    void stop();                              // EvTransport{false}
    void padPress(int pad, bool on);          // EvPadPressed
    void noteOn(int note, float vel01);       // EvNoteOn (vel в 0..1)
    void noteOff(int note);                   // EvNoteOff
    void loadSampleToPad(int pad, const std::string& path); // EvLoadSample

    // ---------- Секвенсор ----------
    void setBpm(float bpm);                   // EvSeqSetBpm
    void setSwing(float k01);                 // EvSeqSetSwing (0..1)
    void setPattern(int index);               // EvSeqSetPattern

    // ---- Вспомогательные утилиты (могут пригодиться GUI) ----
    // Сформировать полный id: track.N.<suffix> либо master.<suffix>
    static std::string makeTrackParamId(int track, std::string_view suffix);

private:
    IParameterStore& store_;
    IEventBus&       bus_;

    // ---- Доступ к параметрам/нормализация ----
    static float  clampByMeta(const ParamMeta& m, float v);
    static float  map01ToByMeta(const ParamMeta& m, float k01);
    static float  mapTo01ByMeta(const ParamMeta& m, float v);

    // ---- Макросы per-track (реализации) ----
    void applyRitualOnTrack(int track, float k01);
    void applyDriftOnTrack (int track, float k01);
    void applyFogOnTrack   (int track, float k01);
    void applyBleedOnTrack (int track, float k01);
    void applyDoomOnTrack  (int track, float k01);

    // ---- Сервис ----
    static inline float mix(float a, float b, float t) { return a + (b - a) * t; }
    static std::string  trackPrefix(int track);

    // Базовые суффиксы параметров (без префикса трека). Подгони под свои реальные id при регистрации.
    static constexpr std::string_view SID_ENV_A    = "env.attack.ms";
    static constexpr std::string_view SID_ENV_D    = "env.decay.ms";
    static constexpr std::string_view SID_ENV_S    = "env.sustain";
    static constexpr std::string_view SID_ENV_R    = "env.release.ms";

    static constexpr std::string_view SID_REV_SEND = "fx.reverb.send";
    static constexpr std::string_view SID_SAT      = "fx.saturation";
    static constexpr std::string_view SID_DRIFT_D  = "drift.depth";
    static constexpr std::string_view SID_DRIFT_R  = "drift.rate";

    // сами «ручки макросов» (для UI состояния/автосейва), тоже пер-трековые
    static constexpr std::string_view SID_MAC_RIT  = "macro.ritual";
    static constexpr std::string_view SID_MAC_DRF  = "macro.drift";
    static constexpr std::string_view SID_MAC_FOG  = "macro.fog";
    static constexpr std::string_view SID_MAC_BLD  = "macro.bleed";
    static constexpr std::string_view SID_MAC_DOM  = "macro.doom";
};
