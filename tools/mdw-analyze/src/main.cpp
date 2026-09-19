// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

// mdw-analyze: offline WAV-in analysis CLI (Phase 1.8). Runs the same
// Analyzer/OnsetDetector/TempoTracker/BeatClock pipeline the engine will
// drive live (§4.3), so beat detection and transition logic can be
// iterated on and scored against fixtures without a DAW or GPU (§4.10).
//
// Usage:
//   mdw-analyze <input.wav> [--reference beats.txt] [--json out.json]
//               [--csv out.csv] [--plot out.svg]
//   mdw-analyze --suite <fixturesDir> [--thresholds thresholds.json]

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "milkdawp/core/Analyzer.h"
#include "milkdawp/core/BeatClock.h"
#include "milkdawp/core/OnsetDetector.h"
#include "milkdawp/core/Resample.h"
#include "milkdawp/core/Scoring.h"
#include "milkdawp/core/TempoTracker.h"
#include "milkdawp/core/Version.h"
#include "milkdawp/core/Wav.h"

namespace {

using namespace milkdawp::core;

constexpr double kInternalSampleRate = 48000.0;

struct AnalysisResult {
  std::vector<double> onsetTimes;
  std::vector<double> bassOnsetTimes;
  std::vector<double> beatTimes;
  std::vector<float> timesSeconds;
  std::vector<float> tempoBpm;
  std::vector<float> tempoConfidence;
  std::vector<AnalysisFrame> frames;
  float finalBpm = 0.0f;
};

std::vector<float> downmixToMono(const WavAudio& audio) {
  if (audio.numChannels == 1) {
    return audio.interleavedSamples;
  }
  const std::size_t numFrames = audio.interleavedSamples.size() / static_cast<std::size_t>(audio.numChannels);
  std::vector<float> mono(numFrames);
  for (std::size_t i = 0; i < numFrames; ++i) {
    double sum = 0.0;
    for (int c = 0; c < audio.numChannels; ++c) {
      sum += audio.interleavedSamples[i * static_cast<std::size_t>(audio.numChannels) +
                                       static_cast<std::size_t>(c)];
    }
    mono[i] = static_cast<float>(sum / audio.numChannels);
  }
  return mono;
}

AnalysisResult analyzeWav(const WavAudio& audio) {
  auto mono = downmixToMono(audio);
  auto resampled = (audio.sampleRate == kInternalSampleRate)
                        ? mono
                        : resampleLinear(mono, audio.sampleRate, kInternalSampleRate);

  Analyzer analyzer(kInternalSampleRate);
  OnsetDetector broadbandDetector(kInternalSampleRate);
  OnsetDetector bassDetector(kInternalSampleRate);
  TempoTracker tempoTracker(kInternalSampleRate);
  BeatClock beatClock(kInternalSampleRate);

  const std::size_t hopSize = Analyzer::hopSize;
  std::size_t numHops = (resampled.size() + hopSize - 1) / hopSize;
  numHops = std::max<std::size_t>(numHops, 1);
  resampled.resize(numHops * hopSize, 0.0f);

  AnalysisResult result;
  bool haveLastBeatIndex = false;
  std::uint64_t lastBeatIndex = 0;

  for (std::size_t hop = 0; hop < numHops; ++hop) {
    const std::uint64_t samplePos = hop * hopSize;
    const float* hopData = &resampled[samplePos];

    auto frame = analyzer.processHop(hopData);
    result.frames.push_back(frame);

    auto broadbandOnset = broadbandDetector.processHop(frame.onsetStrength);
    auto bassOnset = bassDetector.processHop(frame.bassOnsetStrength);
    auto tempo = tempoTracker.processHop(frame.onsetStrength);
    auto beatState = beatClock.processHop(samplePos, tempo, bassOnset);

    if (broadbandOnset) {
      result.onsetTimes.push_back(static_cast<double>(broadbandOnset->samplePos) / kInternalSampleRate);
    }
    if (bassOnset) {
      result.bassOnsetTimes.push_back(static_cast<double>(bassOnset->samplePos) / kInternalSampleRate);
    }
    if (haveLastBeatIndex && beatState.beatIndex > lastBeatIndex) {
      result.beatTimes.push_back(static_cast<double>(samplePos) / kInternalSampleRate);
    }
    haveLastBeatIndex = true;
    lastBeatIndex = beatState.beatIndex;

    result.timesSeconds.push_back(static_cast<float>(samplePos) / static_cast<float>(kInternalSampleRate));
    result.tempoBpm.push_back(tempo.bpm);
    result.tempoConfidence.push_back(tempo.confidence);
    result.finalBpm = tempo.bpm;
  }

  return result;
}

std::vector<double> readReferenceBeats(const std::string& path) {
  std::ifstream file(path);
  std::vector<double> beats;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }
    try {
      beats.push_back(std::stod(line));
    } catch (const std::exception&) {
      // Skip malformed lines rather than aborting the whole run.
    }
  }
  return beats;
}

double referenceBpmFromBeats(const std::vector<double>& beats) {
  if (beats.size() < 2) {
    return 0.0;
  }
  std::vector<double> sorted = beats;
  std::sort(sorted.begin(), sorted.end());
  std::vector<double> intervals;
  for (std::size_t i = 1; i < sorted.size(); ++i) {
    intervals.push_back(sorted[i] - sorted[i - 1]);
  }
  std::sort(intervals.begin(), intervals.end());
  const double medianInterval = intervals[intervals.size() / 2];
  return medianInterval > 0.0 ? 60.0 / medianInterval : 0.0;
}

void writeJson(const std::string& path, const AnalysisResult& result) {
  std::ofstream out(path);
  out << "{\n";
  out << "  \"finalBpm\": " << result.finalBpm << ",\n";

  auto writeArray = [&](const char* name, const std::vector<double>& values, bool trailingComma) {
    out << "  \"" << name << "\": [";
    for (std::size_t i = 0; i < values.size(); ++i) {
      out << (i > 0 ? ", " : "") << values[i];
    }
    out << "]" << (trailingComma ? "," : "") << "\n";
  };
  writeArray("onsets", result.onsetTimes, true);
  writeArray("bassOnsets", result.bassOnsetTimes, true);
  writeArray("beats", result.beatTimes, true);

  out << "  \"tempoCurve\": [\n";
  for (std::size_t i = 0; i < result.timesSeconds.size(); ++i) {
    out << "    {\"t\": " << result.timesSeconds[i] << ", \"bpm\": " << result.tempoBpm[i]
        << ", \"confidence\": " << result.tempoConfidence[i] << "}"
        << (i + 1 < result.timesSeconds.size() ? "," : "") << "\n";
  }
  out << "  ]\n";
  out << "}\n";
}

void writeCsv(const std::string& path, const AnalysisResult& result) {
  std::ofstream out(path);
  out << "time,bass,lowMid,mid,high,rms,onsetStrength,bassOnsetStrength,bpm,confidence\n";
  for (std::size_t i = 0; i < result.frames.size(); ++i) {
    const auto& f = result.frames[i];
    out << result.timesSeconds[i] << ',' << f.bassEnergy << ',' << f.lowMidEnergy << ',' << f.midEnergy
        << ',' << f.highEnergy << ',' << f.broadbandRms << ',' << f.onsetStrength << ','
        << f.bassOnsetStrength << ',' << result.tempoBpm[i] << ',' << result.tempoConfidence[i] << '\n';
  }
}

void writePlotSvg(const std::string& path, const AnalysisResult& result) {
  constexpr int width = 1000;
  constexpr int height = 220;
  std::ofstream out(path);
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height
      << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
  out << "  <rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";

  if (!result.frames.empty()) {
    float maxRms = 1e-6f;
    for (const auto& f : result.frames) {
      maxRms = std::max(maxRms, f.broadbandRms);
    }
    const double xScale = static_cast<double>(width) / static_cast<double>(result.frames.size());

    out << "  <polyline fill=\"none\" stroke=\"black\" stroke-width=\"1\" points=\"";
    for (std::size_t i = 0; i < result.frames.size(); ++i) {
      const double x = static_cast<double>(i) * xScale;
      const double y = height - 10.0 - (static_cast<double>(result.frames[i].broadbandRms) / maxRms) * (height - 20.0);
      out << x << ',' << y << ' ';
    }
    out << "\"/>\n";

    const double totalSeconds = static_cast<double>(result.timesSeconds.back());
    for (double beat : result.beatTimes) {
      const double x = totalSeconds > 0.0 ? (beat / totalSeconds) * width : 0.0;
      out << "  <line x1=\"" << x << "\" y1=\"0\" x2=\"" << x << "\" y2=\"" << height
          << "\" stroke=\"red\" stroke-width=\"0.5\" stroke-opacity=\"0.6\"/>\n";
    }
  }

  out << "</svg>\n";
}

struct CliOptions {
  std::string inputWavPath;
  std::optional<std::string> referencePath;
  std::optional<std::string> jsonPath;
  std::optional<std::string> csvPath;
  std::optional<std::string> plotPath;
  std::optional<std::string> suiteDirectory;
};

CliOptions parseArgs(int argc, char** argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto nextArg = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };

    if (arg == "--reference") {
      options.referencePath = nextArg();
    } else if (arg == "--json") {
      options.jsonPath = nextArg();
    } else if (arg == "--csv") {
      options.csvPath = nextArg();
    } else if (arg == "--plot") {
      options.plotPath = nextArg();
    } else if (arg == "--suite") {
      options.suiteDirectory = nextArg();
    } else if (arg == "--thresholds") {
      ++i; // consumed by runSuite() re-parsing argv; accepted here so parseArgs doesn't choke on it
    } else if (!arg.empty() && arg[0] != '-') {
      options.inputWavPath = arg;
    }
  }
  return options;
}

int runSingleFile(const CliOptions& options) {
  WavAudio audio;
  try {
    audio = readWavFile(options.inputWavPath);
  } catch (const std::exception& e) {
    std::cerr << "mdw-analyze: " << e.what() << "\n";
    return 1;
  }

  auto result = analyzeWav(audio);

  std::printf("mdw-analyze: %s -- %.1f bpm, %zu onsets, %zu beats detected\n",
              options.inputWavPath.c_str(), static_cast<double>(result.finalBpm), result.onsetTimes.size(),
              result.beatTimes.size());

  if (options.referencePath) {
    auto reference = readReferenceBeats(*options.referencePath);
    auto fMeasure = computeBeatFMeasure(result.beatTimes, reference);
    const double referenceBpm = referenceBpmFromBeats(reference);
    const double tempoError = computeOctaveTolerantTempoError(result.finalBpm, referenceBpm);

    std::printf("  reference: %zu beats, %.1f bpm (from median interval)\n", reference.size(), referenceBpm);
    std::printf("  F-measure: %.3f (precision %.3f, recall %.3f)\n", fMeasure.fMeasure, fMeasure.precision,
                fMeasure.recall);
    std::printf("  tempo error (octave-tolerant): %.3f%%\n", tempoError * 100.0);
  }

  if (options.jsonPath) {
    writeJson(*options.jsonPath, result);
  }
  if (options.csvPath) {
    writeCsv(*options.csvPath, result);
  }
  if (options.plotPath) {
    writePlotSvg(*options.plotPath, result);
  }

  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("mdw-analyze (milkdawp_core %s)\n", milkdawp::core::versionString());
    std::printf("Usage:\n");
    std::printf("  mdw-analyze <input.wav> [--reference beats.txt] [--json out.json] "
                "[--csv out.csv] [--plot out.svg]\n");
    std::printf("  mdw-analyze --suite <fixturesDir> [--thresholds thresholds.json]\n");
    return 0;
  }

  auto options = parseArgs(argc, argv);

  if (options.suiteDirectory) {
    // Phase 1.10's fixture suite runner lives in RunSuite.cpp (kept
    // separate so the CI metric gate's logic is independently testable).
    extern int runFixtureSuite(const std::string& fixturesDir, const std::string& thresholdsPath);

    std::string thresholdsPath = *options.suiteDirectory + "/thresholds.json";
    for (int i = 1; i < argc; ++i) {
      if (std::string(argv[i]) == "--thresholds" && i + 1 < argc) {
        thresholdsPath = argv[i + 1];
      }
    }
    return runFixtureSuite(*options.suiteDirectory, thresholdsPath);
  }

  if (options.inputWavPath.empty()) {
    std::cerr << "mdw-analyze: no input WAV file given\n";
    return 1;
  }

  return runSingleFile(options);
}
