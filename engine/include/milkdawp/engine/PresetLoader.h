// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <juce_core/juce_core.h>

namespace milkdawp::engine {

/// Preset I/O thread work (§4.4, §7 Phase 2.5): cheap syntax pre-validation,
/// a failure blacklist, and file prefetch with load-time measurement. Never
/// touches GL (§4.2) -- everything here is plain file I/O, safe to run on
/// its own thread ahead of when RenderEngine actually needs the preset.
///
/// The blacklist is written from two places: RenderEngine's preset-switch-
/// failed callback (fires on the render/GL thread, on an *actual* projectM
/// load failure this class's cheap pre-validation couldn't have caught) and
/// this class's own validate()/prefetch() (preset-IO thread, on a pre-load
/// failure). Failures are rare -- nowhere near the audio callback or the
/// steady-state per-frame render path -- so a mutex here is the honest
/// simplification, not a violation of §4.2's real-time rules.
///
/// Does not yet integrate with core::Messages' PresetLoadResultMessage: that
/// message carries an *interned* preset ID (§4.2: "strings cross threads
/// only as interned preset IDs, never as juce::String"), and the table that
/// would do that interning -- the PresetLibrary named in §4.1's architecture
/// diagram -- doesn't exist yet (only Playlist, Phase 1.11, does). This
/// class works in plain paths for now; wiring it to PresetLoadResultMessage
/// is deferred until PresetLibrary lands.
class PresetLoader {
public:
  struct ValidationResult {
    bool ok = false;
    std::string reason; // empty when ok == true
  };

  struct PrefetchResult {
    bool success = false;
    std::string reason;   // empty when success == true
    std::string contents; // file bytes; only meaningful when success == true
    std::uint32_t loadTimeMicros = 0;
  };

  /// Cheap, non-parsing sanity check: the file exists, is non-empty, its
  /// first bytes don't look like binary garbage, and it contains at least
  /// one '=' assignment (every real .milk preset is a flat list of
  /// key=value / equation lines). This is *not* a projectM-compatible
  /// parser and never claims to validate preset semantics -- only to catch
  /// "this obviously is not a preset file" before the render thread would
  /// otherwise hit it via a hard projectM load failure.
  [[nodiscard]] static ValidationResult validate(const juce::File& presetFile);

  /// Runs validate() first (a failed validation short-circuits before
  /// touching the filesystem a second time), then reads the whole file and
  /// times the read. Records any failure in this loader's blacklist.
  [[nodiscard]] PrefetchResult prefetch(const juce::File& presetFile);

  void blacklist(const std::string& presetPath, std::string reason);
  [[nodiscard]] bool isBlacklisted(const std::string& presetPath) const;
  [[nodiscard]] std::optional<std::string> blacklistReason(const std::string& presetPath) const;
  void clearBlacklistEntry(const std::string& presetPath);
  void clearBlacklist();
  [[nodiscard]] std::size_t blacklistSize() const;

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::string> blacklist_;
};

} // namespace milkdawp::engine
