// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/core/Wav.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace milkdawp::core {

namespace {

std::uint16_t readU16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t readU32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

void writeU16(std::ostream& out, std::uint16_t v) {
  const char bytes[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
  out.write(bytes, 2);
}

void writeU32(std::ostream& out, std::uint32_t v) {
  const char bytes[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                          static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
  out.write(bytes, 4);
}

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatIeeeFloat = 3;

} // namespace

WavAudio readWavFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("readWavFile: could not open " + path);
  }

  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (bytes.size() < 44) {
    throw std::runtime_error("readWavFile: file too small to be a WAV: " + path);
  }

  if (std::string(bytes.begin(), bytes.begin() + 4) != "RIFF" ||
      std::string(bytes.begin() + 8, bytes.begin() + 12) != "WAVE") {
    throw std::runtime_error("readWavFile: not a RIFF/WAVE file: " + path);
  }

  std::size_t pos = 12;
  bool haveFmt = false;
  std::uint16_t formatTag = 0;
  std::uint16_t numChannels = 0;
  std::uint32_t sampleRate = 0;
  std::uint16_t bitsPerSample = 0;

  std::size_t dataOffset = 0;
  std::size_t dataSize = 0;
  bool haveData = false;

  while (pos + 8 <= bytes.size()) {
    const std::string chunkId(bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                               bytes.begin() + static_cast<std::ptrdiff_t>(pos) + 4);
    const std::uint32_t chunkSize = readU32(&bytes[pos + 4]);
    const std::size_t chunkDataStart = pos + 8;

    if (chunkDataStart + chunkSize > bytes.size()) {
      break; // truncated/corrupt trailing chunk; stop rather than read out of bounds
    }

    if (chunkId == "fmt ") {
      if (chunkSize < 16) {
        throw std::runtime_error("readWavFile: fmt chunk too small: " + path);
      }
      formatTag = readU16(&bytes[chunkDataStart]);
      numChannels = readU16(&bytes[chunkDataStart + 2]);
      sampleRate = readU32(&bytes[chunkDataStart + 4]);
      bitsPerSample = readU16(&bytes[chunkDataStart + 14]);
      haveFmt = true;
    } else if (chunkId == "data") {
      dataOffset = chunkDataStart;
      dataSize = chunkSize;
      haveData = true;
    }

    pos = chunkDataStart + chunkSize + (chunkSize % 2); // chunks are word-aligned
  }

  if (!haveFmt || !haveData) {
    throw std::runtime_error("readWavFile: missing fmt or data chunk: " + path);
  }
  if (formatTag != kFormatPcm && formatTag != kFormatIeeeFloat) {
    throw std::runtime_error("readWavFile: unsupported format tag " + std::to_string(formatTag) +
                              " (only PCM and IEEE float are supported): " + path);
  }
  if (numChannels == 0) {
    throw std::runtime_error("readWavFile: zero channels: " + path);
  }

  WavAudio audio;
  audio.numChannels = numChannels;
  audio.sampleRate = static_cast<double>(sampleRate);

  const std::uint8_t* data = &bytes[dataOffset];

  if (formatTag == kFormatPcm && bitsPerSample == 16) {
    const std::size_t numSamples = dataSize / 2;
    audio.interleavedSamples.resize(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
      const auto raw = static_cast<std::int16_t>(readU16(data + i * 2));
      audio.interleavedSamples[i] = static_cast<float>(raw) / 32768.0f;
    }
  } else if (formatTag == kFormatPcm && bitsPerSample == 8) {
    // 8-bit PCM WAV is unsigned, offset by 128.
    const std::size_t numSamples = dataSize;
    audio.interleavedSamples.resize(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
      audio.interleavedSamples[i] = (static_cast<float>(data[i]) - 128.0f) / 128.0f;
    }
  } else if (formatTag == kFormatIeeeFloat && bitsPerSample == 32) {
    const std::size_t numSamples = dataSize / 4;
    audio.interleavedSamples.resize(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
      const std::uint32_t bits = readU32(data + i * 4);
      float value;
      std::memcpy(&value, &bits, sizeof(float));
      audio.interleavedSamples[i] = value;
    }
  } else {
    throw std::runtime_error("readWavFile: unsupported bit depth " + std::to_string(bitsPerSample) +
                              " for format " + std::to_string(formatTag) + ": " + path);
  }

  return audio;
}

void writeWavFile(const std::string& path, const WavAudio& audio) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("writeWavFile: could not open for writing: " + path);
  }

  const std::uint16_t numChannels = static_cast<std::uint16_t>(audio.numChannels);
  const std::uint32_t sampleRate = static_cast<std::uint32_t>(audio.sampleRate);
  constexpr std::uint16_t bitsPerSample = 16;
  const std::uint16_t blockAlign = static_cast<std::uint16_t>(numChannels * (bitsPerSample / 8));
  const std::uint32_t byteRate = sampleRate * blockAlign;
  const std::uint32_t dataSize = static_cast<std::uint32_t>(audio.interleavedSamples.size() * 2);
  const std::uint32_t riffSize = 36 + dataSize;

  file.write("RIFF", 4);
  writeU32(file, riffSize);
  file.write("WAVE", 4);

  file.write("fmt ", 4);
  writeU32(file, 16);
  writeU16(file, kFormatPcm);
  writeU16(file, numChannels);
  writeU32(file, sampleRate);
  writeU32(file, byteRate);
  writeU16(file, blockAlign);
  writeU16(file, bitsPerSample);

  file.write("data", 4);
  writeU32(file, dataSize);
  for (float sample : audio.interleavedSamples) {
    const float clamped = std::clamp(sample, -1.0f, 1.0f);
    const auto intSample = static_cast<std::int16_t>(clamped * 32767.0f);
    writeU16(file, static_cast<std::uint16_t>(intSample));
  }
}

} // namespace milkdawp::core
