// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

#include "milkdawp/engine/TransitionExecutor.h"

using namespace milkdawp::engine;
using milkdawp::core::CutStyle;
using milkdawp::core::TransitionRequestMessage;

namespace {
constexpr double kSampleRate = 48000.0;
}

TEST_CASE("TransitionExecutor does not fire a Hard cut before dueAtSample", "[engine][TransitionExecutor]") {
  TransitionExecutor executor(kSampleRate);
  REQUIRE(executor.pushRequest({/*presetId=*/1, CutStyle::Hard, /*blendSeconds=*/0.0f, /*dueAtSample=*/1000}));

  int fireCount = 0;
  executor.onTick(999, [&](const TransitionExecutor::DueTransition&) { ++fireCount; });
  CHECK(fireCount == 0);
  CHECK(executor.pendingCount() == 1);
}

TEST_CASE("TransitionExecutor fires a Hard cut exactly at dueAtSample with zero landing error",
          "[engine][TransitionExecutor]") {
  TransitionExecutor executor(kSampleRate);
  REQUIRE(executor.pushRequest({1, CutStyle::Hard, 0.0f, 1000}));

  std::optional<TransitionExecutor::DueTransition> fired;
  executor.onTick(1000, [&](const TransitionExecutor::DueTransition& due) { fired = due; });

  REQUIRE(fired.has_value());
  CHECK(fired->request.presetId == 1);
  CHECK(fired->actualIssueSample == 1000);
  CHECK(fired->landingErrorSamples == 0);
  CHECK(executor.pendingCount() == 0);
}

TEST_CASE("TransitionExecutor fires a Soft cut blend/2 early", "[engine][TransitionExecutor]") {
  TransitionExecutor executor(kSampleRate);
  // 2s blend, so it should issue 1s (48000 samples) before dueAtSample.
  REQUIRE(executor.pushRequest({2, CutStyle::Soft, /*blendSeconds=*/2.0f, /*dueAtSample=*/100000}));

  int fireCount = 0;
  executor.onTick(100000 - 48000 - 1, [&](const TransitionExecutor::DueTransition&) { ++fireCount; });
  CHECK(fireCount == 0);

  std::optional<TransitionExecutor::DueTransition> fired;
  executor.onTick(100000 - 48000, [&](const TransitionExecutor::DueTransition& due) { fired = due; });
  REQUIRE(fired.has_value());
  CHECK(fired->landingErrorSamples == 0);
}

TEST_CASE("TransitionExecutor reports a positive landing error when a tick overshoots the issue sample",
          "[engine][TransitionExecutor]") {
  TransitionExecutor executor(kSampleRate);
  REQUIRE(executor.pushRequest({3, CutStyle::Hard, 0.0f, 1000}));

  std::optional<TransitionExecutor::DueTransition> fired;
  executor.onTick(1050, [&](const TransitionExecutor::DueTransition& due) { fired = due; });
  REQUIRE(fired.has_value());
  CHECK(fired->landingErrorSamples == 50);
}

TEST_CASE("TransitionExecutor fires a request whose issue sample is already in the past on the very next tick",
          "[engine][TransitionExecutor]") {
  TransitionExecutor executor(kSampleRate);
  // Pushed "late": dueAtSample is behind currentSample already.
  REQUIRE(executor.pushRequest({4, CutStyle::Hard, 0.0f, /*dueAtSample=*/500}));

  int fireCount = 0;
  executor.onTick(600, [&](const TransitionExecutor::DueTransition&) { ++fireCount; });
  CHECK(fireCount == 1);
}

TEST_CASE("TransitionExecutor fires multiple due requests in push order and leaves not-yet-due ones pending",
          "[engine][TransitionExecutor]") {
  TransitionExecutor executor(kSampleRate);
  REQUIRE(executor.pushRequest({10, CutStyle::Hard, 0.0f, 100}));
  REQUIRE(executor.pushRequest({20, CutStyle::Hard, 0.0f, 200}));
  REQUIRE(executor.pushRequest({30, CutStyle::Hard, 0.0f, 9999}));

  std::vector<std::uint32_t> firedIds;
  executor.onTick(200, [&](const TransitionExecutor::DueTransition& due) { firedIds.push_back(due.request.presetId); });

  REQUIRE(firedIds.size() == 2);
  CHECK(firedIds[0] == 10);
  CHECK(firedIds[1] == 20);
  CHECK(executor.pendingCount() == 1);
}

TEST_CASE("TransitionExecutor::pushRequest saturates gracefully instead of blocking",
          "[engine][TransitionExecutor]") {
  TransitionExecutor executor(kSampleRate);
  std::size_t pushed = 0;
  for (int i = 0; i < 1000; ++i) {
    if (executor.pushRequest({static_cast<std::uint32_t>(i), CutStyle::Hard, 0.0f, 0})) {
      ++pushed;
    }
  }
  CHECK(pushed > 0);
  CHECK(pushed < 1000);
}
