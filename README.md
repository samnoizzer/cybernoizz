# CYBERNOIZZ — High-Gain Industrial Metal Amp Sim (VST3 / Standalone)

**CYBERNOIZZ** is a low-CPU, zero-latency C++17 / JUCE 8 VST3 & Standalone guitar amplifier simulator engineered to recreate **Dino Cazares' 1995 *Demanufacture* guitar tone** (his Elan Memran / Metalhead-modded Marshall JCM800 2203 head with two extra 12AX7 preamp stages and 6550 power tubes driving Bad Brains guitarist Dr. Know's loaned oversized Mesa/Boogie Rectifier 4x12 and Marshall 1960 4x12 cabinets at Bearsville Studios).

---

## Core Features & Signal Chain

1. **Routing Modes (`Mono`, `Mono -> Stereo`, `Stereo`)**
   - Automatically skips redundant second-channel preamp/gate processing in `Mono` and `Mono -> Stereo` modes for **50% lower CPU usage**.
2. **`NR` Knob (Cable & Static Noise Reduction)**
   - Dual high-Q hum notch filters (60 Hz & 120 Hz) + adaptive RF static low-pass + steep 3:1 downward expander to eliminate bad cable buzz and active-pickup static hiss before high-gain saturation.
3. **`Tight` Button (Intelligent Gunshot Noise Gate)**
   - Dual-band detector comparing palm-mute chug resonance (`105 Hz`) against upper-mid/harmonic sustain (`>1.05 kHz`).
   - Clamps down in **8.5 ms** for machine-gun staccato palm-mutes while automatically switching to a smooth **95 ms** release when you hold sustained notes, chords, or harmonics.
4. **`Boost` Knob (Pick-Hit Sub-Bass & Kick Drum Generator)**
   - Transient crest detector + palm-mute weight analyzer that synthesizes a subtle **56 Hz kick-drum chest thump** and a **phase-locked sub-octave backing bass guitar note** when the pick strikes the strings, locking your palm mutes to the kick and bass like Raymond Herrera & Dino Cazares.
5. **Elan Metalhead 2203 Preamp + 6550 Power Amp Section**
   - 4-stage cascaded asymmetric 12AX7 + 6550 tube waveshaper running at **2x polyphase IIR oversampling** using Padé `[3/3]` rational `fastTanh` approximations and `juce::ScopedNoDenormals`.
   - Full tone stack (`Gain`, `Bass`, `Mid`, `Industrial Scoop` @ 560 Hz, `Treble`, `Presence` @ 4.25 kHz, `Depth` @ 96 Hz).
6. **Dual Cabinet IR Loader Bars (Left & Right Cabs + Mix, Volume & Pan)**
   - Zero-latency time-domain SIMD FIR convolver (`FastCabConvolver`) with automatic sample-rate resampling and RMS normalization.
   - Includes **8 embedded 48 kHz / 24-bit `.wav` Impulse Responses** in `Resources/IRs/` plus `LOAD WAV...` buttons to load any external `.wav` / `.aif` / `.flac` IR files.
7. **`Spread` Knob (Phase-Safe Haas & 200% Stereo Widener)**
   - Splits the signal at **220 Hz** so everything below 220 Hz stays **100% mono phase-locked** in the center.
   - Above 220 Hz: applies an orthogonalized Haas stereo doubler in `Mono -> Stereo` mode, and expands stereo width from **100% up to 200%** in `Stereo` mode with zero low-end phase cancellation.
8. **Futuristic Resizable 3D PCB GUI**
   - Vector-rendered carbon-black and dark metallic gray chassis with procedural 45-degree neon-green (`#00FF66`) PCB traces, plated via holes, 3D knurled turret knobs, `BYPASS`, `UNDO`, `REDO`, `A / B / COPY` comparison slots, preset `SAVE / LOAD` (`.cynz`), and dual `IN L/R` & `OUT L/R` LED bar meters.

---

## Included Factory Presets (14 Fear Factory Tones)

| # | Preset Name | Era / Inspiration | Left Cab IR | Right Cab IR |
|---|---|---|---|---|
| 01 | `01 Demanufacture (Album Rhythm)` | *Demanufacture* (1995) Title Track | 01 Bearsville Recto 4x12 V30 | 02 ZeroSignal 1960 G12T75 |
| 02 | `02 Self Bias Resistor (Quad Pick)` | *Demanufacture* (1995) | 01 Bearsville Recto 4x12 V30 | 03 Replica Hybrid 4x12 |
| 03 | `03 Zero Signal (Machine Gun Staccato)` | *Demanufacture* (1995) | 02 ZeroSignal 1960 G12T75 | 04 NewBreed Industrial Scoop |
| 04 | `04 Replica (Radio Hook Chug)` | *Demanufacture* (1995) | 03 Replica Hybrid 4x12 | 01 Bearsville Recto 4x12 V30 |
| 05 | `05 New Breed (Cyber Industrial Grind)` | *Demanufacture* (1995) | 04 NewBreed Industrial Scoop | 02 ZeroSignal 1960 G12T75 |
| 06 | `06 Dog Day Sunrise (Industrial Sustained)` | *Demanufacture* (1995) | 03 Replica Hybrid 4x12 | 08 CyberTight Studio 2x12 |
| 07 | `07 Body Hammer (Crushing Low B Groove)` | *Demanufacture* (1995) | 01 Bearsville Recto 4x12 V30 | 05 Obsolete EdgeCrusher 4x12 |
| 08 | `08 Flashpoint (Hyper-Pick Thrash Razor)` | *Demanufacture* (1995) | 02 ZeroSignal 1960 G12T75 | 08 CyberTight Studio 2x12 |
| 09 | `09 H-K (Hunter-Killer Cyber Wall)` | *Demanufacture* (1995) | 01 Bearsville Recto 4x12 V30 | 04 NewBreed Industrial Scoop |
| 10 | `10 Pisschrist (Mechanical Thrash Assault)` | *Demanufacture* (1995) | 01 Bearsville Recto 4x12 V30 | 02 ZeroSignal 1960 G12T75 |
| 11 | `11 Shock (Obsolete 7-String Drop-A)` | *Obsolete* (1998) | 05 Obsolete EdgeCrusher 4x12 | 01 Bearsville Recto 4x12 V30 |
| 12 | `12 Edgecrusher (Sub-Drop Behemoth)` | *Obsolete* (1998) | 05 Obsolete EdgeCrusher 4x12 | 07 Mechanize Laboga 4x12 |
| 13 | `13 Martyr ('92 Soul Of A New Machine)` | *Soul of a New Machine* (1992) | 06 SoulOfANewMachine '92 4x12 | 02 ZeroSignal 1960 G12T75 |
| 14 | `14 Powershifter (Mechanize Precision)` | *Mechanize* (2010) | 07 Mechanize Laboga 4x12 | 08 CyberTight Studio 2x12 |

---

## Compiling on GitHub Actions (Automatic Windows x64 VST3 Build)

1. Create a new GitHub repository and push the contents of this project folder to the `main` branch.
2. GitHub Actions will automatically run `.github/workflows/build.yml` on `windows-latest` with Visual Studio 2022 (`x64`), automatically fetching the latest **JUCE 8** via CMake `FetchContent`, embedding all 8 `.wav` IRs into `BinaryData`, and compiling with `/O2 /Oi /Ot /arch:AVX2 /fp:fast /GL /LTCG`.
3. When the workflow finishes (~3–4 minutes), open the **Actions** tab on GitHub, click the completed workflow run, and download **`CYBERNOIZZ-Windows-x64-VST3`**, which contains:
   - `CYBERNOIZZ.vst3` (copy to `C:\Program Files\Common Files\VST3\`)
   - `CYBERNOIZZ.exe` (Standalone app)
   - `Factory_IRs/` (all 8 standalone `.wav` IR files)

## Local Windows Build via CMake & Visual Studio 2022

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```
