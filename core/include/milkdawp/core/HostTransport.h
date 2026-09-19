// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>

#include "milkdawp/core/BeatClock.h"

namespace milkdawp::core {

/// Host transport snapshot (Phase 1.7, §4.3 "Host transport override").
/// Mirrors the fields MilkDAWp actually needs from JUCE's `AudioPlayHead`
/// (plugin/ maps that JUCE type onto this plain struct at the boundary, so
/// milkdawp_core stays JUCE-free per §4.1).
struct TransportInfo {
  bool isPlaying = false;
  double bpm = 0.0;
  double ppqPosition = 0.0; // quarter notes since session start (1 ppq == 1 beat)
  int timeSigNumerator = 4;
  std::uint64_t samplePos = 0; // host's absolute sample position
};

/// Derives a BeatClockState directly from host transport (§4.3: "when
/// AudioPlayHead reports isPlaying, a valid bpm, and ppqPosition, the
/// BeatClock is derived from the host with confidence 1.0"). Unlike
/// BeatClock, this holds no persistent phase state: every call recomputes
/// beat/bar/nextBeatSample fresh from the host's own ppqPosition, so a stop,
/// loop, or relocate is handled for free -- there's no stale prediction left
/// over to correct, because nothing is predicted; the host already knows.
class HostTransport {
public:
  explicit HostTransport(double sampleRate);

  BeatClockState processTransport(const TransportInfo& info) const;

private:
  double sampleRate_;
};

} // namespace milkdawp::core
