// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/core/Analyzer.h"

#include <algorithm>
#include <cmath>

#include "milkdawp/core/Fft.h"

namespace milkdawp::core {

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Band edges in Hz (§4.3: "bass / low-mid / mid / high"). Not specified
// precisely in development_roadmap.md beyond the names; these are reasonable
// defaults for kick/bass-aligned cuts and can be tuned against the Phase 1.9
// fixture set without changing the Analyzer's interface.
constexpr float kBassLoHz = 20.0f;
constexpr float kBassHiHz = 160.0f;
constexpr float kLowMidHiHz = 500.0f;
constexpr float kMidHiHz = 2000.0f;
constexpr float kHighHiHz = 8000.0f;

// Envelope time constants for band-energy smoothing (§4.3: "attack/release
// smoothing"). Fast attack so a transient registers immediately; slower
// release so meters/energy-mode transitions don't chatter on every hop.
constexpr float kAttackSeconds = 0.010f;
constexpr float kReleaseSeconds = 0.200f;

float oneLPoleCoeff(float timeConstantSeconds, float hopDurationSeconds) {
  if (timeConstantSeconds <= 0.0f) {
    return 0.0f;
  }
  return std::exp(-hopDurationSeconds / timeConstantSeconds);
}
} // namespace

Analyzer::Analyzer(double sampleRate)
    : sampleRate_(sampleRate), hannWindow_(fftSize), history_(fftSize, 0.0f) {
  for (std::size_t n = 0; n < fftSize; ++n) {
    hannWindow_[n] =
        0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(n) / static_cast<float>(fftSize - 1)));
  }
}

void Analyzer::slideWindowIn(const float* monoHop) {
  std::copy(history_.begin() + static_cast<std::ptrdiff_t>(hopSize), history_.end(), history_.begin());
  std::copy(monoHop, monoHop + hopSize, history_.end() - static_cast<std::ptrdiff_t>(hopSize));
}

std::vector<float> Analyzer::computeMagnitudeSpectrum() const {
  std::vector<float> windowed(fftSize);
  for (std::size_t n = 0; n < fftSize; ++n) {
    windowed[n] = history_[n] * hannWindow_[n];
  }
  auto spectrum = fftReal(windowed);

  std::vector<float> magnitude(numBins);
  for (std::size_t k = 0; k < numBins; ++k) {
    magnitude[k] = std::abs(spectrum[k]);
  }
  return magnitude;
}

float Analyzer::bandEnergy(const std::vector<float>& magnitude, double sampleRate, float loHz, float hiHz) {
  const double binWidth = sampleRate / static_cast<double>(fftSize);
  auto toBin = [&](float hz) -> std::size_t {
    const auto bin = static_cast<std::ptrdiff_t>(std::lround(static_cast<double>(hz) / binWidth));
    return static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(bin, 0, static_cast<std::ptrdiff_t>(numBins - 1)));
  };

  const std::size_t binLo = toBin(loHz);
  const std::size_t binHi = toBin(hiHz);
  if (binHi < binLo) {
    return 0.0f;
  }

  float energy = 0.0f;
  for (std::size_t k = binLo; k <= binHi; ++k) {
    energy += magnitude[k] * magnitude[k];
  }
  return energy;
}

float Analyzer::smoothEnvelope(float previous, float target, float attackCoeff, float releaseCoeff) {
  const float coeff = (target > previous) ? attackCoeff : releaseCoeff;
  return coeff * previous + (1.0f - coeff) * target;
}

AnalysisFrame Analyzer::processHop(const float* monoHop) {
  slideWindowIn(monoHop);

  const auto magnitude = computeMagnitudeSpectrum();

  const float hopDurationSeconds = static_cast<float>(hopSize) / static_cast<float>(sampleRate_);
  const float attackCoeff = oneLPoleCoeff(kAttackSeconds, hopDurationSeconds);
  const float releaseCoeff = oneLPoleCoeff(kReleaseSeconds, hopDurationSeconds);

  const float bassRaw = bandEnergy(magnitude, sampleRate_, kBassLoHz, kBassHiHz);
  const float lowMidRaw = bandEnergy(magnitude, sampleRate_, kBassHiHz, kLowMidHiHz);
  const float midRaw = bandEnergy(magnitude, sampleRate_, kLowMidHiHz, kMidHiHz);
  const float highRaw = bandEnergy(magnitude, sampleRate_, kMidHiHz, kHighHiHz);

  bassEnvelope_ = smoothEnvelope(bassEnvelope_, bassRaw, attackCoeff, releaseCoeff);
  lowMidEnvelope_ = smoothEnvelope(lowMidEnvelope_, lowMidRaw, attackCoeff, releaseCoeff);
  midEnvelope_ = smoothEnvelope(midEnvelope_, midRaw, attackCoeff, releaseCoeff);
  highEnvelope_ = smoothEnvelope(highEnvelope_, highRaw, attackCoeff, releaseCoeff);

  double sumSquares = 0.0;
  for (std::size_t i = 0; i < hopSize; ++i) {
    sumSquares += static_cast<double>(monoHop[i]) * static_cast<double>(monoHop[i]);
  }
  const float rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(hopSize)));

  float broadbandFlux = 0.0f;
  float bassFlux = 0.0f;
  if (havePrevMagnitude_) {
    const double binWidth = sampleRate_ / static_cast<double>(fftSize);
    const auto bassBinHi = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(
        static_cast<std::ptrdiff_t>(std::lround(kBassHiHz / binWidth)), 0,
        static_cast<std::ptrdiff_t>(numBins - 1)));

    for (std::size_t k = 0; k < numBins; ++k) {
      const float logMag = std::log1p(magnitude[k]);
      const float prevLogMag = std::log1p(prevMagnitude_[k]);
      const float diff = std::max(0.0f, logMag - prevLogMag);
      broadbandFlux += diff;
      if (k <= bassBinHi) {
        bassFlux += diff;
      }
    }
  }

  prevMagnitude_ = magnitude;
  havePrevMagnitude_ = true;

  AnalysisFrame frame;
  frame.hopIndex = hopIndex_++;
  frame.bassEnergy = bassEnvelope_;
  frame.lowMidEnergy = lowMidEnvelope_;
  frame.midEnergy = midEnvelope_;
  frame.highEnergy = highEnvelope_;
  frame.broadbandRms = rms;
  frame.onsetStrength = broadbandFlux;
  frame.bassOnsetStrength = bassFlux;
  return frame;
}

} // namespace milkdawp::core
