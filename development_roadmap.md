# MilkDAWp 2 — Development Roadmap

MilkDAWp 2 is a ground-up rebuild of [MilkDAWp](https://github.com/Blue-Kachina/MilkDAWp): a
JUCE + projectM music visualizer that ships as a DAW plugin (VST3, AU) **and** as a standalone
application for Windows, macOS, and Linux. This document scopes the project, records what v1
taught us, fixes the architecture, and breaks the work into phases small enough for a single
focused development session each.

Status legend used throughout: `[ ]` not started · `[~]` in progress · `[x]` done · `[-]` dropped.

---

## Table of contents

1. [Vision, goals, non-goals](#1-vision-goals-non-goals)
2. [What v1 taught us (code audit)](#2-what-v1-taught-us-code-audit)
3. [Product scope by tier](#3-product-scope-by-tier)
4. [Architecture](#4-architecture)
5. [Key decisions](#5-key-decisions)
6. [Repository layout](#6-repository-layout)
7. [Phased plan](#7-phased-plan)
8. [Testing and CI strategy](#8-testing-and-ci-strategy)
9. [Packaging, signing, distribution](#9-packaging-signing-distribution)
10. [Risks and mitigations](#10-risks-and-mitigations)
11. [Working agreements for AI-assisted development](#11-working-agreements-for-ai-assisted-development)
12. [References](#12-references)

---

## 1. Vision, goals, non-goals

### Vision

Drop MilkDAWp on a channel in your DAW, or open it as an app and point it at any audio source,
and get MilkDrop-style visuals that move **with the music**: preset changes land on bars and
drops instead of on a wall-clock timer, and the whole thing is stable enough to run a four-hour
live set or a streaming session without babysitting.

### Goals

- **One engine, two shells.** A single visualization engine drives both the plugin and the
  standalone app. Features land in both at once because the shells are thin.
- **Musical transitions.** Real onset detection and tempo tracking in our own code, with host
  transport (tempo, bars, beats) used whenever the DAW provides it. Transitions are
  beat-quantized, energy-aware, and scheduled with sample accuracy.
- **Zero audio impact.** The plugin remains a bit-exact passthrough with no added latency, no
  allocations, no locks, and no message-thread calls on the audio thread.
- **Robust rendering.** projectM lives in an engine that outlives the editor window. Opening,
  closing, popping out, or going fullscreen never resets the visual.
- **Installable by normal people.** Signed installers on all three platforms; presets bundled;
  first-run works with no configuration.
- **Testable.** The core is a plain C++ library with an offline CLI, so beat detection and
  transition logic can be iterated on against audio fixtures in CI, without a DAW or GPU.

### Non-goals (for 1.0)

- Authoring or editing `.milk` presets. We consume presets; we do not write them.
- Video/media playback, camera input, or non-projectM render backends.
- AAX (Avid signing program), iOS/Android, or web builds. The video-first UI (§4.9) is
  deliberately touch-compatible so a mobile shell is not ruled out later, but none is built.
- A general-purpose VJ mixer. Scenes, setlists, OSC, and texture sharing (Spout/Syphon/NDI)
  are post-1.0 (see §3).

---

## 2. What v1 taught us (code audit)

v1 (`Blue-Kachina/MilkDAWp`, latest tag `v0.7.5`) works and ships, and got there fast. The
following observations come from reading its source and are the concrete reasons v2 is a rebuild
rather than a refactor. Line references are to `src/PluginProcessor.cpp` at `v0.7.5` unless
noted.

### 2.1 Beat detection is not actually ours

- `produceAnalysisSnapshot()` (lines 969–1004) runs an FFT and discards the result
  ("reserved for future phases"), computes short-time energy, and maintains a moving average
  with a comment: *"for future beat detection (no output yet)"*. Nothing downstream consumes it
  except a CPU fallback gradient.
- Beat-driven transitions rely entirely on projectM's internal hard-cut detector
  (`projectm_set_hard_cut_*`, lines 3350–3383), which is a volume-delta threshold with no tempo
  model. Because projectM's own cooldown timer does not reset when we load presets externally,
  v1 adds an application-level cooldown inside the callback and sets
  `projectm_set_preset_duration(86400)` to disable projectM's timer (line 3359). These are
  workarounds stacked on a detector we do not control.
- **v2:** own the analysis (§4.3). Feed PCM to projectM for its visuals, but drive transitions
  from our onset detector, tempo tracker, and, in the plugin, the host's transport.

### 2.2 Transitions have three competing clocks

Timed auto-advance is driven by a `juce::Timer` on the message thread **and** by DAW playhead
time computed on the audio thread (lines 444–496) **and** by projectM's hard-cut callback on the
GL thread. Each path calls `nextPresetInPlaylist()` via `MessageManager::callAsync`. Which one
fires first depends on scheduling, and none of them can land a transition on a beat.

**v2:** one `TransitionScheduler` in the core with a single simulated-time input, emitting
sample-stamped transition requests (§4.4).

### 2.3 Real-time safety violations on the audio thread

- `processBlock` allocates a `std::vector<float>` every block (line 420).
- `processBlock` calls `juce::MessageManager::callAsync` (lines 479, 491), which allocates and
  takes a lock.
- `parameterChanged` (called from the host's automation thread or the audio thread) mutates
  playlist `juce::Array`s and calls `goToPlaylistRelative()` directly (lines 523–560) while the
  message thread reads the same arrays for the UI. This is a data race.

**v2:** the audio callback only writes to lock-free rings. Everything else happens on the
engine thread. Enforced with a real-time sanitizer build in CI (§8).

### 2.4 Two render paths, one of them fake

- `VisualizationThread.h` contains a stub `ProjectMContext` (line 27) that renders animated
  gradients into a `juce::Image`. `SharedAssetCache` caches metadata for that stub, and
  `AdaptiveQualityController` scales the stub's back buffer. Six of seven unit tests exercise
  this path.
- The real projectM instance lives in `VizOpenGLCanvas`, a component inside the **editor**
  (line 3021). It is created lazily on the GL thread and destroyed in
  `openGLContextClosing()` (line 3563).

Consequences: closing the editor kills the visualization; the pop-out and dock operations
reparent the canvas component (lines 2768–2830), which forces JUCE to recreate the GL context,
which destroys and recreates projectM, so every pop-out resets the visual; adaptive quality has
no effect on what the user actually sees.

**v2:** projectM is owned by an engine bound to the processor or app lifetime and renders into
an FBO. Windows are just presentation surfaces (§4.5).

### 2.5 Preset loading blocks the render thread

`projectm_load_preset_file` is called on the GL thread between frames (line 3408). Preset
compilation can take tens to hundreds of milliseconds, so every transition is a visible hitch.
**v2:** measure it, prefetch file contents, pre-validate presets off-thread, and time the load
so the blend midpoint lands on the beat (§4.4).

### 2.6 Three mechanisms for loading one library

projectM symbols are resolved at runtime via `GetProcAddress` on Windows and `dlsym` on POSIX
(two near-identical blocks, lines 3178–3290), *and* the Windows build uses `/DELAYLOAD`
(`CMakeLists.txt` lines 385–397), *and* macOS/Linux set rpaths. The runtime resolution exists so
hosts can scan the plugin even when the DLL is missing, which is a real requirement.
**v2:** one `ProjectMLibrary` wrapper with one loading strategy per platform, and a
"visualization unavailable" state that the UI explains instead of silently falling back.

### 2.7 Structure

`PluginProcessor.cpp` is 3,696 lines and contains the processor, the editor, the look-and-feel,
the external window, the settings panel, and the GL canvas. `PluginEditor.cpp` is a 36-line
stub. Parameter access on the GL thread goes through string-keyed lookups every frame. Several
`static bool` once-flags live inside member functions.

### 2.8 CI covers the stubs

Linux and macOS CI build tests only (plugin off); only Windows builds the plugin. No test
touches the real render path, transitions, or state migration against real presets. No plugin
validator runs.

### 2.9 Worth keeping

- vcpkg manifest mode with pinned baseline, custom dynamic triplets, and the LGPL shared-linkage
  check in CMake.
- `CMakePresets.json` layout and the tag-triggered release workflow, including the macOS
  `install_name_tool` fix-up loop.
- The parameter set itself (beat sensitivity, transition duration + jitter, shuffle, lock,
  preset index, trigger next/prev, hard-cut controls, blend time, quality override) is a good
  1.0 surface and is the basis for state migration.
- Editor-size persistence and the Cubase-specific ordering lesson (host may create the editor
  before `setStateInformation`).
- The DAW-playhead idea: pausing and resetting the transition clock when transport stops or
  loops is correct behaviour and carries forward.
- Embedding SVG icons and the logo as binary data.
- The OBS-friendly external window: fixed title, transparency, hover overlay, borderless
  fullscreen on a chosen display.

---

## 3. Product scope by tier

| Capability | MVP (0.x) | 1.0 | Post-1.0 |
|---|---|---|---|
| VST3 plugin (Win/macOS/Linux) | ✔ | ✔ | |
| AU plugin (macOS) | | ✔ | |
| CLAP / LV2 | | | ✔ |
| Standalone app (Win/macOS/Linux) | ✔ (JUCE Standalone wrapper) | ✔ (full app shell) | |
| Audio input: device / interface | ✔ | ✔ | |
| Audio input: system loopback | documented virtual-cable workaround | Windows + macOS native | Linux native |
| Own onset + tempo tracking | ✔ | ✔ | |
| Host transport sync (tempo, bars) | ✔ | ✔ | |
| Transition modes: timed, beat-quantized, manual | ✔ | ✔ | |
| Transition mode: energy / section-change | | ✔ | |
| Playlist: folder scan, shuffle-no-repeat, lock, prev/next, index automation | ✔ | ✔ | |
| Preset library: tags, ratings, favourites, weighted shuffle | | ✔ | |
| Video-first window with control drawer (§4.9) | ✔ | ✔ | |
| Output window: fullscreen on any display, primary window keeps live mirror | ✔ | ✔ | |
| Detached controls window | | ✔ | |
| Engine survives editor close / output window open-close | ✔ | ✔ | |
| Adaptive quality (FBO resolution scaling that affects real output) | | ✔ | |
| Host automation of all parameters | ✔ | ✔ | |
| MIDI learn (standalone) | | ✔ | |
| State migration from v1 sessions | ✔ | ✔ | |
| Bundled preset pack | | ✔ | |
| Signed, notarized installers | | ✔ | |
| Texture sharing output (Spout / Syphon / NDI) | | | ✔ |
| Scenes and snapshot morphing | | | ✔ |
| Setlists and cues | | | ✔ |
| OSC / web remote | | | ✔ |
| Out-of-process renderer ("Link mode") | | | ✔ |

---

## 4. Architecture

### 4.1 Layers

```
┌──────────────────────────────┐   ┌──────────────────────────────┐
│  milkdawp_plugin             │   │  milkdawp_app                │
│  JUCE AudioProcessor/Editor  │   │  JUCE GUI app shell          │
│  VST3 · AU · (Standalone)    │   │  device/loopback input,      │
│  host transport → BeatClock  │   │  MIDI learn, preferences     │
└──────────────┬───────────────┘   └──────────────┬───────────────┘
               │            milkdawp_ui (shared JUCE widgets,       │
               │            look-and-feel, output windows)          │
┌──────────────┴──────────────────────────────────┴───────────────┐
│  milkdawp_engine  (JUCE-dependent)                              │
│  RenderEngine · ProjectMLibrary · OutputSurface · PresetLoader  │
└──────────────────────────────┬──────────────────────────────────┘
┌──────────────────────────────┴──────────────────────────────────┐
│  milkdawp_core  (no JUCE, no GL; std C++20 only)                │
│  AudioRing · Analyzer (FFT, bands, onsets) · TempoTracker ·     │
│  BeatClock · TransitionScheduler · Playlist · PresetLibrary ·   │
│  ParameterModel · StateSchema/Migration · Messages (POD)        │
└─────────────────────────────────────────────────────────────────┘
```

Rules:

- `milkdawp_core` has **no** JUCE or OpenGL dependency. It compiles on Linux CI without a
  display and is exercised by `mdw-analyze`, an offline CLI (§7, Phase 1). This is what makes
  beat detection and transitions testable by an agent in a loop.
- `milkdawp_engine` is the only place that touches projectM and GL.
- Shells never talk to projectM. They post messages to the engine and read status snapshots.

### 4.2 Threading model

| Thread | Owner | Does | Never does |
|---|---|---|---|
| Audio (host callback / device callback) | shell | copies PCM into `AudioRing`, writes transport info into an atomic snapshot | allocate, lock, log, call message thread |
| Analysis | core, driven by engine | pulls from `AudioRing` in hops (512 samples), runs FFT/onsets/tempo, updates `BeatClock`, ticks `TransitionScheduler` | touch GL or UI |
| Render (GL) | engine | feeds PCM to projectM, executes due `TransitionRequest`s, renders to FBO, presents to surfaces | parse presets, block on I/O |
| Preset I/O | engine | scans folders, reads and pre-validates preset files, warms the next preset | touch GL |
| Message (UI) | shell | widgets, parameter attachments, status polling via a lock-free snapshot | block waiting on any other thread |

Communication is via SPSC/MPSC lock-free queues carrying plain-old-data messages
(`core/Messages.h`). Strings cross threads only as interned preset IDs, never as `juce::String`.

### 4.3 Audio analysis (core)

Inputs: mono mix of the ring at the source sample rate, resampled to a fixed internal rate
(44.1/48 kHz passthrough; others resampled) so tuning constants are stable.

Pipeline per 512-sample hop (≈10.7 ms at 48 kHz):

1. **STFT** with a 2048-point Hann window (4× overlap).
2. **Band energies**: bass / low-mid / mid / high with attack/release smoothing, plus a
   broadband RMS. These feed UI meters and the energy-based transition mode.
3. **Onset detection function (ODF)**: log-compressed magnitude spectral flux, half-wave
   rectified, optionally per band (bass-only ODF is what we want for kick-aligned cuts).
4. **Peak picking**: adaptive threshold (`mean + k·std` over a sliding ±0.5 s window), minimum
   inter-onset interval 60 ms, emits `Onset{samplePos, strength, band}`.
5. **Tempo estimation**: autocorrelation (or comb-filter bank) of the ODF over a rolling 6–8 s
   window restricted to 60–200 BPM, with octave-error weighting toward 90–150. Emits a tempo
   hypothesis with confidence.
6. **Beat phase tracking**: a lightweight predictive tracker (comb/PLL or a small
   dynamic-programming pass over the ODF window) that predicts the next beat time and corrects
   on strong onsets. Emits `BeatClock{bpm, nextBeatSample, beatIndex, barIndex, confidence}`.
   Downbeat (bar) estimation is best-effort: assume 4/4, pick the phase that maximizes bass
   onset energy.

**Host transport override (plugin only):** when `AudioPlayHead` reports `isPlaying`, a valid
`bpm`, and `ppqPosition`, the `BeatClock` is derived from the host with confidence 1.0 and the
detector runs only for meters and energy. Time signature comes from the host when present.

Acceptance metrics (Phase 1) use an annotated fixture set: beat F-measure ≥ 0.85 within ±70 ms
on steady electronic material, tempo within 2% or an exact octave, and no false onsets during
30 s of silence or pink noise.

### 4.4 Transition scheduling (core)

`TransitionScheduler` is a pure state machine ticked by the analysis thread with the current
`BeatClock`, band energies, transport state, playlist, and parameters. It outputs
`TransitionRequest{presetId, cutStyle, blendSeconds, dueAtSample}`.

Modes (a parameter, automatable):

| Mode | Behaviour |
|---|---|
| Manual / Locked | never auto-advances |
| Timed | v1 behaviour: duration with optional jitter min/max; clock pauses when transport stops and resets on loop/relocate |
| Beat-quantized | advance every N bars (1, 2, 4, 8, 16), due at the predicted downbeat; falls back to Timed when beat confidence is below threshold for more than 4 s |
| Hybrid | Timed target, snapped forward to the next bar boundary |
| Energy | hard cut on a strong bass onset when broadband energy exceeds a rolling percentile (a "drop"), with a cooldown expressed in bars; otherwise behaves like Beat-quantized |

Cut style is Hard or Soft with a blend time in seconds or beats. Preset selection is a pluggable
policy: Sequential, Shuffle with a no-repeat history window, or Weighted (ratings/tags, 1.0).

Landing on the beat: the render thread knows the sample position of the frame it is about to
present (from the ring's write cursor and the audio clock). A request is executed on the first
frame whose span includes `dueAtSample`. For Soft cuts the request is issued `blend/2` early so
the perceptual midpoint sits on the beat. Preset file contents are prefetched by the Preset I/O
thread as soon as the *next* preset is chosen, which happens one transition ahead.

### 4.5 Rendering engine

- `ProjectMLibrary`: RAII wrapper around the projectM 4 C API. Loads the shared library at
  runtime on all platforms with one code path, exposes typed functions, and reports a clear
  `Unavailable{reason}` state. Uses `projectm_opengl_render_frame_fbo` so we control the target.
- `RenderEngine`: owns exactly one GL context and one projectM instance for the lifetime of
  the processor or app. Renders into an FBO at the *output* resolution (the largest attached
  surface, times the adaptive-quality scale), then presents to each `OutputSurface`.
- `OutputSurface`: the embedded primary-window surface (editor or app main window) or an
  owned `OutputWindow` that can go fullscreen on a chosen display (§4.9). Surfaces attach and
  detach without affecting the engine. Presentation uses a shared GL context where the platform
  allows (`OpenGLContext::setNativeSharedContext`) and falls back to a low-rate PBO readback +
  CPU blit for the primary-window mirror. Phase 2 contains a spike to settle this per platform.
- Context ownership when no surface is visible (plugin editor closed, no Output window): the
  engine keeps its **logical** state (current preset, playlist position, scheduler) and pauses GPU
  work. Rendering resumes on the next attached surface. A hidden 1×1 context-owner window to
  keep visual trails alive is a 1.0 option, not an MVP requirement.
- Adaptive quality scales the FBO, not a CPU image, and is driven by measured GPU frame time
  with hysteresis. The user sees the effect.

### 4.6 Out-of-process rendering (design constraint, not MVP work)

v1 is described as running "in a separate process/thread". It is a thread. A separate renderer
**process** would isolate hosts from GL driver issues and preset crashes, and the standalone app
could *be* that process ("Link mode"). It also makes embedding a preview inside the plugin
editor hard on macOS (cross-process view embedding is not supported).

Decision for 1.0: in-process engine thread. But the engine boundary is designed for it: all
shell↔engine traffic is POD messages plus an audio ring, so a shared-memory + pipe transport can
replace the in-process queues later without touching the shells. Post-1.0 item.

### 4.7 Standalone audio input

- MVP: `juce::AudioDeviceManager` input device selection (interfaces, mics, virtual cables
  like VB-Cable/BlackHole). Ships with a "how to capture system audio" doc per platform.
- 1.0: native system loopback capture modules behind a `SystemAudioCapture` interface:
  Windows via WASAPI loopback (`AUDCLNT_STREAMFLAGS_LOOPBACK`), macOS via Core Audio process
  taps (macOS 14.2+) with ScreenCaptureKit audio as fallback (13+), each requiring a permission
  prompt handled in the app. Linux stays on PulseAudio/PipeWire monitor sources exposed through
  the device list.

### 4.8 State and compatibility

- State is a versioned schema (`StateSchema v2`) serialized as a `ValueTree`. A migrator reads
  v1's `MilkDAWpState` tree (params child, `presetPath`, `playlistFolderPath`,
  `editorW/H`) and maps every v1 parameter ID onto its v2 equivalent. Round-trip and migration
  are unit-tested against fixtures captured from real v1 sessions.
- Preset references are stored both as absolute paths and as `{libraryRoot, relativePath,
  contentHash}` so a moved preset folder can be relinked.

### 4.9 Window model: video first, controls in a drawer

v1's editor is a control strip with the visual underneath, and the *video* is what pops out.
v2 inverts this. In both shells the primary window **is** the visualization, and the controls
live in a bottom drawer that appears over it, the way modern video players work.

```
┌──────────────────────────────────────────────┐
│                                              │
│              visualization                   │
│                                              │
│                                              │
├──────────────────────────────────────────────┤  ← drawer (hover / tap / pinned)
│ [preset ▾] ◀ ▶  🔒 🔀  [mode ▾]  ♩128  ⛶ ⚙ 📌│     over a translucent scrim
└──────────────────────────────────────────────┘
```

Concepts:

- **Primary window.** The plugin editor, or the app's main window. Always shows the visual.
- **Output window.** A window we own, opened from the drawer's ⛶ button, that can go
  borderless-fullscreen on a chosen display. It is just another `OutputSurface` (§4.5): the
  engine renders once to the FBO and presents to both. The primary window keeps showing the
  live mirror and stays the control surface. This is the OBS workflow: output on the capture
  display, controls in the DAW on the other display, with no "the video left" moment.
  Reason it is a separate window: a plugin editor is framed by the host and can never
  fullscreen itself. In the app the main window can go fullscreen directly, and the Output
  window is for the second display.
- **Detached controls (secondary).** The drawer component hosted in its own floating window,
  for the projector-plus-laptop setup. Same component, so it is cheap; not a design driver.

Drawer behaviour:

| Rule | Why |
|---|---|
| Three states: hidden, revealed, **pinned** | Automating a knob while the drawer vanishes under the cursor is miserable. Pinned is the default in the plugin editor; auto-hide is the default on the Output window and in app fullscreen. |
| Reveal on hover **or** tap; auto-hide after ~3 s of no pointer activity when not pinned | Touch has no hover, and some hosts swallow mouse-move events. |
| Translucent dark scrim behind the controls, optional blur | Knobs over a moving psychedelic field are unreadable. |
| First-run reveal: drawer starts open until the first interaction | Otherwise new users think the plugin has no controls. |
| Drawer row holds only essentials: preset combo, prev/next, lock, shuffle, transition mode, BPM/sync badge, output, settings, pin | Everything else (transition tuning, quality, playlist tools, diagnostics) lives in popovers or the settings panel. |
| Keyboard shortcuts are an app feature; every action is pointer-reachable in the plugin | Hosts intercept keys; v1 already disabled editor keyboard focus for this reason. |
| Minimum primary-window size is small (e.g. 480×270); the drawer collapses to icons | Video-first layouts shrink gracefully; control-first ones do not. |

Shared implementation: `milkdawp_ui` provides `ControlDrawer`, `DrawerScrim`, and
`OutputWindow`, and both shells compose them identically.

### 4.10 Development environment

Cross-platform plugin development cannot be fully containerized: macOS binaries need Apple's
toolchain on macOS, DAW-grade Windows binaries need MSVC, and containers have no GPU or audio
devices. What *can* be containerized is everything that does not need those: the JUCE-free
core, `mdw-analyze`, headless render tests under Mesa, lint, and docs. That is where most
iteration happens, so it gets first-class treatment. Three supported ways to work:

1. **Container (default for Linux work and for agents).** One image, defined in
   `.devcontainer/Dockerfile` (Ubuntu 24.04, GCC + Clang, CMake, Ninja, vcpkg with JUCE and
   projectM **pre-built**, Mesa llvmpipe + Xvfb, pluginval), published to GitHub Container
   Registry by CI. The same image is used by CI jobs, by VS Code / CLion devcontainers, and by
   Claude Code web sessions via a session-start hook. Identical environment everywhere, and the
   10–30 minute vcpkg build happens once when the image is published rather than per checkout.
   On a Linux host, `/dev/dri` and the display socket can be passed through so `mdw-view` runs
   with a real GPU inside the container.
2. **CI as the Windows and macOS build farm.** Every push produces VST3/AU/app artifacts and
   pluginval reports for all three platforms. "Push, wait fifteen minutes, download the
   bundle" is a legitimate day-to-day loop for a project of this size.
3. **Native, when you want it.** Idempotent bootstrap scripts install the minimum
   (`winget` for VS Build Tools + CMake + Ninja on Windows; Xcode command line tools + Homebrew
   on macOS), vcpkg stays repo-local as in v1, and `cmake --preset dev-<os>` does the rest. A
   `bootstrap --doctor` mode reports what is missing. Toolchain minimums are pinned in one
   file (`toolchain.json`) read by the scripts and CI.

Nix could give pinned toolchains on macOS and Linux without containers, but it does not cover
Windows and has a steep learning curve; not adopted.

---

## 5. Key decisions

Decisions marked **Recommended** are the plan of record unless overruled; **Open** items need a
call from Matthew before the phase that depends on them.

| # | Decision | Status | Recommendation and rationale |
|---|---|---|---|
| D1 | Plugin identity | **Open** (needed before Phase 3) | Keep v1's manufacturer code `OMda`, plugin code `Mlkw`, bundle ID `com.otitismedia.MilkDAWp`, and product name `MilkDAWp`. v2 becomes MilkDAWp 1.0; existing sessions keep loading, with state migrated (§4.8). The v1 repo is archived at release. Alternative: new codes if you want v1 and v2 installed side by side permanently. |
| D2 | Plugin formats | Recommended | VST3 + AU + Standalone wrapper for 1.0. CLAP via `clap-juce-extensions` and LV2 post-1.0. No AAX. |
| D3 | Renderer location | Recommended | In-process engine thread for 1.0, IPC-ready boundary (§4.6). |
| D4 | Dependency management | Recommended | Keep vcpkg manifest mode, pinned baseline, custom dynamic triplets (LGPL). Upgrade to the latest JUCE 8.x and projectM 4.x available in the baseline at Phase 0. |
| D5 | Language and toolchain | Recommended | C++20. MSVC 2022, Apple Clang 15+, GCC 12+ / Clang 16+. Warnings as errors on our targets. `clang-format` + `clang-tidy` config committed. |
| D6 | Core test framework | Recommended | Catch2 v3 for `milkdawp_core` and engine tests (JUCE-free core cannot use `juce::UnitTest`). `pluginval` for plugin binaries. |
| D7 | Playlist implementation | Recommended | Own `Playlist` in core rather than `libprojectM-4-playlist`, because we need ratings, tags, history windows, and sample-accurate scheduling that the projectM playlist does not offer. |
| D8 | Standalone shell | Recommended | Phase 3 uses JUCE's `Standalone` plugin format to get an app early. Phase 4 replaces it with a real `juce_add_gui_app` shell sharing `milkdawp_ui`. |
| D9 | Minimum OS | Recommended | Windows 10 21H2+, macOS 12+ (universal x86_64 + arm64), Ubuntu 22.04+ / glibc 2.35+. Loopback on macOS needs 13+/14.2+ and is feature-gated at runtime. |
| D10 | Licensing | Recommended | Project stays AGPL-3.0-or-later (JUCE AGPL path), projectM LGPL-2.1 dynamically linked, notices shipped in installers. Revisit only if a commercial JUCE licence is purchased. |
| D11 | Signing accounts | **Open** (needed before Phase 6) | Apple Developer ID (notarization) and a Windows code-signing certificate (EV or OV via Azure Trusted Signing) are prerequisites for 1.0 installers. Budget and account ownership to be confirmed. |
| D12 | Bundled presets | **Open** (needed before Phase 6) | Curate a licence-clean subset of the projectM community packs; confirm per-pack licences before bundling. |
| D13 | Window model | Decided | Video-first primary window with a hover/tap/pinned control drawer; a separately owned Output window for fullscreen on another display; detached-controls window as a secondary feature (§4.9). Replaces v1's control-strip-plus-pop-out-video layout. |
| D14 | Development environment | Decided | Single container image (devcontainer + CI + agent sessions) covering core, CLI, headless render, and lint; CI as the Windows/macOS build farm; idempotent native bootstrap with a doctor mode for those who want local builds (§4.10). No Nix. |

---

## 6. Repository layout

```
MilkDAWp2/
├── CMakeLists.txt                 # top-level: options, vcpkg, subdirectories
├── CMakePresets.json              # dev-{win,mac,linux}, ci-*, release-*
├── vcpkg.json / vcpkg-configuration.json
├── toolchain.json                 # pinned minimum tool versions, read by bootstrap + CI
├── triplets/                      # x64-windows-dynamic, arm64-osx-dynamic, ...
├── .devcontainer/                 # Dockerfile + devcontainer.json (the one image, §4.10)
├── .claude/                       # session-start hook for Claude Code web sessions
├── cmake/                         # helper modules (deps copy, sanitizers, warnings)
├── core/                          # milkdawp_core  (no JUCE)
│   ├── include/milkdawp/core/...
│   ├── src/...
│   └── tests/                     # Catch2, fixture-driven
├── engine/                        # milkdawp_engine (JUCE + GL + projectM)
│   ├── include/milkdawp/engine/...
│   ├── src/...
│   └── tests/                     # headless GL smoke tests (Mesa on CI)
├── ui/                            # milkdawp_ui: ControlDrawer, scrim, OutputWindow, LAF
├── plugin/                        # milkdawp_plugin: processor, editor, state migration
├── app/                           # milkdawp_app: standalone shell, capture modules
├── tools/
│   └── mdw-analyze/               # offline CLI: WAV in → onsets/beats/transitions out
├── resources/                     # icons, logo, bundled presets (git-lfs or fetched)
├── fixtures/                      # short audio clips + annotations for core tests
├── packaging/                     # Inno Setup / WiX, pkgbuild, AppImage recipes
├── docs/                          # ADRs, user guide, capture how-tos
├── scripts/                       # bootstrap.sh / bootstrap.ps1 (--doctor), CI helpers
└── .github/workflows/             # devcontainer-image.yml, ci.yml, pluginval.yml, release.yml
```

---

## 7. Phased plan

Sizing: **S** ≈ one focused session, **M** ≈ 2–4, **L** ≈ 5–10, **XL** must be split before
starting. Sizes are estimates for planning credits, not commitments. Each phase ends with a
short "what to test by hand" list for Matthew; agents should append to it as they go.

### Phase 0 — Foundation and guardrails

**Goal:** a repo that builds on all three platforms in CI with the target structure, before any
feature code, and a one-command developer environment. **Exit:** `cmake --preset ci-linux &&
ctest` green on Linux, macOS, Windows; sanitizer job green; empty `milkdawp_core` and
`milkdawp_engine` targets link; opening the repo in a devcontainer gives a working build in
under two minutes; a fresh Claude Code web session can build and run the core tests.

- [ ] 0.1 (S) Top-level CMake with options, warnings-as-errors module, C++20, presets for
      dev/ci/release on each platform. Port vcpkg manifest, baseline, and triplets from v1;
      bump to the latest JUCE 8.x and projectM 4.x in the baseline.
- [ ] 0.2 (S) Skeleton targets: `milkdawp_core` (static lib), `milkdawp_engine`,
      `milkdawp_ui`, `milkdawp_plugin`, `milkdawp_app`, `mdw-analyze`, Catch2 test runner.
- [ ] 0.3 (S) CI matrix (Linux, macOS, Windows): configure, build all targets, run core tests.
      vcpkg binary caching via GitHub cache to keep runs under ~15 minutes after warm-up.
- [ ] 0.4 (S) Sanitizer job on Linux: ASan + UBSan for core/engine tests, TSan for queue and
      ring tests. Clang RealtimeSanitizer (`-fsanitize=realtime`) job for functions marked
      `[[clang::nonblocking]]` (the audio callback path).
- [ ] 0.5 (S) `clang-format`, `clang-tidy`, `.editorconfig`, pre-commit hook script,
      `CONTRIBUTING.md` with the threading rules from §4.2.
- [ ] 0.6 (S) ADR directory with ADR-0001..0006 recording D1–D12 as decided so far.
- [ ] 0.7 (S) `LICENSES/`, `THIRD_PARTY_NOTICES.md`, SPDX headers template.
- [ ] 0.8 (S) Fixture policy: short (≤10 s) audio clips with permissive licences or synthesized,
      plus `fixtures/README.md` on annotation format (`beats.txt`: one beat time per line).
- [ ] 0.9 (M) Devcontainer image: `.devcontainer/Dockerfile` with GCC + Clang, CMake, Ninja,
      vcpkg and the manifest dependencies pre-built for `x64-linux-dynamic`, Mesa llvmpipe,
      Xvfb, pluginval, clang-format/tidy. `devcontainer.json` with recommended extensions and
      the CMake preset pre-selected. Optional GPU/display passthrough documented.
- [ ] 0.10 (S) `devcontainer-image.yml`: builds and publishes the image to GHCR on changes to
      the Dockerfile, `vcpkg.json`, or `vcpkg-configuration.json`; CI jobs from 0.3 run inside
      it (`container:`) so CI and local containers are identical.
- [ ] 0.11 (S) Claude Code web session-start hook (`.claude/`): pulls or reuses the image
      contents, configures the Linux preset, warms the build so agents can run tests
      immediately. Verified by opening a fresh session and running `ctest`.
- [ ] 0.12 (M) Native bootstrap: `scripts/bootstrap.ps1` (winget: VS Build Tools, CMake,
      Ninja) and `scripts/bootstrap.sh` (Xcode CLT check, Homebrew: cmake, ninja), both
      idempotent, both reading `toolchain.json`, both with `--doctor` that prints found vs
      required versions and exits non-zero on gaps.
- [ ] 0.13 (S) `CONTRIBUTING.md` "three ways to develop" section (§4.10) with the exact
      commands for each, and a note on which phases need native builds.

Hand test: open the repo in VS Code with the Dev Containers extension and confirm the build and
tests run without installing anything else on the host.

### Phase 1 — Core analysis and scheduling (JUCE-free)

**Goal:** beat detection and transition logic that can be iterated on offline. **Exit:**
`mdw-analyze` reports beat F-measure and tempo error against every fixture; scheduler
simulations are deterministic and pass; CI runs the metric suite and fails on regression.

- [ ] 1.1 (S) `AudioRing`: lock-free SPSC ring for interleaved float PCM with sample-position
      cursors; `copyLatest(n)` and `consumeHop(n)` APIs. TSan-tested.
- [ ] 1.2 (S) `Messages.h`: POD message types (parameter change, transition request, preset
      load result, status snapshot) and the SPSC/MPSC queue templates. Static-asserted trivially
      copyable.
- [ ] 1.3 (M) `Analyzer`: resampler to internal rate, STFT (own radix-2 FFT or a header-only
      dependency such as pffft via vcpkg), band energies with smoothing, spectral-flux ODF
      (broadband + bass).
- [ ] 1.4 (M) `OnsetDetector`: adaptive threshold, peak picking, min inter-onset interval.
      Unit tests on synthetic clicks, sweeps, silence, noise.
- [ ] 1.5 (M) `TempoTracker`: autocorrelation/comb tempo estimate with octave weighting and
      confidence; hysteresis so BPM does not flicker.
- [ ] 1.6 (M) `BeatClock` phase tracker: predicts next beat and bar, corrects on onsets,
      exposes confidence and `samplesUntilNextBeat()`. Downbeat heuristic for 4/4.
- [ ] 1.7 (S) `HostTransport` adapter: given `{bpm, ppq, timeSig, isPlaying, samplePos}`
      produce a `BeatClock` with confidence 1.0; handles stop, loop, and relocate.
- [ ] 1.8 (M) `mdw-analyze` CLI: WAV in, JSON/CSV out (onsets, beats, tempo curve, band
      energies), `--reference beats.txt` scoring (F-measure at ±70 ms, tempo error), and
      `--plot` to emit an SVG for eyeballing.
- [ ] 1.9 (S) Fixture set: 8–12 clips covering four-on-the-floor, syncopated, tempo change,
      breakdown/drop, sparse acoustic, silence, noise. Annotate beats; commit.
- [ ] 1.10 (S) Metric gate in CI: `mdw-analyze --suite fixtures/` must meet §4.3 thresholds;
      thresholds live in `fixtures/thresholds.json` so tuning is explicit.
- [ ] 1.11 (M) `Playlist`: folder scan (recursive, `.milk`), sequential / shuffle-no-repeat
      (history window) / weighted policies, lock, index mapping, stable ordering across
      rescans. Pure functions, unit-tested.
- [ ] 1.12 (L) `TransitionScheduler`: modes from §4.4, cut style, blend timing, transport
      pause/reset, confidence fallback, cooldown in bars. Driven by a simulated clock in tests:
      given a synthetic `BeatClock` stream, assert requests are due exactly on downbeats.
- [ ] 1.13 (S) `ParameterModel`: the canonical list of parameters (ID, range, default,
      automatable, v1 alias) shared by plugin, app, and migration. Generated docs table.
- [ ] 1.14 (M) `StateSchema` v2 + `MigrateFromV1`: read v1 `MilkDAWpState`, map params,
      paths, editor size. Fixtures captured from real v1 sessions (Matthew to provide 2–3
      `.vstpreset` or host project state blobs).

Hand test: run `mdw-analyze` on a couple of your own tracks and check the SVG: do the beat
markers sit on the kicks? Note any track where it drifts and add it as a fixture.

### Phase 2 — Rendering engine

**Goal:** projectM rendering owned by an engine, not a window. **Exit:** a headless test
renders three real presets to an FBO on Linux CI (Mesa llvmpipe) and checks the output is
non-black and changes between frames; a dev-only viewer app shows live rendering from a WAV
file with beat-aligned transitions.

- [ ] 2.1 (M) `ProjectMLibrary`: single runtime-loading path (LoadLibrary/dlopen), typed
      function table, version check, `Unavailable{reason}`. Remove `/DELAYLOAD` reliance; keep
      rpath + bundle-relative search. Windows dependency probing (GLEW etc.) folded in.
- [ ] 2.2 (M) `RenderEngine` skeleton: owns GL context + projectM instance; FBO render via
      `projectm_opengl_render_frame_fbo`; per-frame PCM feed from `AudioRing`; parameter
      application from queue (no string lookups on the render thread).
- [ ] 2.3 (L) **Spike:** presentation to multiple surfaces per platform. Try shared contexts
      (`setNativeSharedContext`) on Win/macOS/Linux; measure; fall back to PBO readback for the
      preview. Write ADR-0007 with the result and the per-platform strategy.
- [ ] 2.4 (M) `OutputSurface` implementations: embedded component (primary window) and
      `OutputWindow` (owned top-level window, borderless fullscreen on a chosen display,
      remembers its display). Attach/detach without engine restart; both surfaces show the
      same frame. Port v1's OBS niceties (fixed window title, transparency option).
- [ ] 2.5 (M) `PresetLoader` on the Preset I/O thread: read file, cheap syntax pre-validation,
      blacklist on failure (`projectm_set_preset_switch_failed_event_callback`), prefetch of the
      next preset, load-time measurement and logging.
- [ ] 2.6 (S) Execute `TransitionRequest`s on the render thread at `dueAtSample`; early-issue
      for soft cuts. Log actual vs intended landing error in samples.
- [ ] 2.7 (M) Headless render test harness: offscreen GL context on Linux (EGL surfaceless or
      Xvfb + Mesa), render N frames of fixture presets, assert non-black + frame-to-frame delta,
      run under ASan.
- [ ] 2.8 (S) Frame timing and GPU time metrics (`GL_TIMESTAMP` queries where available);
      status snapshot for the UI.
- [ ] 2.9 (M) `mdw-view` dev tool: WAV → engine → primary window with the `ControlDrawer`
      from 2.11. First place beat-aligned transitions are visible to a human. Runs inside the
      devcontainer with GPU passthrough on Linux hosts.
- [ ] 2.10 (S) Engine behaviour with zero surfaces: pause GPU work, keep logical state, resume.
      Test: attach, detach, attach again; preset and playlist position unchanged.
- [ ] 2.11 (M) `milkdawp_ui` drawer components: `ControlDrawer` (hidden / revealed / pinned
      states, hover and tap reveal, auto-hide timer, first-run reveal), `DrawerScrim`
      (translucent band, optional blur), slot layout that collapses to icons at small widths.
      Unit-testable state machine for the reveal/hide logic.

Hand test: `mdw-view` with a folder of presets and a track with a clear drop. Transitions
should land on downbeats in Beat-quantized mode; no hitch longer than one frame on most presets.

### Phase 3 — Plugin shell (VST3 / AU / Standalone wrapper)

**Goal:** a plugin at least as capable as v1 0.7.5, on the new engine. **Exit:** `pluginval`
strictness 5+ passes on all platforms in CI; the v1 → v2 migration test passes; manual checks in
Reaper, Ableton Live, FL Studio, Cubase, Logic (AU) pass the checklist below.

- [ ] 3.1 (M) `MilkDAWpProcessor`: stereo/mono passthrough, RT-safe ring writes, transport
      snapshot, `ParameterModel`-driven APVTS layout, engine lifetime bound to the processor.
      `[[clang::nonblocking]]` on `processBlock`; RTSan job covers it.
- [ ] 3.2 (S) State save/restore with schema v2 and v1 migration; editor size persistence with
      the Cubase ordering fix.
- [ ] 3.3 (M) Video-first editor (§4.9): the whole editor is an embedded `OutputSurface`
      with the `ControlDrawer` over it, pinned by default. Drawer row: preset combo, picker,
      prev/next, lock, shuffle, transition mode, BPM/sync badge, output, settings, pin. Status
      from engine snapshots, not timers polling the processor. Resizable down to 480×270.
- [ ] 3.4 (S) Transition settings popover: mode selector, bars (N), blend, energy threshold,
      jitter, with sensible defaults (Beat-quantized, 4 bars, soft 2 beats).
- [ ] 3.5 (S) Beat/tempo badge in the drawer (BPM, confidence, host-sync indicator), useful
      for trust and for debugging in the field.
- [ ] 3.12 (S) Output window from the plugin: ⛶ opens `OutputWindow` (2.4) on the remembered
      display; editor keeps the live mirror and pinned drawer; closing the editor leaves the
      output window running; removing the plugin closes it.
- [ ] 3.13 (S) Detached controls: "float controls" action hosts the drawer in a small owned
      window; docking returns it. Same component, no duplicated wiring.
- [ ] 3.6 (S) Host transport integration: `AudioPlayHead` → `HostTransport`; verify stop,
      loop, relocate behaviour in two DAWs.
- [ ] 3.7 (S) Enable the JUCE `Standalone` format to get an early app for testing (D8).
- [ ] 3.8 (M) AU target on macOS; `auval` in CI on macOS runner.
- [ ] 3.9 (M) `pluginval` job in CI for VST3 (all platforms) and AU (macOS), strictness 5,
      with the runtime dependency layout check from v1 (`check_runtime_win.ps1`) ported.
- [ ] 3.10 (S) Runtime dependency bundling per platform, ported from v1 (DLL copy, dylib
      fix-up, rpath), now for VST3, AU, and Standalone.
- [ ] 3.11 (S) DAW compatibility checklist doc (`docs/daw-checklist.md`): scan, insert,
      automate every parameter, save/reload, drawer reveal/pin in each host, output window on
      second display, close and reopen editor with output open, remove plugin.

Hand test: the DAW checklist in at least Reaper + one other host on each OS you have. Load a v1
project and confirm preset, playlist, and knob values survive. Reproduce your OBS setup: output
window fullscreen on the capture display, editor with pinned drawer on the other.

### Phase 4 — Standalone application

**Goal:** a real app people would open without a DAW. **Exit:** app launches to a working
visual within 10 seconds on a clean machine with a bundled preset and default input; all
features of the plugin editor are available; preferences persist.

- [ ] 4.1 (M) `milkdawp_app` shell with `juce_add_gui_app`: video-first main window with the
      shared `ControlDrawer` (auto-hide default in fullscreen, pinned otherwise), menu bar,
      keyboard shortcuts (F11 fullscreen, arrows prev/next, L lock, Esc reveals drawer),
      single-instance guard. Main window can fullscreen directly; ⛶ opens the `OutputWindow`
      for a second display; "float controls" for the projector-plus-laptop setup.
- [ ] 4.2 (M) Audio input: `AudioDeviceManager` device selector, input channel pair choice,
      level meter, "no signal" hint. Startup restores the last device; graceful fallback when
      it is missing.
- [ ] 4.3 (S) Preferences (`PropertiesFile`): device, preset library root, output display,
      quality, logging toggle, last window geometry.
- [ ] 4.4 (M) MIDI learn: map CC/notes to any parameter; persisted; UI affordance on each
      control.
- [ ] 4.5 (M) Preset library browser: tree of the library root, search, favourites, recently
      played, right-click add to blacklist.
- [ ] 4.6 (S) File associations and drag-and-drop for `.milk` files and folders.
- [ ] 4.7 (L) Windows WASAPI loopback capture module behind `SystemAudioCapture`, selectable as
      "System audio" in the device list.
- [ ] 4.8 (L) macOS system audio capture via Core Audio process taps (14.2+) with
      ScreenCaptureKit fallback (13+); permission flow and messaging.
- [ ] 4.9 (S) Linux: verify PipeWire/Pulse monitor sources appear; document.
- [ ] 4.10 (S) Crash reporting hooks (local minidump/log bundle) and "collect logs" menu item.
- [ ] 4.11 (S) Retire the JUCE `Standalone` wrapper format once 4.1–4.3 land (or keep it as a
      dev convenience behind a CMake option).

Hand test: fresh user account on each OS: install, launch, play music from a browser, confirm
visuals react without configuring anything (Windows/macOS via loopback, Linux via monitor
source).

### Phase 5 — Musicality, performance, and polish

**Goal:** the features that make v2 noticeably better than v1 in a live setting. **Exit:**
energy mode demonstrably cuts on drops in the fixture set; adaptive quality keeps ≥55 fps at
1080p on an integrated GPU with the bundled pack; 4-hour soak test passes with flat memory.

- [ ] 5.1 (M) Energy / section-change transition mode: rolling energy percentile, drop
      detector, build-up detection (optional), cooldown in bars.
- [ ] 5.2 (M) Weighted shuffle with ratings and tags; per-preset "never auto-select"; tag
      filters in the transition settings ("only *calm* during breakdowns" is post-1.0).
- [ ] 5.3 (M) Adaptive quality on the real FBO with GPU-time-driven hysteresis and a manual
      override; visible current-scale indicator.
- [ ] 5.4 (S) Preset load hitch mitigation: measure per-preset compile time, cache it, and
      prefer cheap presets when the scheduler needs a hard cut on the next beat.
- [ ] 5.5 (S) Beat sensitivity semantics: one knob that scales both our detector's threshold
      and `projectm_set_beat_sensitivity`, documented.
- [ ] 5.6 (M) Multi-instance behaviour in a DAW: shared library handle, per-instance engine,
      GPU budget awareness (lower FPS for instances without visible surfaces).
- [ ] 5.7 (M) Soak and stress tests: 4-hour run script for the app; rapid parameter
      automation; preset folder of 2,000 files; hot-unplugging the audio device.
- [ ] 5.8 (S) Accessibility and UX pass: keyboard navigation in the app, tooltips, high-DPI on
      all platforms, drawer scrim contrast over bright presets, touch-target sizes in the
      drawer (≥ 32 px) so a future touch shell needs no relayout.
- [ ] 5.9 (S) Diagnostics panel: GL vendor/renderer, projectM version, frame time, beat
      confidence, last errors, "copy diagnostics" button (replaces v1's Phase 9 benchmark
      idea with something cheaper and more useful).

Hand test: a DJ-style hour with mixed genres in the app with Energy mode; note every transition
that felt wrong and file it with the timestamp.

### Phase 6 — Content, packaging, release

**Goal:** 1.0. **Exit:** signed installers for all platforms published by the tag workflow;
docs live; v1 repo archived with a pointer.

- [ ] 6.1 (S) Curate and licence-check the bundled preset pack (D12); default preset chosen;
      first-run library root points at it.
- [ ] 6.2 (M) Windows installer (Inno Setup or WiX): VST3 to `Common Files\VST3`, app to
      Program Files, optional desktop shortcut, uninstaller; signed (D11).
- [ ] 6.3 (M) macOS: `.pkg` with VST3 + AU + app, Developer ID signed, notarized, stapled;
      universal binary.
- [ ] 6.4 (M) Linux: AppImage for the app, tarball for the VST3, `.deb` as stretch.
- [ ] 6.5 (S) Release workflow: tag → build matrix → sign → package → GitHub Release with
      generated notes and checksums. Pre-release channel on `-beta` tags.
- [ ] 6.6 (M) Documentation: user guide (plugin + app), capture how-tos per platform, OBS
      workflow, MIDI/automation guide, troubleshooting, FAQ on licences.
- [ ] 6.7 (S) In-app "About" with versions and licences; update check (opt-in, GitHub
      releases API).
- [ ] 6.8 (S) Beta programme: two weeks of `-beta` builds, issue template, triage.
- [ ] 6.9 (S) 1.0 release, archive v1 repo with a README pointer, announce.

### Post-1.0 backlog (unscheduled)

- Texture sharing output: Spout (Windows), Syphon (macOS), NDI (all) so OBS/Resolume can
  take the frame without screen capture.
- Scenes: snapshot all parameters, morph between snapshots over time.
- Setlists and cues with DAW markers / MIDI program changes.
- OSC server and a small web remote for phone control.
- Out-of-process renderer / "Link mode" between plugin and app (§4.6).
- CLAP and LV2 formats.
- Linux native loopback capture module.
- Offline high-resolution render to video.

---

## 8. Testing and CI strategy

| Layer | Tooling | Runs where |
|---|---|---|
| Environment | devcontainer image built and smoke-tested (configure + core tests inside it) | on changes to Dockerfile or vcpkg manifests |
| `milkdawp_core` unit tests | Catch2 v3; deterministic, fixture-driven | every push, all platforms; ASan/UBSan/TSan job on Linux, inside the devcontainer image |
| Analysis quality gate | `mdw-analyze --suite fixtures/` vs `thresholds.json` | every push, Linux |
| Scheduler simulations | Catch2 with simulated `BeatClock` streams | every push |
| Engine headless render | offscreen GL via Mesa llvmpipe, fixture presets | every push, Linux; nightly on macOS/Windows runners |
| RT safety | Clang RealtimeSanitizer on `processBlock` and ring code | every push, Linux |
| Plugin validation | `pluginval` strictness 5 (VST3 all platforms, AU macOS), `auval` | every push |
| State migration | fixtures from real v1 sessions | every push |
| Soak / stress | scripted 4-hour app run, memory sampling | nightly / pre-release |
| Manual DAW matrix | `docs/daw-checklist.md` | before each beta and release |

Coverage expectation: core ≥ 80% line coverage reported in CI; engine and shells covered by
smoke tests and validators rather than a percentage.

---

## 9. Packaging, signing, distribution

- **Windows:** signed VST3 bundle + signed app installer. Runtime DLLs (projectM, GLEW,
  freetype, png, zlib, brotli) live inside the `.vst3` bundle's binary folder and next to the
  app exe. Signing via Azure Trusted Signing or an OV/EV certificate (D11).
- **macOS:** universal `.pkg` installing `MilkDAWp.vst3`, `MilkDAWp.component`, and
  `MilkDAWp.app`; each bundle carries `Frameworks/` with fixed-up dylibs; hardened runtime,
  Developer ID signature, notarization, stapling. Loopback capture entitlements/permissions
  documented.
- **Linux:** AppImage for the app (bundles the shared libraries), `.tar.gz` for the VST3 with
  `Contents/Resources/lib` rpath layout from v1; `.deb` stretch.
- **Presets:** bundled pack installed to a shared location per platform; user library root
  defaults there but is changeable.
- **Licences:** AGPL-3.0 for MilkDAWp, LGPL-2.1 for projectM (dynamically linked, notices and
  source offer in installers), JUCE AGPL, third-party notices generated at build time.

---

## 10. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Shared GL contexts across windows behave differently per platform/host | primary window mirror + Output window design | Phase 2.3 spike before committing; PBO readback fallback for the primary-window mirror is always available |
| Hosts swallow hover or mouse-move events so the drawer never reveals | controls unreachable in that host | tap/click reveal as well as hover; pinned is the plugin default; DAW checklist (3.11) tests drawer reveal per host |
| Devcontainer image drifts from what CI runs, or grows stale against the vcpkg baseline | "works in the container, fails in CI" | CI runs *inside* the published image; image rebuild is triggered by manifest changes; image tag recorded in CI logs |
| projectM preset compile hitches on the render thread | visible stutter on transitions | measure and cache per-preset cost (5.4), prefetch, prefer cheap presets for hard cuts, consider upstream async load contribution |
| Hosts that dislike OpenGL (some macOS hosts, sandboxed AUv3 not in scope) | plugin unusable in that host | pluginval + DAW matrix early (Phase 3); engine can run with zero surfaces; out-of-process renderer is the long-term escape hatch |
| macOS system audio capture APIs require newer OS and permissions | standalone loopback on older macOS | feature-gate at runtime; document BlackHole fallback; MVP ships without native loopback |
| Beat tracking on non-electronic or rubato material | wrong-feeling transitions | confidence-gated fallback to Timed mode; host transport wins in the DAW; fixtures include hard cases so the gate is honest |
| Signing/notarization accounts and costs | blocks 1.0 installers | decide D11 early (Phase 4 at the latest); unsigned dev builds continue via CI artifacts |
| vcpkg baseline drift breaking projectM/JUCE builds | CI red for reasons unrelated to our code | pinned baseline; bump on a branch with the full matrix; binary cache |
| Scope creep from post-1.0 ideas (Spout, scenes, OSC) | 1.0 slips | tiers in §3 are the contract; new ideas go to the backlog section, not into phases |
| Preset pack licensing | cannot bundle content | D12 resolved before Phase 6; ship with a downloader as fallback |

---

## 11. Working agreements for AI-assisted development

These apply to any agent session working in this repo (and to humans, but agents forget).

**Session sizing.** Pick one checkbox. If it does not fit in a session, split it in this file
first (add sub-items), commit the split, then start. Never leave a phase's exit criteria vaguer
than you found them.

**Definition of done for a task.**
1. Code compiles warning-free on the platform you are on; CI green on all three.
2. Tests exist for the behaviour (core: unit; engine: headless smoke; shells: pluginval or the
   DAW checklist updated).
3. Threading rules in §4.2 respected; anything touching the audio callback carries
   `[[clang::nonblocking]]` and passes the RTSan job.
4. The checkbox in this file is ticked in the same commit, with a one-line note if the
   approach changed.
5. If a decision in §5 was touched, an ADR was added or amended.

**Boundaries.**
- Do not add JUCE or GL includes to `core/`. If you think you need to, write down why in the PR
  and stop.
- Do not add a third way to do something that already has two. Delete one first.
- Do not disable, skip, or loosen a test or a threshold in `fixtures/thresholds.json` to get
  green. Lowering a threshold is a decision for Matthew with the metric report attached.
- Do not change plugin identity codes, bundle IDs, or the state schema version without an ADR.
- Do not add UI that only exists in one shell. Drawer, output window, and settings are
  `milkdawp_ui` components composed by both shells.
- Work in the devcontainer unless the task needs a native toolchain (AU, MSVC-specific,
  installers, device capture). Say so in the PR when it does, so the reviewer knows to pull a
  CI artifact rather than build locally.

**When to stop and ask.**
- Any §5 item marked **Open** that the task depends on.
- A platform behaviour that contradicts an ADR (write up what you saw first).
- Anything involving accounts, certificates, or licences.
- End of each phase: post the hand-test list and what you would like checked.

**Commit hygiene.** Small commits, imperative subject, body says *why*. Reference the roadmap
item (`[1.6]`) in the subject.

---

## 12. References

- v1 source: https://github.com/Blue-Kachina/MilkDAWp (tag `v0.7.5`)
- projectM 4 C API: https://github.com/projectM-visualizer/projectm (`src/api/include/projectM-4/`)
- JUCE CMake API: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
- pluginval: https://github.com/Tracktion/pluginval
- Onset detection and beat tracking background: Bello et al., "A Tutorial on Onset Detection
  in Music Signals" (2005); Ellis, "Beat Tracking by Dynamic Programming" (2007); Böck et al.,
  "Evaluating the Online Capabilities of Onset Detection Methods" (2012).
- Clang RealtimeSanitizer: https://clang.llvm.org/docs/RealtimeSanitizer.html
- vcpkg manifest mode and versioning: https://learn.microsoft.com/vcpkg/
