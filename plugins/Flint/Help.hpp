// Flint — hover tips and the in-plugin help pages. License: MIT.
#pragma once
#include "FlintEngine.hpp"

namespace flint {

static const char* const kManualURL = "https://github.com/natehouk/flint/blob/main/docs/MANUAL.md";

inline const char* paramTip(uint32_t p) {
    switch (p) {
    case kFreq: return "The filter's cutoff frequency, 20 Hz to 20 kHz. Low pass keeps what is below it, high pass what is above it. You can also drag the dot in the display left and right.";
    case kPeak: return "Resonance: a peak at the cutoff frequency. Low settings are warm and smooth; higher settings squelch and whistle. At the very top the filter self-oscillates and sings a tone of its own. Drag the dot up and down for the same control.";
    case kResponse: return "LOW PASS removes the highs above the frequency (darker). HIGH PASS removes the lows below it (thinner). Both slope at 12 dB per octave. Switching is smooth, so it is safe while playing.";
    case kEfLevel: return "How far the envelope follower moves the cutoff as the input gets louder, up to six octaves. Right of centre opens the filter on loud notes (auto-wah); left of centre closes it (ducking). Centre is off.";
    case kEfSpeed: return "How quickly the envelope follower reacts. Left is fast and snappy, following every transient; right is slow and smooth, following the overall level.";
    case kInGain: return "Level going into the filter. Turning it up drives the filter into saturation, which adds warmth, then grit, and tames the resonance less politely. Balance it with Output.";
    case kOutGain: return "Level coming out of Flint. Use it to make up for Input gain and loud resonance, so you compare at equal loudness.";
    case kMidiOn: return "Turns MIDI control on or off. When off, learned controllers are ignored.";
    default: return "";
    }
}

enum TipTarget { kTipUndo, kTipRedo, kTipLearn, kTipMidi, kTipHelp, kTipA, kTipB, kTipCopy, kTipPrev, kTipPresets, kTipNext,
                 kTipHandle, kTipEfLight, kNumTipTargets };
inline const char* targetTitle(int t) {
    static const char* const n[kNumTipTargets] = { "Undo", "Redo", "MIDI Learn", "MIDI", "Help", "Setting A", "Setting B", "Copy A/B",
                                                   "Previous preset", "Presets", "Next preset", "Filter", "Envelope follower" };
    return n[t];
}
inline const char* targetTip(int t) {
    switch (t) {
    case kTipUndo: return "Steps back through your edits, including preset loads and A/B switches. Cmd/Ctrl-Z.";
    case kTipRedo: return "Re-applies an edit you undid. Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y.";
    case kTipLearn: return "Click, then click any control and move a knob or fader on your MIDI controller to assign it. Right-click a control to unassign. Assignments are saved with your project.";
    case kTipMidi: return paramTip(kMidiOn);
    case kTipHelp: return "Opens the help pages: the filter, the envelope follower, gain and saturation, A/B, MIDI Learn and shortcuts.";
    case kTipA: case kTipB: return "Flint keeps two complete settings, A and B. Click the other letter to switch and compare by ear.";
    case kTipCopy: return "Copies the current setting to the other slot.";
    case kTipPrev: return "Load the previous preset. The mouse wheel over the preset name also steps through presets.";
    case kTipPresets: return "Open the preset browser: factory presets by category, your own presets, and Save Current As. A * after the name means you have changed it. In the browser, type to search names and categories, and click a star to keep a preset in Favourites.";
    case kTipNext: return "Load the next preset.";
    case kTipHandle: return "Drag left/right to set the frequency and up/down to set the peak. The mouse wheel changes the peak. The faint curve and the bar show where the envelope follower is moving the filter right now.";
    case kTipEfLight: return "Lights up as the envelope follower moves the filter: brighter means more movement. Orange is upward (opening), blue downward (closing).";
    default: return "";
    }
}

struct HelpPage { const char* title; const char* body; };
static const HelpPage kHelpPages[] = {
    { "Overview",
      "Flint is a small, characterful filter: one 12 dB/oct low pass or high pass, with a resonance (Peak) that ranges from warm and smooth to screaming self-oscillation, "
      "an input stage that saturates when you push it, and an envelope follower that moves the cutoff with the loudness of the incoming sound.\n\n"
      "To start, drag the dot in the display: left/right is frequency, up/down is peak. Then try the presets.\n\n"
      "Hover over anything for a moment to see what it does. Turn the tips off at the bottom of this window." },
    { "The filter",
      "FREQUENCY sets the cutoff, from 20 Hz to 20 kHz.\n\n"
      "PEAK adds resonance at the cutoff. Low settings shape the tone gently; mid settings give the classic squelchy sweep; near the top the filter rings, and at the very top it "
      "oscillates by itself, producing a sine-like tone at the cutoff frequency even with no input.\n\n"
      "RESPONSE switches between LOW PASS (removes highs) and HIGH PASS (removes lows)." },
    { "Envelope follower",
      "The envelope follower listens to the incoming signal and moves the cutoff with its loudness.\n\n"
      "EF LEVEL sets how far, in either direction: right of centre opens the filter as the sound gets louder (auto-wah, funky quack), left of centre closes it (a ducking, "
      "darkening effect). Centre is off; up to six octaves at either end.\n\n"
      "EF SPEED sets how quickly it reacts: left follows every transient, right follows the overall level smoothly.\n\n"
      "The EF light and the faint second curve in the display show the movement as it happens." },
    { "Gain & saturation",
      "INPUT sets how hard the signal hits the filter. Around 0 dB the filter is clean and creamy; pushing it adds warmth, then grit and clipping, and makes high resonance "
      "rougher and more aggressive.\n\n"
      "OUTPUT sets the level after the filter. Use it to make up for Input gain and for loud resonance, so A/B comparisons and bypassing are at equal loudness.\n\n"
      "Flint softly limits its output just under 0 dBFS, so even extreme settings will not blast your speakers." },
    { "A/B & undo",
      "Flint holds two complete settings, A and B. Click B to switch to it (the first time, it starts as a copy of A); click A to go back. COPY copies the current setting into the other slot.\n\n"
      "Every edit is an undo step: moving a knob or the dot, switching response, loading a preset, switching A/B. Use UNDO/REDO in the header, or Cmd/Ctrl-Z and Cmd/Ctrl-Shift-Z." },
    { "MIDI Learn & shortcuts",
      "MIDI LEARN: click LEARN, click a control, move a knob or fader on your controller. Right-click to unassign; LEARN again or Esc to finish.\n\n"
      "Double-click a knob to type a value (1.5k, -6, 40). Cmd/Ctrl-click a knob resets it. Shift-drag is fine adjustment; grab a knob's rim to turn it in a circle. "
      "The mouse wheel works on knobs, on the display (peak) and on the preset name.\n"
      "Cmd/Ctrl-Z undo, Cmd/Ctrl-Shift-Z redo. Esc closes this window." },
};
static constexpr int kNumHelpPages = sizeof(kHelpPages) / sizeof(kHelpPages[0]);

} // namespace flint
