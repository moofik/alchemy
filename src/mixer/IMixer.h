#pragma once
#include "core/audio/INode.h"
struct ChannelDesc { std::string srcNodeId; float gain=0.8f; float pan=0.f; bool mute=false; };
struct IMixer : INode {
    virtual int addChannel(const ChannelDesc&) = 0; // возвращает индекс
    virtual void setGain(int ch, float) = 0;
    virtual void setPan (int ch, float) = 0;
    virtual void setSend(int ch, int busIndex, float amount) = 0; // send A/B (Reverb/Delay)
};