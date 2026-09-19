// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace milkdawp::core {

/// Per-hop analysis output (Phase 1.3, §4.3 step 2-3). Feeds band-energy UI
/// meters, the energy-based transition mode, and the onset detector /
/// tempo tracker (Phase 1.4/1.5) via onsetStrength/bassOnsetStrength.
struct AnalysisFrame {
  std::uint64_t hopIndex = 0;
  float bassEnergy = 0.0f;    // smoothed band energy, roughly 20-160 Hz
  float lowMidEnergy = 0.0f;  // roughly 160-500 Hz
  float midEnergy = 0.0f;     // roughly 500-2000 Hz
  float highEnergy = 0.0f;    // roughly 2000-8000 Hz
  float broadbandRms = 0.0f;  // short-time RMS of the raw hop samples
  float onsetStrength = 0.0f;     // broadband log-flux ODF value, half-wave rectified
  float bassOnsetStrength = 0.0f; // bass-band-only ODF value, half-wave rectified (kick-aligned cuts)
};

/// STFT + band energies + spectral-flux onset detection function (§4.3).
/// Consumes fixed-size hops of mono PCM already at the internal analysis
/// rate (resample with resampleLinear() first if the source rate differs).
///
/// Not thread-safe; owned and driven entirely by the analysis thread (§4.2).
class Analyzer {
public:
  static constexpr std::size_t fftSize = 2048; // Hann window, 4x overlap at hopSize=512
  static constexpr std::size_t hopSize = 512;
  static constexpr std::size_t numBins = fftSize / 2 + 1;

  explicit Analyzer(double sampleRate);

  [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

  /// `monoHop` must point to exactly `hopSize` samples.
  AnalysisFrame processHop(const float* monoHop);

private:
  void slideWindowIn(const float* monoHop);
  [[nodiscard]] std::vector<float> computeMagnitudeSpectrum() const;
  [[nodiscard]] static float bandEnergy(const std::vector<float>& magnitude, double sampleRate,
                                         float loHz, float hiHz);
  [[nodiscard]] static float smoothEnvelope(float previous, float target, float attackCoeff,
                                             float releaseCoeff);

  double sampleRate_;
  std::vector<float> hannWindow_; // size fftSize
  std::vector<float> history_;    // last fftSize raw samples, oldest first
  std::vector<float> prevMagnitude_;
  bool havePrevMagnitude_ = false;

  float bassEnvelope_ = 0.0f;
  float lowMidEnvelope_ = 0.0f;
  float midEnvelope_ = 0.0f;
  float highEnvelope_ = 0.0f;

  std::uint64_t hopIndex_ = 0;
};

} // namespace milkdawp::core
