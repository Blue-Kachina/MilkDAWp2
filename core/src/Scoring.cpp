// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/core/Scoring.h"

#include <algorithm>
#include <cmath>

namespace milkdawp::core {

BeatFMeasureResult computeBeatFMeasure(std::vector<double> detectedSeconds,
                                        std::vector<double> referenceSeconds, double toleranceSeconds) {
  std::sort(detectedSeconds.begin(), detectedSeconds.end());
  std::sort(referenceSeconds.begin(), referenceSeconds.end());

  std::vector<bool> refMatched(referenceSeconds.size(), false);
  std::size_t matches = 0;

  for (double d : detectedSeconds) {
    double bestDist = toleranceSeconds;
    std::ptrdiff_t bestIdx = -1;
    for (std::size_t i = 0; i < referenceSeconds.size(); ++i) {
      if (refMatched[i]) {
        continue;
      }
      const double dist = std::abs(referenceSeconds[i] - d);
      if (dist <= bestDist) {
        bestDist = dist;
        bestIdx = static_cast<std::ptrdiff_t>(i);
      }
    }
    if (bestIdx >= 0) {
      refMatched[static_cast<std::size_t>(bestIdx)] = true;
      ++matches;
    }
  }

  BeatFMeasureResult result;
  result.precision =
      detectedSeconds.empty() ? 0.0 : static_cast<double>(matches) / static_cast<double>(detectedSeconds.size());
  result.recall =
      referenceSeconds.empty() ? 0.0 : static_cast<double>(matches) / static_cast<double>(referenceSeconds.size());
  result.fMeasure = (result.precision + result.recall > 0.0)
                         ? 2.0 * result.precision * result.recall / (result.precision + result.recall)
                         : 0.0;
  return result;
}

double computeOctaveTolerantTempoError(double detectedBpm, double referenceBpm) {
  if (referenceBpm <= 0.0) {
    return 1.0;
  }
  const double sameOctave = std::abs(detectedBpm - referenceBpm) / referenceBpm;
  const double doubleTime = std::abs(detectedBpm - 2.0 * referenceBpm) / referenceBpm;
  const double halfTime = std::abs(detectedBpm - referenceBpm / 2.0) / referenceBpm;
  return std::min({sameOctave, doubleTime, halfTime});
}

} // namespace milkdawp::core
