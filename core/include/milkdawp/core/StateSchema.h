// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace milkdawp::core {

/// A plain-data mirror of v1's `MilkDAWpState` ValueTree (§4.8), populated
/// by the plugin layer (which owns the actual `juce::ValueTree::readFromData`
/// call -- ValueTree is a JUCE type, so it never appears in milkdawp_core,
/// per §4.1). paramValues holds v1's raw APVTS values keyed by v1's
/// parameter id (see the "PARAM id/value" children JUCE's APVTS writes).
struct V1StateRecord {
  std::string version;
  std::string presetPath;
  std::string playlistFolderPath;
  int editorWidth = 0;
  int editorHeight = 0;
  std::map<std::string, float> paramValues;
};

/// MilkDAWp 2's canonical state (§4.8 "StateSchema v2"). Preset references
/// are stored as both an absolute path and (once PresetLibrary exists,
/// Phase 2.5) a {libraryRoot, relativePath, contentHash} triple so a moved
/// preset folder can be relinked; Phase 1 only has the plain paths.
struct StateSchemaV2 {
  static constexpr int currentSchemaVersion = 2;

  int schemaVersion = currentSchemaVersion;
  std::string presetAbsolutePath;
  std::string playlistFolderPath;
  int editorWidth = 0;
  int editorHeight = 0;
  std::map<std::string, float> paramValues; // keyed by ParameterModel's v2 ids
};

/// Maps a v1 session onto v2's schema (§4.8, §7 Phase 1.14): every v1
/// parameter carries its value forward via ParameterSpec::v1Alias; v2-only
/// parameters get ParameterModel's default, except presetSelectionPolicy,
/// which is derived from v1's boolean `shuffle` (true -> ShuffleNoRepeat,
/// false -> Sequential) so a migrated session's playback behaviour doesn't
/// silently change.
[[nodiscard]] StateSchemaV2 migrateFromV1(const V1StateRecord& v1);

/// Simple, deterministic, JUCE-free serialization for StateSchemaV2 (round-trip
/// tested per §4.8). One "key=value" pair per line; not intended to be the
/// plugin's actual on-disk/host-state format (Phase 3.2 wraps this, or
/// something compatible with it, in whatever juce::AudioProcessor::
/// getStateInformation needs) -- it exists so milkdawp_core's state logic is
/// testable without a JUCE dependency.
[[nodiscard]] std::string serializeStateSchemaV2(const StateSchemaV2& state);
[[nodiscard]] StateSchemaV2 deserializeStateSchemaV2(const std::string& text);

} // namespace milkdawp::core
