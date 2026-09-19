// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/core/Resample.h"

#include <algorithm>
#include <cmath>

namespace milkdawp::core {

std::vector<float> resampleLinear(const std::vector<float>& input, double inputRate, double outputRate) {
  if (input.empty() || inputRate <= 0.0 || outputRate <= 0.0) {
    return {};
  }
  if (inputRate == outputRate) {
    return input;
  }

  const double ratio = inputRate / outputRate;
  const auto outputLength =
      static_cast<std::size_t>(std::floor(static_cast<double>(input.size()) / ratio));

  std::vector<float> output(outputLength);
  for (std::size_t i = 0; i < outputLength; ++i) {
    const double srcPos = static_cast<double>(i) * ratio;
    const auto srcIndex = static_cast<std::size_t>(srcPos);
    const double frac = srcPos - static_cast<double>(srcIndex);

    const float a = input[srcIndex];
    const float b = (srcIndex + 1 < input.size()) ? input[srcIndex + 1] : input[srcIndex];
    output[i] = static_cast<float>((1.0 - frac) * a + frac * b);
  }
  return output;
}

} // namespace milkdawp::core
