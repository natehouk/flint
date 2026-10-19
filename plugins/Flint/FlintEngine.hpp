// Flint — a small saturating filter with an envelope follower.
// Clean-room implementation from public, generic techniques: a topology-preserving-transform
// state-variable filter (Zavalishin) with a soft-limited integrator for bounded self-oscillation,
// tanh input saturation, a peak envelope follower with level-dependent (dB) modulation depth,
// and per-sample one-pole parameter smoothing. No third-party product code, assets or
// disassembly were used. License: MIT (see LICENSE).

#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace flint {

static constexpr float kPi = 3.14159265358979f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }

// ------------------------------------------------------------------ params (append-only)
enum Param : uint32_t {
    kFreq,          // Hz, 20 .. 20000
    kPeak,          // 0 .. 1 (self-oscillates at the top)
    kResponse,      // 0 low pass, 1 high pass
    kEfLevel,       // -1 .. 1: envelope follower depth, either direction
    kEfSpeed,       // 0 fast .. 1 slow
    kInGain,        // dB, drives the filter's saturation
    kOutGain,       // dB
    kMidiOn,
    kOutEf,         // output: current envelope modulation, octaves
    kOutLearn,      // output: MIDI learn echo for the UI
    kParamCount
};

static constexpr float kEfOctaves = 6.f;     // full EF Level moves the cutoff up to 6 octaves
static constexpr float kEfFloorDb = -54.f;   // the follower responds from this level up to 0 dBFS

// Damping of the state-variable filter for a Peak setting. Slightly negative at the top, so the
// filter rings on by itself; the soft-limited integrator keeps that oscillation bounded.
inline float peakToK(float peak) { return 2.f - 2.04f * std::pow(clampf(peak, 0.f, 1.f), 0.6f); }

// Envelope follower time constants for a Speed setting (seconds)
inline float efAttack(float speed)  { return 0.0003f * std::pow(200.f, clampf(speed, 0.f, 1.f)); }   // 0.3 ms .. 60 ms
inline float efRelease(float speed) { return 0.01f * std::pow(60.f, clampf(speed, 0.f, 1.f)); }      // 10 ms .. 600 ms

struct SVF {
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    inline void process(float x, float g, float k, float& lp, float& hp) {
        const float a1 = 1.f / (1.f + g * (g + k));
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + g * a1 * v3;
        const float v2 = ic2 + g * v1;
        ic1 = 2.f * v1 - ic1;
        ic1 = ic1 / std::sqrt(1.f + 0.035f * ic1 * ic1);     // soft limit: musical, bounded resonance
        ic2 = 2.f * v2 - ic2;
        lp = v2; hp = x - k * v1 - v2;
    }
};

class Engine {
public:
    float p[kParamCount];
    float efOct = 0;            // current modulation (octaves), for the UI
    float efNorm = 0;           // follower level 0..1 (after the dB mapping)

    Engine() {
        std::fill(p, p + kParamCount, 0.f);
        p[kFreq] = 1000; p[kPeak] = 0.3f; p[kResponse] = 0; p[kEfLevel] = 0; p[kEfSpeed] = 0.5f;
        p[kInGain] = 0; p[kOutGain] = 0; p[kMidiOn] = 1;
        snap();
    }
    void setSampleRate(float sr) {
        fs = sr;
        smooth = 1.f - std::exp(-1.f / (0.010f * sr));     // ~10 ms parameter interpolation
        reset();
    }
    void reset() { for (auto& f : flt) f.reset(); env = 0; efOct = 0; efNorm = 0; snap(); }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        const float tOct = std::log2(clampf(p[kFreq], 10.f, 22000.f));
        const float tPeak = clampf(p[kPeak], 0.f, 1.f), tResp = p[kResponse] > 0.5f ? 1.f : 0.f;
        const float tLevel = clampf(p[kEfLevel], -1.f, 1.f);
        const float tIn = dbToGain(p[kInGain]), tOut = dbToGain(p[kOutGain]);
        const float sp = clampf(p[kEfSpeed], 0.f, 1.f);
        // the detector is fast; Speed sets how quickly the modulation itself follows it
        const float ca = 1.f - std::exp(-1.f / (efAttack(sp) * fs)), cr = 1.f - std::exp(-1.f / (efRelease(sp) * fs));
        const float da = 1.f - std::exp(-1.f / (0.0002f * fs)), dr = 1.f - std::exp(-1.f / (0.012f * fs));

        for (uint32_t i = 0; i < n; ++i) {
            const float xl = inL[i], xr = inR[i];
            sOct += (tOct - sOct) * smooth; sPeak += (tPeak - sPeak) * smooth; sResp += (tResp - sResp) * smooth;
            sLevel += (tLevel - sLevel) * smooth; sIn += (tIn - sIn) * smooth; sOut += (tOut - sOut) * smooth;

            // envelope follower on the incoming signal (both channels, linked)
            const float a = std::max(std::fabs(xl), std::fabs(xr));
            env += (a > env ? da : dr) * (a - env);
            const float target = clampf((20.f * std::log10(env + 1e-9f) - kEfFloorDb) / -kEfFloorDb, 0.f, 1.f);
            efNorm += (target > efNorm ? ca : cr) * (target - efNorm);

            if (--ctl <= 0) {           // coefficients every 4 samples
                ctl = 4;
                efOct = sLevel * kEfOctaves * efNorm;
                const float fc = clampf(std::exp2(sOct + efOct), 10.f, 0.45f * fs);
                g = std::tan(kPi * fc / fs);
                k = peakToK(sPeak);
            }
            float out[2];
            for (int ch = 0; ch < 2; ++ch) {
                const float u = std::tanh((ch == 0 ? xl : xr) * sIn);     // input gain sets how hard the filter is driven
                float lp, hp; flt[ch].process(u, g, k, lp, hp);
                out[ch] = (lp + (hp - lp) * sResp) * sOut;
            }
            outL[i] = softCeil(out[0]); outR[i] = softCeil(out[1]);
        }
        for (auto& f : flt) { if (!(std::fabs(f.ic1) < 1e4f)) f.ic1 = 0; if (!(std::fabs(f.ic2) < 1e4f)) f.ic2 = 0; }
        if (!(env < 1e6f)) env = 0;
    }

    // magnitude response at hz for drawing; extraOct shifts the cutoff (envelope modulation)
    static float magnitude(const float* v, float hz, float extraOct = 0.f) {
        const float fc = clampf(v[kFreq] * std::exp2(extraOct), 10.f, 30000.f);
        const float k = std::max(peakToK(v[kPeak]), 0.03f), w = hz / fc;
        const float re = 1.f - w * w, im = k * w, den = std::sqrt(re * re + im * im);
        return (v[kResponse] > 0.5f ? w * w : 1.f) / std::max(den, 1e-6f);
    }

private:
    static float softCeil(float x) {
        const float a = std::fabs(x);
        if (!(a < 1e6f)) return 0.f;
        return a > 0.89f ? std::copysign(0.89f + 0.11f * std::tanh((a - 0.89f) / 0.11f), x) : x;
    }
    void snap() {
        sOct = std::log2(clampf(p[kFreq], 10.f, 22000.f)); sPeak = p[kPeak]; sResp = p[kResponse] > 0.5f ? 1.f : 0.f;
        sLevel = p[kEfLevel]; sIn = dbToGain(p[kInGain]); sOut = dbToGain(p[kOutGain]);
        g = std::tan(kPi * clampf(std::exp2(sOct), 10.f, 0.45f * fs) / fs); k = peakToK(sPeak); ctl = 0;
    }

    SVF flt[2];
    float fs = 48000.f, smooth = 0.01f;
    float sOct = 10, sPeak = 0, sResp = 0, sLevel = 0, sIn = 1, sOut = 1;
    float env = 0, g = 0.1f, k = 1.4f;
    int ctl = 0;
};

} // namespace flint
