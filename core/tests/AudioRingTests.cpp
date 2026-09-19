// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <atomic>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/AudioRing.h"

using milkdawp::core::AudioRing;

namespace {
std::vector<float> makeFrames(int startValue, std::size_t numFrames, int channels) {
  std::vector<float> v(numFrames * static_cast<std::size_t>(channels));
  for (std::size_t f = 0; f < numFrames; ++f) {
    for (int c = 0; c < channels; ++c) {
      v[f * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)] =
          static_cast<float>(startValue + static_cast<int>(f));
    }
  }
  return v;
}
} // namespace

TEST_CASE("AudioRing consumeHop reads frames in write order", "[core][AudioRing]") {
  AudioRing ring(16, 1);

  auto batch1 = makeFrames(0, 4, 1); // 0,1,2,3
  ring.write(batch1.data(), 4);

  std::vector<float> hop(2);
  REQUIRE(ring.consumeHop(hop.data(), 2));
  CHECK(hop[0] == 0.0f);
  CHECK(hop[1] == 1.0f);

  REQUIRE(ring.consumeHop(hop.data(), 2));
  CHECK(hop[0] == 2.0f);
  CHECK(hop[1] == 3.0f);
}

TEST_CASE("AudioRing consumeHop returns false when not enough data yet", "[core][AudioRing]") {
  AudioRing ring(16, 1);
  auto batch = makeFrames(0, 3, 1);
  ring.write(batch.data(), 3);

  std::vector<float> hop(4);
  CHECK_FALSE(ring.consumeHop(hop.data(), 4));
}

TEST_CASE("AudioRing handles wraparound across the capacity boundary", "[core][AudioRing]") {
  AudioRing ring(4, 1);

  auto batch1 = makeFrames(0, 3, 1); // 0,1,2
  ring.write(batch1.data(), 3);

  // Consume 2 of the 3 frames now, so the reader stays within `capacity`
  // frames of the writer once batch2 lands -- this test is about a write
  // physically wrapping past the end of the backing buffer, not about a
  // reader falling behind (that's the snap-forward test below).
  std::vector<float> hop(2);
  REQUIRE(ring.consumeHop(hop.data(), 2));
  CHECK(hop[0] == 0.0f);
  CHECK(hop[1] == 1.0f);

  auto batch2 = makeFrames(3, 3, 1); // 3,4,5 -- physically wraps past index 4
  ring.write(batch2.data(), 3);

  REQUIRE(ring.consumeHop(hop.data(), 2));
  CHECK(hop[0] == 2.0f);
  CHECK(hop[1] == 3.0f);
  REQUIRE(ring.consumeHop(hop.data(), 2));
  CHECK(hop[0] == 4.0f);
  CHECK(hop[1] == 5.0f);
}

TEST_CASE("AudioRing snaps a lagging reader forward instead of returning stale data",
          "[core][AudioRing]") {
  AudioRing ring(4, 1);
  // Write far more than capacity before the reader ever consumes anything.
  auto batch = makeFrames(0, 10, 1); // 0..9, only the last 4 (6,7,8,9) survive
  ring.write(batch.data(), 10);

  std::vector<float> hop(2);
  REQUIRE(ring.consumeHop(hop.data(), 2));
  // The reader was 10 frames behind a ring that only holds 4; it should have
  // been snapped to the oldest still-available frame (6), not blocked forever
  // or handed zeros/garbage for frames that no longer exist.
  CHECK(hop[0] == 6.0f);
  CHECK(hop[1] == 7.0f);
}

TEST_CASE("AudioRing copyLatest right-aligns and zero-pads before any data exists",
          "[core][AudioRing]") {
  AudioRing ring(8, 1);
  auto batch = makeFrames(10, 3, 1); // 10,11,12
  ring.write(batch.data(), 3);

  std::vector<float> latest(5, -1.0f);
  ring.copyLatest(latest.data(), 5);
  CHECK(latest[0] == 0.0f);
  CHECK(latest[1] == 0.0f);
  CHECK(latest[2] == 10.0f);
  CHECK(latest[3] == 11.0f);
  CHECK(latest[4] == 12.0f);
}

TEST_CASE("AudioRing copyLatest reflects the most recent frames after wraparound",
          "[core][AudioRing]") {
  AudioRing ring(4, 1);
  auto batch = makeFrames(0, 10, 1); // 0..9
  ring.write(batch.data(), 10);

  std::vector<float> latest(4);
  ring.copyLatest(latest.data(), 4);
  CHECK(latest[0] == 6.0f);
  CHECK(latest[1] == 7.0f);
  CHECK(latest[2] == 8.0f);
  CHECK(latest[3] == 9.0f);
}

TEST_CASE("AudioRing survives concurrent writer/reader without tearing (TSan target)",
          "[core][AudioRing][concurrency]") {
  constexpr std::size_t channels = 2;
  constexpr std::size_t totalFrames = 20000;
  constexpr std::size_t writeChunk = 7; // deliberately not a divisor of capacity
  constexpr std::size_t hopSize = 5;

  // Capacity is deliberately much smaller than totalFrames: the point of this
  // test is that concurrent access never tears a frame (never reads a frame
  // half-written), not that every frame survives -- AudioRing is explicitly
  // allowed to drop frames when the reader falls behind (§4.2: audio must
  // never block on a full ring), so the test must not assume a specific
  // number of frames survive to be consumed.
  AudioRing ring(256, static_cast<int>(channels));
  std::atomic<bool> writerDone{false};

  std::thread writer([&] {
    std::size_t written = 0;
    std::size_t value = 0;
    while (written < totalFrames) {
      const std::size_t n = std::min(writeChunk, totalFrames - written);
      auto frames = makeFrames(static_cast<int>(value), n, static_cast<int>(channels));
      ring.write(frames.data(), n);
      written += n;
      value += n;
    }
    writerDone.store(true, std::memory_order_release);
  });

  std::size_t consumedHops = 0;
  std::vector<float> hop(hopSize * channels);

  // Poll until the writer has finished and a bounded grace period of empty
  // reads confirms the ring has been fully drained -- never until a target
  // hop count, which could be unreachable if frames were dropped.
  int emptyReadsAfterWriterDone = 0;
  while (emptyReadsAfterWriterDone < 1000) {
    if (ring.consumeHop(hop.data(), hopSize)) {
      ++consumedHops;
      // Every channel within a frame must carry the same value (see
      // makeFrames); if the ring tore a write mid-frame, this would fail.
      for (std::size_t f = 0; f < hopSize; ++f) {
        for (std::size_t c = 1; c < channels; ++c) {
          CHECK(hop[f * channels] == hop[f * channels + c]);
        }
      }
    } else if (writerDone.load(std::memory_order_acquire)) {
      ++emptyReadsAfterWriterDone;
    }
  }

  writer.join();
  CHECK(consumedHops > 0);
}
