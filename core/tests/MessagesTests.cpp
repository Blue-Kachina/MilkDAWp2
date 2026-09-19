// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <atomic>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Messages.h"

using namespace milkdawp::core;

TEST_CASE("SpscQueue push/pop round-trips values in FIFO order", "[core][Messages]") {
  SpscQueue<ParameterChangeMessage, 4> queue;

  REQUIRE(queue.push({1, 0.5f}));
  REQUIRE(queue.push({2, 0.25f}));

  auto a = queue.pop();
  REQUIRE(a.has_value());
  CHECK(a->parameterId == 1);
  CHECK(a->value == 0.5f);

  auto b = queue.pop();
  REQUIRE(b.has_value());
  CHECK(b->parameterId == 2);
}

TEST_CASE("SpscQueue pop on empty returns nullopt", "[core][Messages]") {
  SpscQueue<StatusSnapshotMessage, 4> queue;
  CHECK_FALSE(queue.pop().has_value());
}

TEST_CASE("SpscQueue push fails once full, without corrupting existing entries", "[core][Messages]") {
  // Capacity 4 means 3 usable slots (one is kept empty to distinguish full
  // from empty using only head/tail indices).
  SpscQueue<TransitionRequestMessage, 4> queue;

  REQUIRE(queue.push({1, CutStyle::Hard, 0.0f, 100}));
  REQUIRE(queue.push({2, CutStyle::Soft, 1.0f, 200}));
  REQUIRE(queue.push({3, CutStyle::Hard, 0.0f, 300}));
  CHECK_FALSE(queue.push({4, CutStyle::Hard, 0.0f, 400}));

  auto first = queue.pop();
  REQUIRE(first.has_value());
  CHECK(first->presetId == 1);

  // Freeing a slot allows exactly one more push.
  REQUIRE(queue.push({5, CutStyle::Soft, 2.0f, 500}));
  CHECK_FALSE(queue.push({6, CutStyle::Hard, 0.0f, 600}));
}

TEST_CASE("SpscQueue survives a real producer/consumer thread pair (TSan target)",
          "[core][Messages][concurrency]") {
  SpscQueue<PresetLoadResultMessage, 1024> queue;
  constexpr std::uint32_t messageCount = 50000;

  std::thread producer([&] {
    for (std::uint32_t i = 0; i < messageCount; ++i) {
      while (!queue.push({i, true, i})) {
        std::this_thread::yield();
      }
    }
  });

  std::uint32_t received = 0;
  while (received < messageCount) {
    if (auto msg = queue.pop()) {
      CHECK(msg->presetId == received);
      ++received;
    } else {
      std::this_thread::yield();
    }
  }

  producer.join();
  CHECK(received == messageCount);
}
