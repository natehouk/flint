// Random parameters, MIDI and transport at 44.1-192 kHz with loud noise input: output finite, <= 0 dBFS.
#include "../plugins/Flint/ParamDefs.hpp"
#include <cstdio>
#include <random>
using namespace flint;
int main() {
    std::mt19937 g(11); std::uniform_real_distribution<float> u(0, 1); int bad = 0; float pk = 0;
    for (int sr : { 44100, 48000, 96000, 192000 }) {
        Engine e; e.setSampleRate((float)sr);
        for (int it = 0; it < 2500; ++it) {
            for (uint32_t i = 0; i < kParamCount; ++i) {
                const ParamDef& d = paramDef(i); if (d.hints & kHintOutput) continue;
                if (u(g) < 0.5f) continue;                       // change about half the parameters per block
                float v = (u(g) < .3f) ? (u(g) < .5f ? d.min : d.max) : d.min + (d.max - d.min) * u(g);
                if (d.hints & kHintInteger) v = std::round(v);
                e.p[i] = v;
            }
            
            
            float in[400], L[400], R[400]; const float amp = u(g) < .2f ? 0 : (u(g) < .5f ? 4.f : 0.5f);
            for (float& x : in) x = amp * (2 * u(g) - 1);
            e.process(in, in, L, R, 400);
            for (int k = 0; k < 400; ++k) { if (!std::isfinite(L[k]) || !std::isfinite(R[k])) bad++; pk = std::max(pk, std::max(std::fabs(L[k]), std::fabs(R[k]))); }
        }
    }
    printf("fuzz nonfinite=%d peak=%.3f\n", bad, pk); return bad || pk > 1.f;
}
