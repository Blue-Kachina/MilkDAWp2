// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Analyzer.h"

using namespace milkdawp::core;

namespace {
constexpr double kSampleRate = 48000.0;
constexpr float kPi = 3.14159265358979323846f;

std::vector<float> sineHop(float freqHz, double sampleRate, std::size_t numSamples, std::size_t& phaseSamples) {
  std::vector<float> hop(numSamples);
  for (std::size_t i = 0; i < numSamples; ++i) {
    const double t = static_cast<double>(phaseSamples + i) / sampleRate;
    hop[i] = std::sin(2.0f * kPi * freqHz * static_cast<float>(t));
  }
  phaseSamples += numSamples;
  return hop;
}
} // namespace

TEST_CASE("Analyzer reports near-silence for a silent input", "[core][Analyzer]") {
  Analyzer analyzer(kSampleRate);
  std::vector<float> silence(Analyzer::hopSize, 0.0f);

  AnalysisFrame frame;
  for (int i = 0; i < 8; ++i) {
    frame = analyzer.processHop(silence.data());
  }

  CHECK(frame.broadbandRms < 1e-6f);
  CHECK(frame.bassEnergy < 1e-6f);
  CHECK(frame.midEnergy < 1e-6f);
  CHECK(frame.highEnergy < 1e-6f);
  CHECK(frame.onsetStrength == 0.0f);
}

TEST_CASE("Analyzer routes a low-frequency tone mostly into the bass band", "[core][Analyzer]") {
  Analyzer analyzer(kSampleRate);
  std::size_t phase = 0;

  AnalysisFrame frame;
  for (int i = 0; i < 40; ++i) { // let the envelope settle
    auto hop = sineHop(100.0f, kSampleRate, Analyzer::hopSize, phase);
    frame = analyzer.processHop(hop.data());
  }

  CHECK(frame.bassEnergy > frame.midEnergy);
  CHECK(frame.bassEnergy > frame.highEnergy);
}

TEST_CASE("Analyzer routes a high-frequency tone mostly into the high band", "[core][Analyzer]") {
  Analyzer analyzer(kSampleRate);
  std::size_t phase = 0;

  AnalysisFrame frame;
  for (int i = 0; i < 40; ++i) {
    auto hop = sineHop(5000.0f, kSampleRate, Analyzer::hopSize, phase);
    frame = analyzer.processHop(hop.data());
  }

  CHECK(frame.highEnergy > frame.bassEnergy);
  CHECK(frame.highEnergy > frame.lowMidEnergy);
}

TEST_CASE("Analyzer's onset strength spikes when a tone begins after silence", "[core][Analyzer]") {
  Analyzer analyzer(kSampleRate);
  std::vector<float> silence(Analyzer::hopSize, 0.0f);
  std::size_t phase = 0;

  float maxSilenceOnset = 0.0f;
  for (int i = 0; i < 6; ++i) {
    auto frame = analyzer.processHop(silence.data());
    maxSilenceOnset = std::max(maxSilenceOnset, frame.onsetStrength);
  }

  float maxOnsetAfterTransient = 0.0f;
  for (int i = 0; i < 6; ++i) {
    auto hop = sineHop(440.0f, kSampleRate, Analyzer::hopSize, phase);
    auto frame = analyzer.processHop(hop.data());
    maxOnsetAfterTransient = std::max(maxOnsetAfterTransient, frame.onsetStrength);
  }

  CHECK(maxOnsetAfterTransient > maxSilenceOnset * 10.0f);
}

TEST_CASE("Analyzer's broadband RMS tracks amplitude", "[core][Analyzer]") {
  Analyzer analyzer(kSampleRate);
  std::size_t phase = 0;

  AnalysisFrame quiet;
  for (int i = 0; i < 8; ++i) {
    auto hop = sineHop(440.0f, kSampleRate, Analyzer::hopSize, phase);
    for (auto& s : hop) {
      s *= 0.1f;
    }
    quiet = analyzer.processHop(hop.data());
  }

  Analyzer analyzer2(kSampleRate);
  phase = 0;
  AnalysisFrame loud;
  for (int i = 0; i < 8; ++i) {
    auto hop = sineHop(440.0f, kSampleRate, Analyzer::hopSize, phase);
    for (auto& s : hop) {
      s *= 0.9f;
    }
    loud = analyzer2.processHop(hop.data());
  }

  CHECK(loud.broadbandRms > quiet.broadbandRms);
}
