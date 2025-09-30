
#pragma once
#include <fstream>
#include <string>
#include "sequencer/Sequencer.h"

struct Project {
    Pattern pattern;
    // TODO: samples/patches/mixer later

    bool save(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        f << "steps " << pattern.steps << "\n";
        f << "swing " << pattern.swing << "\n";
        for (int i=0;i<pattern.steps;i++) {
            const auto& st = pattern.data[i];
            f << "step " << i << " " << (st.active?1:0) << " " << (st.isPad?1:0) << " " << st.padOrNote << " " << st.vel << " " << st.micro << "\n";
        }
        return true;
    }

    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string key;
        int steps=0;
        float swing=0.f;
        Pattern p;
        while (f >> key) {
            if (key=="steps") { f >> steps; p.steps = steps; p.data.resize(steps); }
            else if (key=="swing") { f >> swing; p.swing = swing; }
            else if (key=="step") {
                int idx,a,isp,n; float v, m;
                f >> idx >> a >> isp >> n >> v >> m;
                if (idx>=0 && idx<steps) {
                    p.data[idx].active = (a!=0);
                    p.data[idx].isPad  = (isp!=0);
                    p.data[idx].padOrNote = n;
                    p.data[idx].vel = v;
                    p.data[idx].micro = m;
                }
            }
        }
        pattern = p;
        return true;
    }
};
