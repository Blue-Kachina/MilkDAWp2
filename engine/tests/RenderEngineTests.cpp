// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/AudioRing.h"
#include "milkdawp/engine/RenderEngine.h"

using namespace milkdawp::engine;

// Like ProjectMLibraryTests.cpp, these assume nothing about whether projectM
// is actually present on the machine running them. What's pinned down is the
// contract: an unavailable RenderEngine is a valid, inert object -- every
// call on it is a safe no-op, never a crash -- which is exactly the state
// this sandbox (no projectM installed) always exercises.

TEST_CASE("RenderEngine::create never returns null, even when projectM is unavailable",
          "[engine][RenderEngine]") {
  const auto engine = RenderEngine::create();
  REQUIRE(engine != nullptr);
  if (!engine->isAvailable()) {
    CHECK_FALSE(engine->unavailableReason().empty());
  }
}

TEST_CASE("RenderEngine accepts parameter updates and preset loads without crashing when unavailable",
          "[engine][RenderEngine]") {
  const auto engine = RenderEngine::create();
  REQUIRE(engine != nullptr);

  CHECK(engine->pushParameterUpdate({RenderEngine::ParameterTarget::BeatSensitivity, 1.5f}));
  engine->loadPreset("does/not/exist.milk", true);
  engine->setPresetSwitchFailedCallback([](std::string_view, std::string_view) {});
}

TEST_CASE("RenderEngine::renderFrame is a no-op when unavailable and never touches the ring's reader cursor",
          "[engine][RenderEngine]") {
  const auto engine = RenderEngine::create();
  REQUIRE(engine != nullptr);

  milkdawp::core::AudioRing ring(4096, 2);
  std::vector<float> silence(1024 * 2, 0.0f);
  ring.write(silence.data(), 1024);

  if (!engine->isAvailable()) {
    engine->renderFrame(ring, /*targetFbo=*/0);
    // consumeHop's cursor belongs to the analysis thread; renderFrame() must
    // never advance it (it only ever calls copyLatest()).
    std::vector<float> hop(512 * 2, -1.0f);
    CHECK(ring.consumeHop(hop.data(), 512));
  }
}

TEST_CASE("RenderEngine parameter queue saturates gracefully instead of blocking", "[engine][RenderEngine]") {
  const auto engine = RenderEngine::create();
  REQUIRE(engine != nullptr);

  std::size_t pushed = 0;
  for (std::size_t i = 0; i < RenderEngine::kParameterQueueCapacity * 2; ++i) {
    if (engine->pushParameterUpdate({RenderEngine::ParameterTarget::PresetDurationSeconds, 5.0f})) {
      ++pushed;
    }
  }
  CHECK(pushed > 0);
  CHECK(pushed < RenderEngine::kParameterQueueCapacity * 2);
}
