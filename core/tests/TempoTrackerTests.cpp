// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/TempoTracker.h"

using namespace milkdawp::core;

namespace {
constexpr double kSampleRate = 48000.0;
constexpr std::size_t kHopSize = 512;
constexpr double kHopDuration = static_cast<double>(kHopSize) / kSampleRate;

/// A synthetic ODF click train: an impulse every `periodHops` hops, zero
/// elsewhere, for `totalHops` hops in total.
std::vector<float> clickTrain(std::size_t periodHops, std::size_t totalHops) {
  std::vector<float> odf(totalHops, 0.0f);
  for (std::size_t i = 0; i < totalHops; i += periodHops) {
    odf[i] = 5.0f;
  }
  return odf;
}

std::size_t hopsPerSecond() { return static_cast<std::size_t>(1.0 / kHopDuration); }

TempoEstimate runToConvergence(TempoTracker& tracker, const std::vector<float>& odf) {
  TempoEstimate estimate;
  for (float v : odf) {
    estimate = tracker.processHop(v);
  }
  return estimate;
}
} // namespace

TEST_CASE("TempoTracker locks onto a steady 120 BPM click train", "[core][TempoTracker]") {
  TempoTracker tracker(kSampleRate, kHopSize);

  const std::size_t periodHops = static_cast<std::size_t>(std::lround(60.0 / 120.0 / kHopDuration));
  const std::size_t totalHops = hopsPerSecond() * 10; // 10 seconds
  auto odf = clickTrain(periodHops, totalHops);

  auto estimate = runToConvergence(tracker, odf);

  CHECK(estimate.bpm == Catch::Approx(120.0f).margin(5.0f));
  CHECK(estimate.confidence > 0.3f);
}

TEST_CASE("TempoTracker locks onto a steady 100 BPM click train", "[core][TempoTracker]") {
  TempoTracker tracker(kSampleRate, kHopSize);

  const std::size_t periodHops = static_cast<std::size_t>(std::lround(60.0 / 100.0 / kHopDuration));
  const std::size_t totalHops = hopsPerSecond() * 10;
  auto odf = clickTrain(periodHops, totalHops);

  auto estimate = runToConvergence(tracker, odf);

  CHECK(estimate.bpm == Catch::Approx(100.0f).margin(5.0f));
  CHECK(estimate.confidence > 0.3f);
}

TEST_CASE("TempoTracker reports low confidence for silence", "[core][TempoTracker]") {
  TempoTracker tracker(kSampleRate, kHopSize);
  std::vector<float> silence(hopsPerSecond() * 10, 0.0f);

  auto estimate = runToConvergence(tracker, silence);
  CHECK(estimate.confidence < 0.1f);
}

TEST_CASE("TempoTracker's bpm does not flicker hop to hop on a steady click train",
          "[core][TempoTracker]") {
  TempoTracker tracker(kSampleRate, kHopSize);
  const std::size_t periodHops = static_cast<std::size_t>(std::lround(60.0 / 128.0 / kHopDuration));
  const std::size_t totalHops = hopsPerSecond() * 12;
  auto odf = clickTrain(periodHops, totalHops);

  // Let it converge over the first 8 seconds, then check stability over the
  // last 2 seconds -- consecutive hop-to-hop bpm readings should be close.
  const std::size_t warmupHops = hopsPerSecond() * 8;
  float previousBpm = 0.0f;
  bool first = true;
  float maxHopToHopJump = 0.0f;

  for (std::size_t i = 0; i < odf.size(); ++i) {
    auto estimate = tracker.processHop(odf[i]);
    if (i >= warmupHops) {
      if (!first) {
        maxHopToHopJump = std::max(maxHopToHopJump, std::abs(estimate.bpm - previousBpm));
      }
      previousBpm = estimate.bpm;
      first = false;
    }
  }

  CHECK(maxHopToHopJump < 2.0f);
}
