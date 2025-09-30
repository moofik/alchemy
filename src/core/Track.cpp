#include "core/Track.h"
#include <algorithm> // std::min
#include <utility>   // std::move
#include <cassert>

Track::Track(int id)
        : id_(id)
{
}

void Track::prepare(int sampleRate, int blockSize)
{
    sr_ = sampleRate;
    bs_ = blockSize;
    for (auto& fx : chain_) {
        if (fx) fx->prepare(sr_, bs_);
    }
}

void Track::reset()
{
    for (auto& fx : chain_) {
        if (fx) fx->reset();
    }
}

void Track::bindBus(TrackBus bus)
{
    // Привязка на текущий аудиоблок (указатели валидны до конца блока)
    bus_ = bus;
}

void Track::processChain()
{
    // RT-путь: никаких аллокаций/локов/исключений
    if (!bus_.L || !bus_.R || bus_.frames <= 0) return;

    for (auto& fx : chain_) {
        if (fx) fx->process(bus_.L, bus_.R, bus_.frames);
    }
}

// ===== редактирование цепочки (НЕ RT) =====

bool Track::addEffect(std::unique_ptr<IFx> fx)
{
    if (!fx) return false;
    fx->prepare(sr_, bs_);
    chain_.push_back(std::move(fx));
    return true;
}

bool Track::insertEffect(size_t index, std::unique_ptr<IFx> fx)
{
    if (!fx) return false;
    if (index > chain_.size()) return false; // можно вставлять только до/в конец
    fx->prepare(sr_, bs_);
    chain_.insert(chain_.begin() + static_cast<std::ptrdiff_t>(index), std::move(fx));
    return true;
}

bool Track::removeEffect(size_t index)
{
    if (index >= chain_.size()) return false;
    chain_.erase(chain_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool Track::moveEffect(size_t from, size_t to)
{
    if (from >= chain_.size() || to >= chain_.size()) return false;
    if (from == to) return true;
    auto ptr = std::move(chain_[from]);
    chain_.erase(chain_.begin() + static_cast<std::ptrdiff_t>(from));
    chain_.insert(chain_.begin() + static_cast<std::ptrdiff_t>(to), std::move(ptr));
    return true;
}

// ===== параметры FX (НЕ RT) =====

bool Track::setFxParam(size_t fxIndex, std::string_view paramId, float value)
{
    if (fxIndex >= chain_.size() || !chain_[fxIndex]) return false;
    return chain_[fxIndex]->setParam(paramId, value);
}

float Track::getFxParam(size_t fxIndex, std::string_view paramId) const
{
    if (fxIndex >= chain_.size() || !chain_[fxIndex]) return 0.f;
    return chain_[fxIndex]->getParam(paramId);
}

std::span<const FxParamDesc> Track::fxParams(size_t fxIndex) const
{
    if (fxIndex >= chain_.size() || !chain_[fxIndex]) return {};
    return chain_[fxIndex]->params();
}

// ===== introspection =====

size_t Track::chainSize() const { return chain_.size(); }
int    Track::id()        const { return id_;           }
