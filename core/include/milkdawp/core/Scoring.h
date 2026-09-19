// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <vector>

namespace milkdawp::core {

/// Beat-tracking accuracy metrics (Phase 1.8/§4.3 acceptance metrics).
struct BeatFMeasureResult {
  double precision = 0.0; // fraction of detected beats that matched a reference beat
  double recall = 0.0;    // fraction of reference beats that were matched
  double fMeasure = 0.0;  // harmonic mean of precision and recall
};

/// Greedy nearest-neighbour matching within `toleranceSeconds` between
/// detected and reference beat times (both in seconds, need not be sorted).
/// This is the standard MIR beat-tracking F-measure (§4.3: "beat F-measure
/// >= 0.85 within +/-70ms on steady electronic material").
[[nodiscard]] BeatFMeasureResult computeBeatFMeasure(std::vector<double> detectedSeconds,
                                                      std::vector<double> referenceSeconds,
                                                      double toleranceSeconds = 0.070);

/// Octave-tolerant relative tempo error: min(|d-r|, |d-2r|, |d-r/2|) / r.
/// (§4.3: "tempo within 2% or an exact octave" -- a tracker that locks onto
/// double or half the true tempo is a much smaller error than a wrong tempo
/// entirely, and shouldn't fail the gate the same way.)
[[nodiscard]] double computeOctaveTolerantTempoError(double detectedBpm, double referenceBpm);

} // namespace milkdawp::core
