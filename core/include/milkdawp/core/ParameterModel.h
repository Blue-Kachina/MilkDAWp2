// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

namespace milkdawp::core {

enum class ParameterType { Float, Bool, Int, Choice };

/// One parameter's canonical definition (Phase 1.13). Shared by the plugin
/// (APVTS layout), the app (preferences/MIDI-learn targets), and
/// MigrateFromV1 (via v1Alias). This is the single source of truth: nothing
/// else in the codebase should hardcode a parameter's id, range, or default.
struct ParameterSpec {
  std::string id;   // v2 canonical id, stable across releases (never rename without an ADR)
  std::string displayName;
  ParameterType type;
  float minValue = 0.0f;   // Float/Int only
  float maxValue = 0.0f;   // Float/Int only
  float defaultValue = 0.0f;
  bool automatable = true;
  std::string v1Alias;                // v1's parameter id, or "" if this parameter is new in v2
  std::vector<std::string> choices;   // Choice only, in index order (defaultValue is the index)
};

/// The full v1.0 parameter surface: v1's 15 parameters carried forward
/// unchanged (§2.9: "a good 1.0 surface... basis for state migration") plus
/// the new v2-only transition-scheduling parameters from §4.4 (mode, bar
/// count, preset-selection policy), defaulted per §7 Phase 3.4 ("Beat-quantized,
/// 4 bars, soft 2 beats").
[[nodiscard]] const std::vector<ParameterSpec>& allParameters();

/// Look up a parameter by its v2 id. Returns nullptr if not found.
[[nodiscard]] const ParameterSpec* findParameter(const std::vector<ParameterSpec>& params,
                                                  const std::string& id);

/// Look up the (single) parameter whose v1Alias matches `v1Id`. Returns
/// nullptr if no v2 parameter carries that v1 alias.
[[nodiscard]] const ParameterSpec* findByV1Alias(const std::vector<ParameterSpec>& params,
                                                  const std::string& v1Id);

/// Renders `params` as a GitHub-flavoured Markdown table (id, name, type,
/// range/choices, default, automatable, v1 alias) -- the "generated docs
/// table" in Phase 1.13. See docs/parameters.md for the committed output.
[[nodiscard]] std::string renderParameterDocsMarkdown(const std::vector<ParameterSpec>& params);

} // namespace milkdawp::core
