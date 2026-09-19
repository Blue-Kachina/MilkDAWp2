// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/OnsetDetector.h"

using namespace milkdawp::core;

namespace {
constexpr double kSampleRate = 48000.0;
constexpr std::size_t kHopSize = 512;

std::vector<Onset> runDetector(OnsetDetector& detector, const std::vector<float>& odfStream) {
  std::vector<Onset> onsets;
  for (float v : odfStream) {
    if (auto onset = detector.processHop(v)) {
      onsets.push_back(*onset);
    }
  }
  return onsets;
}
} // namespace

TEST_CASE("OnsetDetector finds nothing in silence", "[core][OnsetDetector]") {
  OnsetDetector detector(kSampleRate, kHopSize);
  std::vector<float> silence(200, 0.0f);
  CHECK(runDetector(detector, silence).empty());
}

TEST_CASE("OnsetDetector finds nothing in flat low-level noise", "[core][OnsetDetector]") {
  OnsetDetector detector(kSampleRate, kHopSize);
  std::vector<float> noise(200, 0.05f);
  // A perfectly flat signal has zero variance, so nothing should ever exceed
  // mean + k*stddev (which equals the flat value itself with zero margin).
  CHECK(runDetector(detector, noise).empty());
}

TEST_CASE("OnsetDetector fires on isolated spikes in an otherwise quiet stream",
          "[core][OnsetDetector]") {
  OnsetDetector detector(kSampleRate, kHopSize, 0.5f, 2.5f, 0.06f);

  std::vector<float> odf(300, 0.01f);
  const std::vector<std::size_t> spikePositions{50, 120, 200};
  for (auto pos : spikePositions) {
    odf[pos] = 3.0f;
  }

  auto onsets = runDetector(detector, odf);

  REQUIRE(onsets.size() == spikePositions.size());
  for (std::size_t i = 0; i < onsets.size(); ++i) {
    const std::uint64_t expectedHop = spikePositions[i];
    CHECK(onsets[i].samplePos == expectedHop * kHopSize);
    CHECK(onsets[i].strength == 3.0f);
  }
}

TEST_CASE("OnsetDetector suppresses a second spike inside the minimum inter-onset interval",
          "[core][OnsetDetector]") {
  // hopDuration = 512/48000 s ~= 10.67ms. A 60ms refractory period is ~5.6
  // hops; two spikes 2 hops apart must collapse to one onset.
  OnsetDetector detector(kSampleRate, kHopSize, 0.5f, 2.5f, 0.06f);

  std::vector<float> odf(100, 0.01f);
  odf[30] = 3.0f;
  odf[32] = 3.0f; // well inside the refractory window

  auto onsets = runDetector(detector, odf);
  REQUIRE(onsets.size() == 1);
  CHECK(onsets[0].samplePos == std::uint64_t{30} * kHopSize);
}

TEST_CASE("OnsetDetector allows two spikes once the minimum interval has passed",
          "[core][OnsetDetector]") {
  OnsetDetector detector(kSampleRate, kHopSize, 0.5f, 2.5f, 0.06f);

  std::vector<float> odf(100, 0.01f);
  odf[30] = 3.0f;
  odf[50] = 3.0f; // 20 hops later, well past a ~5.6-hop refractory period

  auto onsets = runDetector(detector, odf);
  REQUIRE(onsets.size() == 2);
}
