#pragma once
#include <memory>
#include <string_view>
#include <functional>
#include <unordered_map>
#include "IFX.h"

using FxFactoryFn = std::function<std::unique_ptr<IFx>()>;

struct IFxRegistry {
    virtual ~IFxRegistry() = default;
    virtual bool   registerFx(std::string_view type, FxFactoryFn make) = 0;
    virtual bool   isRegistered(std::string_view type) const = 0;
    virtual std::unique_ptr<IFx> create(std::string_view type) const = 0;
    virtual std::vector<std::string> listTypes() const = 0;
};
