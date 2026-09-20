// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>

#include "milkdawp/core/DoubleBufferedSnapshot.h"

using namespace milkdawp::core;

namespace {
struct Payload {
  int a = 0;
  double b = 0.0;
};
} // namespace

TEST_CASE("DoubleBufferedSnapshot::read returns a default value before any publish",
          "[core][DoubleBufferedSnapshot]") {
  DoubleBufferedSnapshot<Payload> snapshot;
  const auto value = snapshot.read();
  CHECK(value.a == 0);
  CHECK(value.b == 0.0);
}

TEST_CASE("DoubleBufferedSnapshot::read reflects the most recent publish", "[core][DoubleBufferedSnapshot]") {
  DoubleBufferedSnapshot<Payload> snapshot;
  snapshot.publish({1, 1.5});
  CHECK(snapshot.read().a == 1);

  snapshot.publish({2, 2.5});
  CHECK(snapshot.read().a == 2);
  CHECK(snapshot.read().b == 2.5);
}

TEST_CASE("DoubleBufferedSnapshot never hands a reader a torn value under concurrent publish",
          "[core][DoubleBufferedSnapshot]") {
  DoubleBufferedSnapshot<Payload> snapshot;
  std::atomic<bool> stop{false};
  std::atomic<bool> sawTornValue{false};

  std::thread writer([&] {
    int i = 0;
    while (!stop.load(std::memory_order_relaxed)) {
      const auto v = ++i;
      // A torn read would see a.a from one publish and b from another; make
      // the two fields derivable from each other so a torn mix is detectable.
      snapshot.publish({v, static_cast<double>(v) * 2.0});
    }
  });

  for (int i = 0; i < 200000; ++i) {
    const auto value = snapshot.read();
    if (static_cast<double>(value.a) * 2.0 != value.b) {
      sawTornValue.store(true, std::memory_order_relaxed);
      break;
    }
  }

  stop.store(true, std::memory_order_relaxed);
  writer.join();

  CHECK_FALSE(sawTornValue.load());
}
