#pragma once
#include <string>
#include <functional>
#include <atomic>
enum class ParamType { kFloat, kInt, kBool, kEnum };
struct ParamMeta {
    std::string id;     // "mixer.ch1.gain", "sampler.pad01.pitch"
    std::string name;   // для UI
    ParamType   type;
    float min=0.f, max=1.f, def=0.f, step=0.f;
};

struct IParam {
    virtual ~IParam() = default;
    virtual const ParamMeta& meta() const = 0;
    virtual float getFloat() const = 0;
    virtual void  setFloat(float v) = 0; // thread-safe + smoothing
};

class ParamFloat : public IParam {
public:
    explicit ParamFloat(ParamMeta m) : meta_(std::move(m)), value_(meta_.def) {}
    const ParamMeta& meta() const override { return meta_; }
    float getFloat() const override { return value_.load(std::memory_order_relaxed); }
    void  setFloat(float v) override { value_.store(v, std::memory_order_relaxed); }
private:
    ParamMeta meta_;
    std::atomic<float> value_;
};

struct IParameterStore {
    virtual ~IParameterStore() = default;
    virtual IParam* find(const std::string& id) = 0;
    virtual void add(std::unique_ptr<IParam> p) = 0;
    virtual void commitParams() = 0;
    virtual void commitListeners() = 0;
        // Подписки для UI/контроллеров:
    using Listener = std::function<void(const std::string& id, float value)>;
    virtual void addListener(Listener l) = 0;
    virtual void notify(const std::string& id, float value) = 0; // вызывается при изменениях
    virtual void dumpMap() = 0;
    // Можно вызывать из любого треда (UI/аудио).
// Значение хранится в atomic<float>, чтение/запись lock-free.
// Слушатели должны быть лёгкими и thread-safe.
    bool set(const std::string& id, float value) noexcept;
};

enum class ParamId : uint16_t {
    // Envelope
    EnvAttackMs, EnvDecayMs, EnvSustain, EnvReleaseMs,
    // Sample modes / playback
    SampleHalf, SampleReverse, SampleDrag, SampleRateBase,
    // Macro targets
    MacroRitual, MacroDrift, MacroFog, MacroBleed, MacroDoom,
    // FX placeholders (на будущее)
    ReverbSend, Saturation, DriftDepth, DriftRate
};

enum class ActionId : uint16_t {
    Play, Stop, PlayPause,
    PadNoteOn, PadNoteOff, // расширишь при необходимости
    ToggleHalf, ToggleReverse
};

enum class MacroId : uint8_t {
    Ritual, Drift, Fog, Bleed, Doom
};