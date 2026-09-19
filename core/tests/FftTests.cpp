// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cmath>
#include <complex>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Fft.h"

using Catch::Approx;
using namespace milkdawp::core;

TEST_CASE("isPowerOfTwo", "[core][Fft]") {
  CHECK(isPowerOfTwo(1));
  CHECK(isPowerOfTwo(2));
  CHECK(isPowerOfTwo(2048));
  CHECK_FALSE(isPowerOfTwo(0));
  CHECK_FALSE(isPowerOfTwo(3));
  CHECK_FALSE(isPowerOfTwo(2047));
}

TEST_CASE("fft of silence is silence", "[core][Fft]") {
  std::vector<float> silence(64, 0.0f);
  auto spectrum = fftReal(silence);
  for (const auto& bin : spectrum) {
    CHECK(std::abs(bin) == Approx(0.0f).margin(1e-6));
  }
}

TEST_CASE("fft of a DC signal has all energy in bin 0", "[core][Fft]") {
  constexpr std::size_t n = 64;
  std::vector<float> dc(n, 1.0f);
  auto spectrum = fftReal(dc);

  CHECK(std::abs(spectrum[0]) == Approx(static_cast<float>(n)).margin(1e-3));
  for (std::size_t k = 1; k < n; ++k) {
    CHECK(std::abs(spectrum[k]) == Approx(0.0f).margin(1e-3));
  }
}

TEST_CASE("fft of a pure sine at bin k peaks at bin k and its mirror", "[core][Fft]") {
  constexpr std::size_t n = 64;
  constexpr std::size_t k = 5;
  constexpr float kPi = 3.14159265358979323846f;

  std::vector<float> sine(n);
  for (std::size_t i = 0; i < n; ++i) {
    sine[i] = std::sin(2.0f * kPi * static_cast<float>(k) * static_cast<float>(i) / static_cast<float>(n));
  }

  auto spectrum = fftReal(sine);

  const float expectedPeakMag = static_cast<float>(n) / 2.0f;
  CHECK(std::abs(spectrum[k]) == Approx(expectedPeakMag).epsilon(0.01));
  CHECK(std::abs(spectrum[n - k]) == Approx(expectedPeakMag).epsilon(0.01));

  // Everywhere else should be near zero.
  for (std::size_t bin = 0; bin < n; ++bin) {
    if (bin == k || bin == n - k) {
      continue;
    }
    CHECK(std::abs(spectrum[bin]) < 1e-2f);
  }
}

TEST_CASE("fft satisfies Parseval's theorem (energy preserved up to scale by N)", "[core][Fft]") {
  constexpr std::size_t n = 128;
  constexpr float kPi = 3.14159265358979323846f;
  std::vector<float> signal(n);
  for (std::size_t i = 0; i < n; ++i) {
    signal[i] =
        std::sin(2.0f * kPi * 7.0f * static_cast<float>(i) / static_cast<float>(n)) +
        0.5f * std::cos(2.0f * kPi * 20.0f * static_cast<float>(i) / static_cast<float>(n));
  }

  double timeEnergy = 0.0;
  for (float s : signal) {
    timeEnergy += static_cast<double>(s) * static_cast<double>(s);
  }

  auto spectrum = fftReal(signal);
  double freqEnergy = 0.0;
  for (const auto& bin : spectrum) {
    freqEnergy += static_cast<double>(std::norm(bin));
  }

  CHECK(freqEnergy / static_cast<double>(n) == Approx(timeEnergy).epsilon(0.01));
}
