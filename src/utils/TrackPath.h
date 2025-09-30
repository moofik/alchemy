#pragma once
#include <string>
#include <string_view>

using TrackId = int;
inline constexpr TrackId kMasterTrack = -1;

namespace TrackPath {

    inline std::string prefix(TrackId track) {
        return (track < 0) ? "master" : "track." + std::to_string(track);
    }

    inline std::string trackParam(TrackId track, std::string_view suffix) {
        std::string s;
        auto p = prefix(track);
        s.reserve(p.size() + 1 + suffix.size());
        s += p; s += '.'; s += suffix;
        return s;
    }

    inline std::string fxParam(TrackId track, int fxIndex, std::string_view paramId) {
        std::string s = prefix(track);
        s += ".fx["; s += std::to_string(fxIndex); s += "].";
        s += paramId;
        return s;
    }

}