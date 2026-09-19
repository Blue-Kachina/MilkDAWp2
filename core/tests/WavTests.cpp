// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Wav.h"

using Catch::Approx;
using namespace milkdawp::core;

namespace {
class TempWavFile {
public:
  TempWavFile()
      : path_((std::filesystem::temp_directory_path() /
                ("milkdawp_wav_test_" + std::to_string(std::random_device{}()) + ".wav"))
                   .string()) {}
  ~TempWavFile() { std::filesystem::remove(path_); }
  [[nodiscard]] const std::string& path() const { return path_; }

private:
  std::string path_;
};
} // namespace

TEST_CASE("WAV round-trips a mono sine wave through 16-bit PCM", "[core][Wav]") {
  TempWavFile temp;

  WavAudio original;
  original.numChannels = 1;
  original.sampleRate = 48000.0;
  original.interleavedSamples.resize(480);
  for (std::size_t i = 0; i < original.interleavedSamples.size(); ++i) {
    original.interleavedSamples[i] =
        0.5f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 48000.0f);
  }

  writeWavFile(temp.path(), original);
  auto readBack = readWavFile(temp.path());

  CHECK(readBack.numChannels == original.numChannels);
  CHECK(readBack.sampleRate == original.sampleRate);
  REQUIRE(readBack.interleavedSamples.size() == original.interleavedSamples.size());
  for (std::size_t i = 0; i < original.interleavedSamples.size(); ++i) {
    // 16-bit PCM quantization: within ~1/32768 of the original.
    CHECK(readBack.interleavedSamples[i] == Approx(original.interleavedSamples[i]).margin(0.001));
  }
}

TEST_CASE("WAV round-trips interleaved stereo", "[core][Wav]") {
  TempWavFile temp;

  WavAudio original;
  original.numChannels = 2;
  original.sampleRate = 44100.0;
  original.interleavedSamples = {0.5f, -0.5f, 0.25f, -0.25f, 0.0f, 0.0f};

  writeWavFile(temp.path(), original);
  auto readBack = readWavFile(temp.path());

  CHECK(readBack.numChannels == 2);
  CHECK(readBack.sampleRate == 44100.0);
  REQUIRE(readBack.interleavedSamples.size() == original.interleavedSamples.size());
  for (std::size_t i = 0; i < original.interleavedSamples.size(); ++i) {
    CHECK(readBack.interleavedSamples[i] == Approx(original.interleavedSamples[i]).margin(0.001));
  }
}

TEST_CASE("WAV clamps out-of-range samples on write instead of wrapping", "[core][Wav]") {
  TempWavFile temp;
  WavAudio original;
  original.numChannels = 1;
  original.sampleRate = 48000.0;
  original.interleavedSamples = {2.0f, -2.0f}; // out of [-1, 1]

  writeWavFile(temp.path(), original);
  auto readBack = readWavFile(temp.path());

  CHECK(readBack.interleavedSamples[0] == Approx(1.0f).margin(0.001));
  CHECK(readBack.interleavedSamples[1] == Approx(-1.0f).margin(0.001));
}

TEST_CASE("readWavFile throws on a nonexistent file", "[core][Wav]") {
  CHECK_THROWS_AS(readWavFile("Z:/definitely/not/a/real/file.wav"), std::runtime_error);
}

TEST_CASE("readWavFile throws on a file that isn't RIFF/WAVE", "[core][Wav]") {
  TempWavFile temp;
  {
    std::ofstream notWav(temp.path(), std::ios::binary);
    notWav << "this is not a wav file, but it is long enough to pass the size check";
  }
  CHECK_THROWS_AS(readWavFile(temp.path()), std::runtime_error);
}
