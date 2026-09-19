// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/core/Fft.h"

#include <cassert>
#include <cmath>
#include <utility>

namespace milkdawp::core {

namespace {

constexpr float kPi = 3.14159265358979323846f;

std::size_t reverseBits(std::size_t value, int bits) {
  std::size_t result = 0;
  for (int i = 0; i < bits; ++i) {
    result = (result << 1) | (value & 1U);
    value >>= 1;
  }
  return result;
}

} // namespace

void fft(std::vector<std::complex<float>>& data) {
  const std::size_t n = data.size();
  assert(isPowerOfTwo(n));
  if (n <= 1) {
    return;
  }

  int bits = 0;
  while ((std::size_t(1) << bits) < n) {
    ++bits;
  }

  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t j = reverseBits(i, bits);
    if (j > i) {
      std::swap(data[i], data[j]);
    }
  }

  for (std::size_t len = 2; len <= n; len <<= 1) {
    const float angle = -2.0f * kPi / static_cast<float>(len);
    const std::complex<float> wlen(std::cos(angle), std::sin(angle));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<float> w(1.0f, 0.0f);
      const std::size_t half = len / 2;
      for (std::size_t k = 0; k < half; ++k) {
        const std::complex<float> u = data[i + k];
        const std::complex<float> v = data[i + k + half] * w;
        data[i + k] = u + v;
        data[i + k + half] = u - v;
        w *= wlen;
      }
    }
  }
}

std::vector<std::complex<float>> fftReal(const std::vector<float>& real) {
  std::vector<std::complex<float>> data(real.size());
  for (std::size_t i = 0; i < real.size(); ++i) {
    data[i] = std::complex<float>(real[i], 0.0f);
  }
  fft(data);
  return data;
}

} // namespace milkdawp::core
