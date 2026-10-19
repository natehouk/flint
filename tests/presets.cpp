// Run noise plus a bass line through every factory preset; report levels.
#include "../plugins/Flint/ParamDefs.hpp"
#include "../plugins/Flint/Presets.hpp"
#include <cstdio>
#include <vector>
using namespace flint;
int main() {
    const int SR = 48000, N = SR * 3; int bad = 0;
    std::vector<float> x(N); uint32_t r = 3; float lp = 0;
    for (int i = 0; i < N; ++i) { r = r * 1664525u + 1013904223u; const float w = (int32_t)r * (1.f / 2147483648.f); lp += 0.05f * (w - lp);
        x[i] = 0.2f * (w * 0.5f + lp * 2.f) + 0.3f * std::sin(2 * kPi * 55 * i / (float)SR) + 0.15f * std::sin(2 * kPi * 440 * i / (float)SR); }
    double si = 0; for (float v : x) si += v * v; const float inR = (float)std::sqrt(si / N);
    for (uint32_t p = 0; p < kNumPresets; ++p) {
        Engine e; e.setSampleRate(SR); for (uint32_t k = 0; k < kParamCount; ++k) e.p[k] = presets()[p].v[k]; e.reset();
        std::vector<float> L(N), R(N); for (int i = 0; i < N; i += 512) e.process(x.data() + i, x.data() + i, L.data() + i, R.data() + i, std::min(512, N - i));
        double s = 0; float pk = 0; bool nf = false;
        for (int i = 0; i < N; ++i) { s += L[i] * L[i] + R[i] * R[i]; pk = std::max(pk, std::max(std::fabs(L[i]), std::fabs(R[i]))); nf |= !std::isfinite(L[i]); }
        const float rel = 20 * std::log10(std::sqrt(s / (2.0 * N)) / inR + 1e-9f);
        const bool ok = !nf && rel > -20 && rel < 6 && pk <= 1.f; bad += !ok;
        printf("%-16s %-12s level %+6.1f dB  peak %.2f %s\n", presets()[p].name, presets()[p].category, rel, pk, ok ? "" : "<-- CHECK");
    }
    printf("%u presets, %d flagged\n", kNumPresets, bad); return bad;
}
