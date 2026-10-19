// CPU: high peak, envelope follower on (fast), input driven, frequency moving every block.
#include "../plugins/Flint/ParamDefs.hpp"
#include <chrono>
#include <cstdio>
using namespace flint;
int main() {
    const int SR = 48000; Engine e; e.setSampleRate(SR);
    e.p[kPeak] = 0.9f; e.p[kEfLevel] = 0.8f; e.p[kEfSpeed] = 0.1f; e.p[kInGain] = 12;
    static float in[SR], L[SR], R[SR]; uint32_t r = 1; for (float& x : in) { r = r * 1664525u + 1013904223u; x = 0.3f * ((int32_t)r * (1.f / 2147483648.f)); }
    const int secs = 30; auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < secs; ++k) for (int b = 0; b < SR; b += 256) { e.p[kFreq] = 200 + (b % 5000); e.process(in + b, in + b, L + b, R + b, 256); }
    const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("%d s of audio in %.3f s: %.2f%% of one core\n", secs, el, 100 * el / secs); return 0;
}
