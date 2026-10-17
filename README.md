# Metal Sub Drop

A monophonic sub-drop ("boom") synth for metal production. Windows x64 **VST3** and **CLAP**, plus a Standalone app for quick testing.

Play a note and you get a clean sine boom whose pitch drops (or rises into key). The controls are aimed at fitting that boom into a dense mix with kick, bass guitar and down-tuned guitars.

## Getting the Windows build

### Option A: GitHub Actions (no Windows machine needed)
Push this repo to GitHub. The workflow in `.github/workflows/build.yml` builds on `windows-latest` and uploads an artifact called **MetalSubDrop-Windows-x64** containing:

- `Metal Sub Drop.vst3`
- `Metal Sub Drop.clap`
- `Metal Sub Drop.exe` (standalone)

You can also start it manually from the Actions tab (*Run workflow*).

### Option B: Build on Windows
Requirements: Visual Studio 2022 or newer with **Desktop development with C++**, CMake 3.22+, and Git. Then run:

```bat
build-windows.bat
```

JUCE 8 and clap-juce-extensions are downloaded automatically by CMake. The output goes to `build\MetalSubDrop_artefacts\Release\`.

### Install
| Format | Copy to |
|---|---|
| VST3 | `C:\Program Files\Common Files\VST3\` (copy the whole `.vst3` folder) |
| CLAP | `C:\Program Files\Common Files\CLAP\` |

The MSVC runtime is linked statically, so you don't need to install a VC++ redistributable.

## Controls

### Pitch Drop
| Control | What it does |
|---|---|
| **Tune / Fine** | Transpose in semitones/cents. Use these to match the song key. |
| **Drop** | How far the pitch travels, in semitones (12 = one octave). |
| **Time** | How long the drop takes. Turn on **Sync** to set it in host tempo instead (1/16 to 4 bars). |
| **Curve** | Positive values fall fast, then settle (punchy, 808-style). Negative values hold, then dive. |
| **Floor** | The pitch never goes below this frequency. It keeps the tail audible instead of turning into rumble. |
| **Punch** | Adds a short pitch blip at the start so the hit cuts through guitars. |
| **Fall / Land** | **Fall** starts on the played note and drops. **Land** starts above and lands on the played note, so the boom ends in key. |

### Amp
**Attack**, **Hold** and **Decay** shape the level over time. **Release** applies only in **Gated** mode. **Velocity** sets how much MIDI velocity affects the level.

The amp has two trigger modes:
- **One-Shot** (default): every hit plays the whole envelope.
- **Gated**: note-off starts the release, so the MIDI note length controls how long the boom lasts.

### Mix Fit
| Control | What it does |
|---|---|
| **Drive** | Saturation, with automatic level compensation. It adds harmonics so the sub is heard on phones and laptops. |
| **Low Cut** | 24 dB/oct high-pass filter. It removes infrasonic energy that uses up headroom without being audible. |
| **High Cut** | Low-pass filter that keeps the drive harmonics away from the bass guitar and the kick click. |
| **Ceiling** | Soft-clip ceiling on the output, so the boom can't overshoot and slam your bus compressor or limiter. |
| **Output** | Output level. |

The display shows the pitch curve over time, the amp envelope, the Floor (dashed line) and the Low Cut region (shaded). The readout shows the start and end note and frequency. It warns you if the drop ends below the Low Cut, which means part of the boom is being filtered away.

**Audition** plays the last received note (E1 by default), so you can tweak the sound without a MIDI clip.

### Factory presets
The **Presets** menu has six starting points:
- Init Drop
- Breakdown Boom
- Tight Impact
- One Bar Synced
- Small Speaker Growl
- Rise Into Key

## Tips for fitting the boom in a metal mix
1. **Tune it first.** Set the drop so it ends on the song's root note, or use **Land** mode. A boom that's out of key against the bass guitar sounds muddy even when the levels are right.
2. **Set Floor to around 25–35 Hz and Low Cut just below it.** Most systems can't reproduce anything below that. Energy down there only eats headroom and triggers your master limiter.
3. **Keep it mono.** The plugin outputs the same signal on both channels by design.
4. **Use Drive + High Cut together for translation.** Add drive until the boom is audible on small speakers, then pull High Cut down (800 Hz–2 kHz) until it stops fighting the guitars.
5. **Use Ceiling as a safety.** Set it a few dB below 0 so the transient never spikes. Then set the level with Output.
6. **Sidechain the kick.** Duck the boom a little from the kick in your DAW, or keep Punch low so the two transients don't stack.

## Why CLAP as well?
Both formats come from the same code. For this plugin, CLAP adds:
- Better parameter handling in CLAP hosts (Bitwig, REAPER, Studio One and others): sample-accurate automation events and host-side modulation of any parameter.
- An open, MIT-licensed format with no SDK licensing agreement required.
- More reliable state and threading rules. The plugin passes `clap-validator` with no failures.

If your DAW supports CLAP, use the CLAP version. Otherwise use VST3; it sounds identical.

## Development notes
- `Source/SubDropEngine.h` is the DSP: a plain C++ voice with no JUCE dependency. The editor's display reuses `pitchAt()` from it.
- `Source/PluginProcessor.*` handles parameters, sample-accurate MIDI, tempo sync, presets and state.
- `Source/PluginEditor.*` is the UI.
- The plugin was validated with **pluginval** (strictness 10, including the Steinberg VST3 validator) and with **clap-validator**.
- Parameter IDs are stable. If you add parameters, give them new IDs rather than renaming existing ones, or saved projects will lose their settings.
