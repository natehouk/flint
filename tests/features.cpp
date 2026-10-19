// Feature tests for Flint: response vs the analog model, both responses, peak and self-oscillation,
// saturation, envelope follower (direction, depth, speed), click-free switching, gains, sample rates.
#include "../plugins/Flint/ParamDefs.hpp"
#include "../plugins/Flint/Presets.hpp"
#include <cstdio>
#include <vector>
using namespace flint;

static int fails = 0;
static void check(bool ok, const char* fmt, double a = 0, double b = 0, double c = 0) {
    printf("%s  ", ok ? "PASS" : "FAIL"); printf(fmt, a, b, c); printf("\n"); fails += !ok;
}
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return std::sqrt(s / (b - a)); }
static void run(Engine& e, const std::vector<float>& in, std::vector<float>& L, std::vector<float>& R, int block = 256) {
    L.resize(in.size()); R.resize(in.size());
    for (size_t i = 0; i < in.size(); i += block) e.process(in.data() + i, in.data() + i, L.data() + i, R.data() + i, (uint32_t)std::min<size_t>(block, in.size() - i));
}
static std::vector<float> sine(float hz, float amp, int n, float fs) { std::vector<float> v(n); for (int i = 0; i < n; ++i) v[i] = amp * std::sin(2 * kPi * hz * i / fs); return v; }
static double gainDb(Engine e, float hz, float fs) {          // small signal: linear region
    e.setSampleRate(fs); std::vector<float> in = sine(hz, 0.01f, (int)fs, fs), L, R; run(e, in, L, R);
    return 20 * std::log10(rms(L, in.size() / 2, in.size()) / rms(in, in.size() / 2, in.size()));
}

int main() {
    const float fs = 48000;
    // 1. response follows the analog model (low frequencies, where warping is negligible)
    double worst = 0;
    for (int resp = 0; resp < 2; ++resp) for (float pk : { 0.f, 0.4f, 0.8f }) for (float fc : { 200.f, 1000.f, 3000.f }) for (float m : { 0.25f, 0.5f, 1.f, 2.f, 4.f }) {
        const float hz = fc * m;
        if (hz > 3000) continue;
        Engine e; e.p[kFreq] = fc; e.p[kPeak] = pk; e.p[kResponse] = (float)resp;
        worst = std::max(worst, std::fabs(gainDb(e, hz, fs) - 20 * std::log10(Engine::magnitude(e.p, hz))));
    }
    check(worst < 0.6, "response matches the analog model up to 3 kHz (worst %.2f dB)", worst);
    { Engine e; e.p[kFreq] = 1000; e.p[kPeak] = 0;
      const double a = gainDb(e, 4000, fs), b = gainDb(e, 8000, fs);
      check(std::fabs((a - b) - 12) < 1.5, "low pass slope 12 dB/oct (4k->8k: %.1f dB)", a - b); }
    { Engine e; e.p[kFreq] = 1000; e.p[kPeak] = 0; e.p[kResponse] = 1;
      const double a = gainDb(e, 250, fs), b = gainDb(e, 125, fs), pass = gainDb(e, 10000, fs);
      check(std::fabs((a - b) - 12) < 1.0 && std::fabs(pass) < 0.5, "high pass slope 12 dB/oct (%.1f dB), passband %.2f dB", a - b, pass); }
    // 2. peak raises the resonance
    { Engine e0, e1; e0.p[kFreq] = e1.p[kFreq] = 1000; e0.p[kPeak] = 0; e1.p[kPeak] = 0.85f;
      const double g0 = gainDb(e0, 1000, fs), g1 = gainDb(e1, 1000, fs);
      check(g0 < -5 && g1 > 12, "peak at cutoff: %.1f dB at 0%%, %.1f dB at 85%%", g0, g1); }
    // 3. self-oscillation at the top, none just below; pitch follows the frequency
    for (float pk : { 0.9f, 1.f }) {
        Engine e; e.p[kPeak] = pk; e.p[kFreq] = 440; e.setSampleRate(fs);
        std::vector<float> in(fs * 2, 0.f), L, R; in[0] = 0.5f; run(e, in, L, R);
        const double tail = rms(L, fs, fs * 2);
        int zc = 0; for (size_t i = fs + 1; i < (size_t)fs * 2; ++i) zc += (L[i - 1] < 0) != (L[i] < 0);
        if (pk < 0.95f) check(tail < 1e-4, "no self-oscillation at 90%% peak (tail rms %.1e)", tail);
        else check(tail > 0.1 && std::fabs(zc / 2.0 - 440) < 20, "self-oscillates at 100%% peak: rms %.2f, pitch %.0f Hz (cutoff 440)", tail, zc / 2.0);
    }
    // 4. input gain saturates (adds harmonics); output gain is linear
    { auto thd = [&](float inDb) { Engine e; e.p[kFreq] = 20000; e.p[kPeak] = 0; e.p[kInGain] = inDb; e.p[kOutGain] = -inDb; e.setSampleRate(fs);
          std::vector<float> in = sine(500, 0.5f, fs, fs), L, R; run(e, in, L, R);
          double c = 0, s = 0, tot = 0; for (int i = fs / 2; i < fs; ++i) { c += L[i] * std::cos(2 * kPi * 500 * i / fs); s += L[i] * std::sin(2 * kPi * 500 * i / fs); tot += L[i] * L[i]; }
          const double N = fs / 2, fund = 2 * (c * c + s * s) / (N * N), all = tot / N; return 100 * std::sqrt(std::max(0.0, all - fund) / fund); };
      const double clean = thd(-12), hot = thd(18);
      check(clean < 1 && hot > 10, "drive: THD %.2f%% at -12 dB input, %.0f%% at +18 dB", clean, hot); }
    { Engine a, b; a.p[kFreq] = b.p[kFreq] = 20000; a.p[kPeak] = b.p[kPeak] = 0; b.p[kOutGain] = -6;
      check(std::fabs(gainDb(b, 300, fs) - gainDb(a, 300, fs) + 6) < 0.05, "output gain -6 dB: %.2f dB", gainDb(b, 300, fs) - gainDb(a, 300, fs)); }
    // 5. envelope follower: direction, depth, idle when quiet, speed
    auto efRun = [&](float level, float speed, float amp, float secs, std::vector<float>* trace) {
        Engine e; e.p[kEfLevel] = level; e.p[kEfSpeed] = speed; e.setSampleRate(fs);
        std::vector<float> in((size_t)(fs * secs)), L(256), R(256); uint32_t r = 7;
        for (size_t i = 0; i < in.size(); ++i) { r = r * 1664525u + 1013904223u; in[i] = i < in.size() / 2 ? amp * ((int32_t)r * (1.f / 2147483648.f)) : 0.f; }
        for (size_t i = 0; i + 256 <= in.size(); i += 256) { e.process(in.data() + i, in.data() + i, L.data(), R.data(), 256); if (trace) trace->push_back(e.efOct); }
        return e.efOct; };
    { std::vector<float> up, dn; efRun(1, 0.3f, 0.5f, 2, &up); efRun(-1, 0.3f, 0.5f, 2, &dn);
      const float u = up[up.size() / 2 - 2], d = dn[dn.size() / 2 - 2];
      check(u > 4 && u <= 6 && d < -4 && d >= -6, "EF level +100%%: %+.2f oct, -100%%: %+.2f oct (loud noise)", u, d);
      check(std::fabs(up.back()) < 0.05, "EF returns to rest after the sound stops (%.3f oct)", up.back()); }
    { std::vector<float> t; efRun(1, 0.5f, 0.0005f, 1, &t); check(t[t.size() / 2 - 2] < 0.3f, "EF barely moves on very quiet input (-66 dBFS: %.2f oct)", t[t.size() / 2 - 2]); }
    { std::vector<float> f, s; efRun(1, 0.f, 0.5f, 2, &f); efRun(1, 1.f, 0.5f, 2, &s);
      const int i10 = (int)(0.01f * fs / 256) + 1;
      check(f[i10] > 4 && s[i10] < f[i10] * 0.6f, "EF speed: after 10 ms fast %.2f oct, slow %.2f oct", f[i10], s[i10]); }
    { Engine e; check(std::fabs(e.p[kEfLevel]) < 1e-6, "EF off by default"); }
    // 6. switching response and moving parameters is click-free (no sample jumps beyond the signal's own)
    { Engine e; e.setSampleRate(fs); e.p[kFreq] = 800; e.p[kPeak] = 0.5f;
      std::vector<float> in = sine(200, 0.4f, fs, fs), L(fs), R(fs); float maxStep = 0;
      for (int i = 0; i < fs; i += 64) {
          if (i == fs / 4) e.p[kResponse] = 1;
          if (i == fs / 2) e.p[kFreq] = 3000;
          if (i == 3 * fs / 4) { e.p[kPeak] = 0.95f; e.p[kInGain] = 12; }
          e.process(in.data() + i, in.data() + i, L.data() + i, R.data() + i, 64); }
      for (int i = fs / 8; i < fs; ++i) maxStep = std::max(maxStep, std::fabs(L[i] - L[i - 1]));
      check(maxStep < 0.12f, "click-free response/frequency/peak/gain changes (largest step %.3f)", maxStep); }
    // 7. sample rates: cutoff lands in the same place
    for (float sr : { 44100.f, 96000.f, 192000.f }) { Engine e; e.p[kFreq] = 2000; e.p[kPeak] = 0;
      const double g = gainDb(e, 2000, sr); check(std::fabs(g + 6.02) < 0.3, "%.0f Hz: gain at cutoff %.2f dB (model -6.02)", sr, g); }
    // 8. presets: complete and in range
    { int bad = 0; for (uint32_t p = 0; p < kNumPresets; ++p) for (uint32_t k = 0; k < kParamCount; ++k) { const ParamDef& d = paramDef(k);
          if (!(d.hints & kHintOutput) && (presets()[p].v[k] < d.min || presets()[p].v[k] > d.max)) ++bad; }
      check(bad == 0 && kNumPresets >= 20, "%.0f presets, %.0f values out of range", kNumPresets, bad); }
    printf(fails ? "%d FAILURES\n" : "ALL PASSED (0 failures)\n", fails); return fails;
}
