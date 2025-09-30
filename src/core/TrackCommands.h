#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <variant>
#include "devices/IFX.h"

struct CmdAddFx {
    int track; std::string type; int index = -1; // -1 = push_back
};

struct CmdRemoveFx {
    int track; int index;
};

struct CmdMoveFx {
    int track; int from; int to;
};

struct CmdSetFxParam {
    int track; int index; std::string paramId; float value;
};

// Можно расширять: CmdSetTrackGain/Pan, CmdBypassFx, CmdReplaceFx, ...

using TrackCommand = std::variant<CmdAddFx, CmdRemoveFx, CmdMoveFx, CmdSetFxParam>;

struct ITrackCommandQueue {
    virtual ~ITrackCommandQueue() = default;
    virtual void push(const TrackCommand& cmd) = 0;    // из UI-треда
    virtual void drainApply() = 0;                     // движок вызывает между блоками
};
