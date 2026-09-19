// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Scoring.h"

using Catch::Approx;
using namespace milkdawp::core;

TEST_CASE("computeBeatFMeasure is 1.0 for an exact match", "[core][Scoring]") {
  std::vector<double> beats{0.5, 1.0, 1.5, 2.0};
  auto result = computeBeatFMeasure(beats, beats);
  CHECK(result.precision == Approx(1.0));
  CHECK(result.recall == Approx(1.0));
  CHECK(result.fMeasure == Approx(1.0));
}

TEST_CASE("computeBeatFMeasure is 0.0 when nothing is within tolerance", "[core][Scoring]") {
  std::vector<double> detected{0.5, 1.0, 1.5};
  std::vector<double> reference{10.5, 11.0, 11.5};
  auto result = computeBeatFMeasure(detected, reference, 0.070);
  CHECK(result.precision == 0.0);
  CHECK(result.recall == 0.0);
  CHECK(result.fMeasure == 0.0);
}

TEST_CASE("computeBeatFMeasure handles partial matches with extras and misses",
          "[core][Scoring]") {
  // Reference has 4 beats; detector found 3 of them plus one spurious extra.
  std::vector<double> reference{1.0, 2.0, 3.0, 4.0};
  std::vector<double> detected{1.01, 2.02, 5.0, 3.98}; // matches 1.0, 2.0, 4.0; misses 3.0; 5.0 is spurious

  auto result = computeBeatFMeasure(detected, reference, 0.070);
  // 3 of 4 detected matched -> precision 0.75; 3 of 4 reference matched -> recall 0.75
  CHECK(result.precision == Approx(0.75));
  CHECK(result.recall == Approx(0.75));
  CHECK(result.fMeasure == Approx(0.75));
}

TEST_CASE("computeBeatFMeasure matches just inside the tolerance boundary but not just beyond",
          "[core][Scoring]") {
  // Avoid asserting on the exact floating-point boundary (1.0 + tolerance);
  // a hair on either side of it is what actually matters here.
  std::vector<double> reference{1.0};

  auto justInside = computeBeatFMeasure({1.065}, reference, 0.070);
  CHECK(justInside.recall == Approx(1.0));

  auto justBeyond = computeBeatFMeasure({1.080}, reference, 0.070);
  CHECK(justBeyond.recall == 0.0);
}

TEST_CASE("computeBeatFMeasure with no detections at all is 0", "[core][Scoring]") {
  auto result = computeBeatFMeasure({}, {1.0, 2.0});
  CHECK(result.precision == 0.0);
  CHECK(result.recall == 0.0);
  CHECK(result.fMeasure == 0.0);
}

TEST_CASE("computeOctaveTolerantTempoError is ~0 for an exact match", "[core][Scoring]") {
  CHECK(computeOctaveTolerantTempoError(120.0, 120.0) == Approx(0.0));
}

TEST_CASE("computeOctaveTolerantTempoError is ~0 for double or half tempo", "[core][Scoring]") {
  CHECK(computeOctaveTolerantTempoError(240.0, 120.0) == Approx(0.0));
  CHECK(computeOctaveTolerantTempoError(60.0, 120.0) == Approx(0.0));
}

TEST_CASE("computeOctaveTolerantTempoError is large for an unrelated tempo", "[core][Scoring]") {
  // 180 bpm vs a 120 bpm reference is neither the same, double, nor half.
  CHECK(computeOctaveTolerantTempoError(180.0, 120.0) > 0.2);
}
