# NewSynth

A native macOS Audio Unit instrument for Logic Pro: a wavetable synthesizer with a
built-in **dynamic just-intonation engine**. Play a chord and every voice is retuned
to pure JI ratios relative to the detected root — no MPE, no external synth, tuning
is per-voice inside the engine.

Built with C++17 / [JUCE 8](https://juce.com) via CMake FetchContent.

## Features

- **4 independent oscillators**, each in wavetable or noise mode, with wavetable
  position (frame morph), up to **16 unison voices** with cent-spread detune, and level.
- **8-voice polyphony.**
- **Wavetables**: built-in sine→triangle→saw→square morph table; load custom tables
  from .wav/.aiff files as consecutive 2048-sample frames (Serum-style), up to
  **256 frames** per preset. All frames are pre-rendered into band-limited mip
  levels so playback never aliases. Visual waveform display per oscillator.
- **Dynamic JI tuning engine**:
  - JI ratios configurable for all 12 chromatic degrees above the root.
    Defaults: `1/1 16/15 9/8 6/5 5/4 4/3 45/32 3/2 8/5 5/3 7/4 15/8`.
  - Per-degree cent detune (drag vertically on a ratio bar; double-click resets).
  - Auto root detection: **lowest note** (or highest, with *Note From Top*) or
    **most-played** pitch class, gated by a **Notes to Trigger** threshold.
    The root is kept on release and only moves when a new chord of at least
    that many notes is held.
  - Manual root override (*Root Source: Manual* + **Root Note** knob).
  - **Glide** between tunings: when the root moves, sounding voices slide to
    their new frequencies (0–2000 ms).
  - Pitch bend with configurable range (0–48 semitones); optional
    **Single Note Pitchbend** (bend only applies when one note is held).
- Amp ADSR, master volume.

The root pitch class is anchored to its 12-TET frequency, so the detected root
always lands exactly where an equal-tempered instrument would put it; every other
note is a pure ratio above or below it.

## Download and install (macOS 11+, Apple Silicon or Intel)

1. Download `NewSynth-macOS.zip` from the [latest release](https://github.com/grantshueh/newsynth/releases/latest) and unzip it.
2. Copy `NewSynth.component` to `~/Library/Audio/Plug-Ins/Components/` (Logic Pro, GarageBand), `NewSynth.vst3` to `~/Library/Audio/Plug-Ins/VST3/` (other DAWs), and `NewSynth.app` to `/Applications` if you want the standalone.
3. The builds are not signed by Apple. Clear the quarantine flag once in Terminal:

   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/NewSynth.component ~/Library/Audio/Plug-Ins/VST3/NewSynth.vst3 /Applications/NewSynth.app
   ```

4. In Logic Pro: Software Instrument track → **AU Instruments → AloBoi → NewSynth**.

## Building

Requires macOS command-line tools and CMake (`brew install cmake`). Full Xcode is
not needed.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target NewSynth_AU -j8
```

The AU component is copied to `~/Library/Audio/Plug-Ins/Components/NewSynth.component`
automatically after the build. Validate and register it with:

```bash
auval -v aumu NsJi AlBo
```

Then in Logic Pro: Software Instrument track → **AU Instruments → AloBoi → NewSynth**.
(If Logic was running during the install, restart it or run
`killall -9 AudioComponentRegistrar` first.)

A standalone app target (`NewSynth_Standalone`) is also built for quick testing
without a host.

## Headless engine tests

The tuning engine is pure C++ (no JUCE) and unit-tested:

```bash
cmake --build build --target TuningTests && ./build/TuningTests
```

or directly, without CMake:

```bash
clang++ -std=c++17 -I Source Tests/TuningTests.cpp -o tuning_tests && ./tuning_tests
```

## Project layout

```
Source/
  Engine/
    TuningEngine.h    dynamic JI root detection + frequency mapping (pure C++)
    Wavetable.h/.cpp  wavetable frames with band-limited mip levels (FFT)
    SynthEngine.h     8-voice engine: 4 oscs x 16 unison per voice, ADSR, glide
  PluginProcessor.*   AU processor, parameters (APVTS), MIDI handling, state
  PluginEditor.*      dark/green UI: ratio bars, waveform display, knobs
Tests/
  TuningTests.cpp     headless tuning tests
```

## License

NewSynth is free software under the **GNU AGPLv3** (see [LICENSE](LICENSE)). It is built with [JUCE](https://juce.com) under JUCE's AGPLv3 option.
