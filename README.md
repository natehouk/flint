# Flint

**A small saturating filter with an envelope follower, for macOS, Windows and Linux (VST3 · AU · CLAP · VST2).**

Part of **[Epsilon Audio](https://github.com/natehouk/epsilon-plugins)**, a family of free, open-source plugins: Monolith, Caldera, Obsidian, Strata, Basalt, Gneiss, Mica, Flint, Granite, Magma, Schist, Slate, Talc, Geode and Malachite.

![Flint](screenshot.png)

Flint is a clean-room design with the feature set of FabFilter Micro, FabFilter's eighth plugin (January 2010). Micro was a lightweight single filter built on the filter first developed for FabFilter One. Flint was written from scratch, working only from FabFilter's public press release, product page and online help. No FabFilter code, binaries, presets, artwork or trademarks were used.

It is part of a family of plugins:
- [Monolith](https://github.com/natehouk/monolith) (synth). Flint uses Monolith's filter.
- [Caldera](https://github.com/natehouk/caldera) (filter)
- [Obsidian](https://github.com/natehouk/obsidian) (polysynth)
- [Strata](https://github.com/natehouk/strata) (tape delay)
- [Basalt](https://github.com/natehouk/basalt) (live dual filter)
- [Gneiss](https://github.com/natehouk/gneiss) (compressor)
- [Mica](https://github.com/natehouk/mica) (equalizer)
- **Flint**, named after the small, hard stone that throws sparks

## Download

Get the latest build from [**Releases**](../../releases):

| Platform | File | Contains |
|---|---|---|
| macOS 10.13+ (Apple Silicon + Intel) | `Flint-macos-universal.zip` | VST3, AU, CLAP, manual |
| Windows x64 | `Flint-windows-x64.zip` | VST3, CLAP, VST2, manual |
| Linux x64 | `Flint-linux-x64.zip` | VST3, CLAP, VST2, manual |

### Install

| Platform | Copy | To |
|---|---|---|
| macOS | `Flint.vst3` / `Flint.component` / `Flint.clap` | `~/Library/Audio/Plug-Ins/VST3` / `Components` / `CLAP` |
| Windows | `Flint.vst3` (folder) / `Flint.clap` | `C:\Program Files\Common Files\VST3\` / `CLAP\` |
| Linux | `Flint.vst3` / `Flint.clap` | `~/.vst3/` / `~/.clap/` |

**macOS:** the plugins are ad-hoc signed, not notarized. After copying, run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Flint*
```

## Features

**Filter**
- **Low pass or high pass** at **12 dB/oct**, with a click-free switch between them.
- **Frequency** from 20 Hz to 20 kHz.
- **Peak** (resonance) goes from warm and smooth up to **self-oscillation**: at the top, the filter sings a sine-like tone at the cutoff.
- **Input gain drives a saturating stage.** It is clean and creamy at 0 dB, and gritty and clipped when pushed.
- **Output gain** balances the level, and a soft output ceiling keeps it below 0 dBFS.

**Envelope follower**
- **EF Level** moves the cutoff **up or down by as much as six octaves** as the input gets louder. Upward gives auto-wah; downward darkens and ducks the highs.
- **EF Speed** goes from instant and snappy to slow and smooth.
- The **EF light** and a second curve in the display show the movement as it happens.

**Display and workflow**
- **Interactive display.** Drag the dot to set frequency and peak, click anywhere to move the cutoff, and use the mouse wheel to change peak. A bar shows how far the envelope follower can reach.
- **A/B comparison** with copy between the two settings.
- **Undo/redo** for every edit, including A/B switches.
- **MIDI Learn.**
- **Smoothed parameters**, sample-accurate host automation and a resizable interface.
- **24 factory presets** in four categories (Tone, Envelope, Resonant, Drive), plus your own user presets.
- The preset browser **searches** names and categories as you type, and a star keeps a preset in **Favourites**.
- **Hover tips** on every control, plus a **HELP window**.
- **[Full manual](docs/MANUAL.md)**, also in every release zip as `Flint-Manual.html`.

### Compared with FabFilter Micro (2010)

| Micro | Flint |
|---|---|
| One filter based on the FabFilter One filter | ✓ Monolith's filter (this family's One) |
| Low pass and high pass, 12 dB/oct | ✓ |
| Peak up to self-oscillation, saturating at full resonance | ✓ |
| Envelope follower on cutoff, either direction (EF Level, EF Speed, EF light) | ✓ plus the live curve and reach bar in the display |
| Separate input and output gain driving the saturation | ✓ |
| Interactive filter display | ✓ |
| Interactive MIDI Learn, undo/redo, A/B switch | ✓ |
| Smart parameter interpolation, sample-accurate automation | ✓ |
| Help file with interactive hints | ✓ hover tips, in-plugin help pages and full manual |
| Scalable interface | ✓ resizable |
| Factory presets (none listed for Micro) | 24 factory presets plus user presets |
| Pro Tools (RTAS/AAX) | Not supported (needs Avid's SDK and signing) |

## Build from source

The plugin is built on [DPF](https://github.com/DISTRHO/DPF):

```
git clone --recursive https://github.com/DISTRHO/DPF.git dpf
make -C plugins/Flint                    # Linux / macOS (macOS also builds the AU)
make -C plugins/Flint WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++
```

GitHub Actions builds all three platforms on every push. The macOS build is universal and ad-hoc signed. A `v*` tag, or a release published on GitHub, attaches the zips to that release.

## Tests

```
cd tests
g++ -O2 -std=c++11 features.cpp -o features && ./features   # response vs the analog model, slopes, peak and self-oscillation pitch,
                                                            # drive (THD), envelope follower depth/direction/speed, clicks, sample rates
g++ -O2 -std=c++11 fuzz.cpp -o fuzz && ./fuzz               # random parameters at 44.1-192 kHz with loud noise: always finite, <= 0 dBFS
g++ -O2 -std=c++11 presets.cpp -o presets && ./presets      # plays noise and a bass line through every preset
g++ -O3 -std=c++11 bench.cpp -o bench && ./bench            # CPU with the envelope follower and drive working
```

Flint takes about 0.6% of one CPU core.

Release checks:
- Steinberg's VST3 validator passes 47/47.
- The interface is exercised with scripted input and live audio: dragging the dot, the envelope follower display, A/B, undo, tips, help and MIDI Learn.

## Source layout

| File | Contents |
|---|---|
| `plugins/Flint/FlintEngine.hpp` | All DSP: saturating state-variable filter, envelope follower, smoothing. Header-only |
| `plugins/Flint/ParamDefs.hpp` | Parameter table, labels, value mapping |
| `plugins/Flint/Presets.hpp` | Factory presets |
| `plugins/Flint/UserPresets.hpp` | User preset files |
| `plugins/Flint/Help.hpp` | Hover tip text and in-plugin help pages |
| `plugins/Flint/FlintPlugin.cpp` | Plugin wrapper: parameters, programs, MIDI Learn |
| `plugins/Flint/FlintUI.cpp` | Custom vector interface (NanoVG) |
| `docs/MANUAL.md` | User manual (`Flint-Manual.html` is generated from it with pandoc) |

New parameters are only ever appended, so saved sessions and automation keep working across versions.

## License

[MIT](LICENSE). Built with [DPF](https://github.com/DISTRHO/DPF) and its bundled NanoVG, which are under the ISC license. `plugins/Flint/ResizeHandle.hpp` comes from DPF's examples and keeps its ISC notice.

"FabFilter" and "FabFilter Micro" are trademarks of FabFilter Software Instruments. Flint is not affiliated with or endorsed by FabFilter.
