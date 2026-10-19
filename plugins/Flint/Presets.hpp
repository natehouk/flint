// Factory presets (each lists only what it changes from the engine defaults). License: MIT.
// Engine defaults: low pass at 1 kHz, peak 30%, envelope follower off (speed 50%), gains 0 dB.
#pragma once
#include "FlintEngine.hpp"
#include <algorithm>

namespace flint {

struct PV { uint32_t id; float v; };
struct PresetDef { const char* name; const char* category; const PV* pv; };
struct Preset { const char* name; const char* category; float v[kParamCount]; };

static const char* const kCategories[] = { "Tone", "Envelope", "Resonant", "Drive" };
static constexpr int kNumCategories = 4;

#define F(hz, pk, rsp) {kFreq,hz},{kPeak,pk},{kResponse,rsp}
#define EF(lv, sp) {kEfLevel,lv},{kEfSpeed,sp}
#define END { kParamCount, 0 }
namespace pv {
static const PV init[] = { END };
// ---------------------------------------------------------------- Tone
static const PV warm[]     = { F(3500, .15f, 0), END };
static const PV muffled[]  = { F(450, .1f, 0), {kOutGain,2}, END };
static const PV thin[]     = { F(700, .2f, 1), {kOutGain,3}, END };
static const PV clearLow[] = { F(90, .25f, 1), END };
static const PV telephone[] = { F(1800, .55f, 0), {kInGain,6}, {kOutGain,-3}, END };
static const PV creamy[]   = { F(1400, .45f, 0), {kInGain,3}, {kOutGain,-1}, END };
// ---------------------------------------------------------------- Envelope
static const PV autoWah[]   = { F(300, .65f, 0), EF(.7f, .35f), {kOutGain,-2}, END };
static const PV funkQuack[] = { F(220, .78f, 0), EF(.85f, .2f), {kOutGain,-4}, END };
static const PV duckDark[]  = { F(9000, .25f, 0), EF(-.6f, .55f), END };
static const PV slowBloom[] = { F(250, .4f, 0), EF(.55f, .9f), END };
static const PV snapOpen[]  = { F(150, .35f, 0), EF(1.f, 0.f), END };
static const PV hpBreath[]  = { F(2500, .4f, 1), EF(-.7f, .6f), {kOutGain,3}, END };
static const PV drumSkin[]  = { F(600, .6f, 0), EF(.5f, .15f), {kInGain,4}, {kOutGain,-3}, END };
// ---------------------------------------------------------------- Resonant
static const PV squelch[]  = { F(500, .88f, 0), {kOutGain,-4}, END };
static const PV whistle[]  = { F(1200, .94f, 1), {kOutGain,-6}, END };
static const PV singing[]  = { F(440, 1.f, 0), {kOutGain,-6}, END };
static const PV acidLine[] = { F(380, .9f, 0), EF(.6f, .25f), {kInGain,6}, {kOutGain,-6}, END };
static const PV ringing[]  = { F(2200, .97f, 1), EF(.3f, .4f), {kOutGain,-6}, END };
// ---------------------------------------------------------------- Drive
static const PV crunch[]   = { F(5000, .2f, 0), {kInGain,12}, {kOutGain,-8}, END };
static const PV fuzzBox[]  = { F(2400, .5f, 0), {kInGain,20}, {kOutGain,-14}, END };
static const PV hotTape[]  = { F(11000, .1f, 0), {kInGain,8}, {kOutGain,-5}, END };
static const PV angryHP[]  = { F(350, .7f, 1), {kInGain,15}, {kOutGain,-10}, END };
static const PV broken[]   = { F(900, .85f, 0), EF(.4f, .1f), {kInGain,18}, {kOutGain,-12}, END };
} // namespace pv
#undef F
#undef EF
#undef END

static const PresetDef kPresetDefs[] = {
    { "Init", "Init", pv::init },
    { "Warm", "Tone", pv::warm }, { "Muffled", "Tone", pv::muffled }, { "Thin", "Tone", pv::thin },
    { "Clear the Low End", "Tone", pv::clearLow }, { "Telephone", "Tone", pv::telephone }, { "Creamy", "Tone", pv::creamy },
    { "Auto Wah", "Envelope", pv::autoWah }, { "Funk Quack", "Envelope", pv::funkQuack }, { "Duck Dark", "Envelope", pv::duckDark },
    { "Slow Bloom", "Envelope", pv::slowBloom }, { "Snap Open", "Envelope", pv::snapOpen }, { "High Pass Breath", "Envelope", pv::hpBreath },
    { "Drum Skin", "Envelope", pv::drumSkin },
    { "Squelch", "Resonant", pv::squelch }, { "Whistle", "Resonant", pv::whistle }, { "Singing", "Resonant", pv::singing },
    { "Acid Line", "Resonant", pv::acidLine }, { "Ringing", "Resonant", pv::ringing },
    { "Crunch", "Drive", pv::crunch }, { "Fuzz Box", "Drive", pv::fuzzBox }, { "Hot Tape", "Drive", pv::hotTape },
    { "Angry High Pass", "Drive", pv::angryHP }, { "Broken", "Drive", pv::broken },
};
static constexpr uint32_t kNumPresets = sizeof(kPresetDefs) / sizeof(kPresetDefs[0]);

inline const Preset* presets() {
    static Preset table[kNumPresets];
    static bool done = false;
    if (done) return table;
    done = true;
    Engine e;
    for (uint32_t i = 0; i < kNumPresets; ++i) {
        table[i].name = kPresetDefs[i].name; table[i].category = kPresetDefs[i].category;
        std::copy(e.p, e.p + kParamCount, table[i].v);
        for (const PV* x = kPresetDefs[i].pv; x->id != kParamCount; ++x) table[i].v[x->id] = x->v;
    }
    return table;
}

} // namespace flint
