// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

namespace milkdawp::core {

/// Decoded PCM audio, interleaved, normalized to [-1, 1].
struct WavAudio {
  std::vector<float> interleavedSamples;
  int numChannels = 0;
  double sampleRate = 0.0;
};

/// Minimal WAV (RIFF/WAVE) reader supporting PCM 16-bit integer and IEEE
/// float 32-bit, mono or stereo (Phase 1.8: mdw-analyze's WAV input, and
/// Phase 1.9's fixture clips). No third-party dependency, matching §4.1.
/// Throws std::runtime_error on a malformed or unsupported file.
[[nodiscard]] WavAudio readWavFile(const std::string& path);

/// Minimal WAV writer: always PCM 16-bit, for synthesizing fixture clips
/// (Phase 1.9) and mdw-analyze's own test fixtures. `audio.sampleRate` and
/// `audio.numChannels` are taken as given; `interleavedSamples` is clipped
/// to [-1, 1] before conversion.
void writeWavFile(const std::string& path, const WavAudio& audio);

} // namespace milkdawp::core
