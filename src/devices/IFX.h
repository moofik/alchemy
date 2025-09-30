#pragma once
#include "core/audio/INode.h"
#include <string>
#include <string_view>
#include <vector>
#include <span>

enum class FxParamType { Float, Int, Bool, Enum };

struct FxParamDesc {
    std::string id;        // "cutoff", "drive", "mix"...
    std::string name;      // для UI
    FxParamType type;
    float min = 0.f, max = 1.f, def = 0.f, step = 0.f;
};

class IFx {
public:
    virtual ~IFx() = default;

    // Жизненный цикл (не RT-критично)
    virtual void prepare(int sampleRate, int blockSize) = 0;
    virtual void reset() = 0;

    // Главный RT-метод (никаких аллокаций/локов/исключений)
    virtual void process(float* L, float* R, int nframes) = 0;

    // Параметры (описания для UI и доступ по id)
    virtual std::span<const FxParamDesc> params() const = 0;
    virtual bool setParam(std::string_view id, float value) = 0;   // кламп/квант внутри
    virtual float getParam(std::string_view id) const = 0;

    // сервис
    virtual void setBypass(bool on) = 0;
    virtual bool bypass() const = 0;
    virtual int  latencySamples() const { return 0; }
};
