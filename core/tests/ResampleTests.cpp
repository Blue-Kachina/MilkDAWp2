// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Resample.h"

using Catch::Approx;
using namespace milkdawp::core;

TEST_CASE("resampleLinear is identity when rates match", "[core][Resample]") {
  std::vector<float> input{1.0f, 2.0f, 3.0f, 4.0f};
  auto out = resampleLinear(input, 48000.0, 48000.0);
  REQUIRE(out.size() == input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    CHECK(out[i] == input[i]);
  }
}

TEST_CASE("resampleLinear on empty input returns empty", "[core][Resample]") {
  CHECK(resampleLinear({}, 44100.0, 48000.0).empty());
}

TEST_CASE("resampleLinear downsampling halves the sample count for a 2x rate drop",
          "[core][Resample]") {
  std::vector<float> input(200, 0.0f);
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<float>(i);
  }
  auto out = resampleLinear(input, 96000.0, 48000.0);
  CHECK(out.size() == Approx(100).margin(1));
  // A linear ramp resampled linearly stays a ramp with double the step.
  CHECK(out.front() == Approx(0.0f).margin(0.01));
  CHECK(out[1] == Approx(2.0f).margin(0.01));
}

TEST_CASE("resampleLinear upsampling doubles the sample count for a 2x rate increase",
          "[core][Resample]") {
  std::vector<float> input{0.0f, 10.0f, 20.0f, 30.0f};
  auto out = resampleLinear(input, 48000.0, 96000.0);
  CHECK(out.size() == Approx(8).margin(1));
  CHECK(out.front() == Approx(0.0f).margin(0.01));
  // Midpoint between input[0]=0 and input[1]=10 should be interpolated to ~5.
  CHECK(out[1] == Approx(5.0f).margin(0.5));
}
