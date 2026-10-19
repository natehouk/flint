// Parameter metadata shared by the DSP wrapper and the UI. License: MIT.
#pragma once
#include "FlintEngine.hpp"
#include <cstdio>

namespace flint {

static constexpr uint32_t kHintInteger = 1, kHintBoolean = 2, kHintLog = 4, kHintOutput = 8, kHintBipolar = 16;
struct ParamDef { const char* name; const char* sym; const char* unit; float min, max; uint32_t hints; };

inline const ParamDef& paramDef(uint32_t i) {
    static ParamDef t[kParamCount];
    static bool done = false;
    if (!done) {
        done = true;
        auto set = [](uint32_t i, const char* n, const char* s, const char* u, float lo, float hi, uint32_t h) { t[i] = { n, s, u, lo, hi, h }; };
        set(kFreq,     "Frequency",  "freq",     "Hz", 20, 20000, kHintLog);
        set(kPeak,     "Peak",       "peak",     "",   0, 1, 0);
        set(kResponse, "Response",   "response", "",   0, 1, kHintInteger);
        set(kEfLevel,  "EF Level",   "eflevel",  "",  -1, 1, kHintBipolar);
        set(kEfSpeed,  "EF Speed",   "efspeed",  "",   0, 1, 0);
        set(kInGain,   "Input Gain", "ingain",   "dB", -24, 24, kHintBipolar);
        set(kOutGain,  "Output Gain","outgain",  "dB", -24, 24, kHintBipolar);
        set(kMidiOn,   "MIDI",       "midion",   "",   0, 1, kHintBoolean | kHintInteger);
        set(kOutEf,    "EF Modulation", "o_ef",  "oct", -6, 6, kHintOutput);
        set(kOutLearn, "MIDI Learn", "o_learn",  "",   0, 100000, kHintOutput);
    }
    return t[i];
}

inline const char* const* enumLabels(uint32_t p, int& count) {
    static const char* const resp[] = { "Low Pass", "High Pass" };
    if (p == kResponse) { count = 2; return resp; }
    count = 0; return nullptr;
}

// Normalised 0..1 <-> value (log for frequency); used by the UI and MIDI learn.
inline float toNorm(uint32_t p, float v) {
    const ParamDef& d = paramDef(p);
    if (d.hints & kHintLog) return std::log(v / d.min) / std::log(d.max / d.min);
    return (v - d.min) / (d.max - d.min);
}
inline float fromNorm(uint32_t p, float n) {
    const ParamDef& d = paramDef(p);
    n = clampf(n, 0.f, 1.f);
    float v = (d.hints & kHintLog) ? d.min * std::pow(d.max / d.min, n) : d.min + n * (d.max - d.min);
    if (d.hints & kHintInteger) v = std::round(v);
    return v;
}

} // namespace flint
