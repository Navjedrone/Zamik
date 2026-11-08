# Zamik

Zamik moves audio earlier or later in time, by whole samples, and does nothing else.
Samples are copied to new positions: no gain, interpolation, dither, fades or resampling.
On the normal path the output is bit-identical to the input, only shifted.

Formats: AU and VST3 on macOS (Apple Silicon and Intel), VST3 on Windows, plus a standalone app.

## Using it

- **◀ EARLIER / LATER ▶** move the audio. Hold a button to repeat.
- **MS** steps in milliseconds: 1 ms, with ÷8 ÷4 ÷2 ×2 ×4 ×8 (0.125 ms to 8 ms).
- **SMP** steps in samples: 1, 2, 4, 8, 16, 32 or 64.
- **NOTE** steps in tempo-synced divisions from 1/128 to 1/1, straight, triplet or dotted, plus BAR, which follows the host's time signature. The delay you set in MS or SMP stays as a fixed base and note steps add on top of it. **SNAP** clears the base and moves to the nearest grid line.
- **DUAL** splits the display in two. Each row has its own arrows and moves one channel: L and R, or M and S.
  - **SYNC** locks the gap between the two; the upper row then moves both.
  - **BAL** (in M/S) opens the mid/side balance. **AUTO GAIN** keeps the RMS level the same: it is measured from the last few seconds of audio when you move the slider, then stays fixed, so it never rides the gain.
- **A B C D** store four variations to compare. An empty variation starts as a copy of the current one.
- **ADVANCE RANGE** sets how far audio can move earlier. Zamik reports this much latency and the host compensates for it. Keep it as small as you need, because it also delays live monitoring through the track.
- **LIMIT** lights when the shift you asked for does not fit the range. The caption says which advance setting is needed; click the lamp to set it.
- **BYPASS** passes audio unshifted while still applying the reported latency, so the track stays in time with the rest of the session.

Keys (click the plugin window first): ← → move, Shift + ← → move the lower row, 0 zero, M cycle modes, A–D variations.

## Bit transparency

The BIT-TRANSPARENT lamp is on whenever the audio takes the copy path, which is every setting except one:
DUAL + M/S with different mid and side offsets, or a balance away from center. Those need a sum/difference
matrix, which is arithmetic; the M/S MATRIX lamp lights instead. Equal mid/side offsets at center skip the
matrix and stay bit-transparent.

`tests/Tests.cpp` checks this. It feeds random 32-bit and 64-bit words (including NaN payloads, denormals,
infinities and signed zeros) through the real processor in uneven block sizes, at 44.1, 48 and 96 kHz, in
every mode, and compares every output word against the input moved by exactly the reported latency plus the
offset. Host bypass, the bypass parameter, state save/restore and the matrix flag are checked too.

## Installing a build

macOS:
- Copy `Zamik.component` to `~/Library/Audio/Plug-Ins/Components/`
- Copy `Zamik.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
- The builds are not notarized. If macOS refuses to load them, run once in Terminal:
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Zamik.component ~/Library/Audio/Plug-Ins/VST3/Zamik.vst3`
- Logic Pro may need a rescan: Settings › Plug-in Manager › Reset & Rescan Selection.

Windows: copy `Zamik.vst3` to `C:\Program Files\Common Files\VST3\`.

## Building

Needs CMake 3.22+ and a C++17 compiler (Xcode on macOS, Visual Studio 2022 on Windows).
JUCE 8.0.10 is downloaded automatically, or put a JUCE checkout in `./JUCE`.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/ZamikTests_artefacts/Release/ZamikTests      # bit-transparency tests
```

Plugins land in `build/Zamik_artefacts/Release/` (AU, VST3, Standalone).

On GitHub, every push to `main` runs `.github/workflows/build.yml`: it builds the macOS AU, VST3 and
standalone app (universal) and the Windows VST3, runs the bit-transparency tests, `auval -strict` and
pluginval, and attaches `Zamik-macOS.zip` and `Zamik-Windows.zip` to the run. Pushing a tag like `v1.0.0`
also attaches both zips to a GitHub release.

## Licences

JUCE is used under its own licence (AGPLv3 or a commercial JUCE licence; check which applies before
distributing). Fonts: Barlow Condensed and JetBrains Mono, both SIL Open Font License, included in
`Resources/fonts`.
