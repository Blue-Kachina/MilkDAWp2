// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/ParameterModel.h"
#include "milkdawp/core/StateSchema.h"

using namespace milkdawp::core;

namespace {
/// A V1StateRecord matching v1 0.7.5's real defaults (see
/// PluginProcessor.cpp's createParameterLayout(), fetched from
/// github.com/Blue-Kachina/MilkDAWp for this migration).
V1StateRecord makeV1Defaults() {
  V1StateRecord v1;
  v1.version = "0.7.5";
  v1.presetPath = "C:/Presets/favorite.milk";
  v1.playlistFolderPath = "C:/Presets";
  v1.editorWidth = 800;
  v1.editorHeight = 600;
  v1.paramValues = {
      {"beatSensitivity", 1.0f},
      {"transitionDurationSeconds", 5.0f},
      {"shuffle", 0.0f},
      {"lockCurrentPreset", 0.0f},
      {"presetIndex", 3.0f},
      {"triggerNext", 0.0f},
      {"triggerPrev", 0.0f},
      {"transitionJitterEnabled", 0.0f},
      {"transitionDurationMin", 3.0f},
      {"transitionDurationMax", 15.0f},
      {"hardCutEnabled", 0.0f},
      {"hardCutSensitivity", 0.5f},
      {"softCutDuration", 3.0f},
      {"hardCutDuration", 5.0f},
      {"qualityOverride", 0.0f},
  };
  return v1;
}
} // namespace

TEST_CASE("migrateFromV1 carries every v1 parameter forward unchanged", "[core][StateSchema]") {
  auto v1 = makeV1Defaults();
  auto v2 = migrateFromV1(v1);

  CHECK(v2.schemaVersion == StateSchemaV2::currentSchemaVersion);
  CHECK(v2.presetAbsolutePath == "C:/Presets/favorite.milk");
  CHECK(v2.playlistFolderPath == "C:/Presets");
  CHECK(v2.editorWidth == 800);
  CHECK(v2.editorHeight == 600);

  for (const auto& [id, value] : v1.paramValues) {
    INFO("parameter: " << id);
    REQUIRE(v2.paramValues.count(id) == 1);
    CHECK(v2.paramValues.at(id) == value);
  }
}

TEST_CASE("migrateFromV1 gives new v2-only parameters their ParameterModel default",
          "[core][StateSchema]") {
  auto v2 = migrateFromV1(makeV1Defaults());

  const auto& params = allParameters();
  auto* mode = findParameter(params, "transitionMode");
  auto* bars = findParameter(params, "transitionBars");
  REQUIRE(mode != nullptr);
  REQUIRE(bars != nullptr);

  CHECK(v2.paramValues.at("transitionMode") == mode->defaultValue);
  CHECK(v2.paramValues.at("transitionBars") == bars->defaultValue);
}

TEST_CASE("migrateFromV1 derives presetSelectionPolicy from v1's shuffle flag",
          "[core][StateSchema]") {
  auto v1Off = makeV1Defaults();
  v1Off.paramValues["shuffle"] = 0.0f;
  CHECK(migrateFromV1(v1Off).paramValues.at("presetSelectionPolicy") == 0.0f); // Sequential

  auto v1On = makeV1Defaults();
  v1On.paramValues["shuffle"] = 1.0f;
  CHECK(migrateFromV1(v1On).paramValues.at("presetSelectionPolicy") == 1.0f); // ShuffleNoRepeat
}

TEST_CASE("migrateFromV1 falls back to the v2 default for a missing v1 parameter",
          "[core][StateSchema]") {
  auto v1 = makeV1Defaults();
  v1.paramValues.erase("hardCutSensitivity"); // simulate an incomplete/corrupt v1 state

  auto v2 = migrateFromV1(v1);

  auto* spec = findParameter(allParameters(), "hardCutSensitivity");
  REQUIRE(spec != nullptr);
  CHECK(v2.paramValues.at("hardCutSensitivity") == spec->defaultValue);
}

TEST_CASE("StateSchemaV2 serialize/deserialize round-trips exactly", "[core][StateSchema]") {
  StateSchemaV2 original;
  original.schemaVersion = 2;
  original.presetAbsolutePath = "/library/preset.milk";
  original.playlistFolderPath = "/library";
  original.editorWidth = 1024;
  original.editorHeight = 768;
  original.paramValues = {{"beatSensitivity", 1.25f}, {"presetIndex", 7.0f}, {"shuffle", 1.0f}};

  auto text = serializeStateSchemaV2(original);
  auto roundTripped = deserializeStateSchemaV2(text);

  CHECK(roundTripped.schemaVersion == original.schemaVersion);
  CHECK(roundTripped.presetAbsolutePath == original.presetAbsolutePath);
  CHECK(roundTripped.playlistFolderPath == original.playlistFolderPath);
  CHECK(roundTripped.editorWidth == original.editorWidth);
  CHECK(roundTripped.editorHeight == original.editorHeight);
  REQUIRE(roundTripped.paramValues.size() == original.paramValues.size());
  for (const auto& [id, value] : original.paramValues) {
    REQUIRE(roundTripped.paramValues.count(id) == 1);
    CHECK(roundTripped.paramValues.at(id) == value);
  }
}

TEST_CASE("StateSchemaV2 round-trips a full migrated v1 session", "[core][StateSchema]") {
  auto migrated = migrateFromV1(makeV1Defaults());
  auto roundTripped = deserializeStateSchemaV2(serializeStateSchemaV2(migrated));

  CHECK(roundTripped.presetAbsolutePath == migrated.presetAbsolutePath);
  REQUIRE(roundTripped.paramValues.size() == migrated.paramValues.size());
  for (const auto& [id, value] : migrated.paramValues) {
    CHECK(roundTripped.paramValues.at(id) == value);
  }
}
