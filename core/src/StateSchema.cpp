// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/core/StateSchema.h"

#include <sstream>

#include "milkdawp/core/ParameterModel.h"

namespace milkdawp::core {

StateSchemaV2 migrateFromV1(const V1StateRecord& v1) {
  StateSchemaV2 v2;
  v2.schemaVersion = StateSchemaV2::currentSchemaVersion;
  v2.presetAbsolutePath = v1.presetPath;
  v2.playlistFolderPath = v1.playlistFolderPath;
  v2.editorWidth = v1.editorWidth;
  v2.editorHeight = v1.editorHeight;

  for (const auto& spec : allParameters()) {
    if (!spec.v1Alias.empty()) {
      auto it = v1.paramValues.find(spec.v1Alias);
      v2.paramValues[spec.id] = (it != v1.paramValues.end()) ? it->second : spec.defaultValue;
    } else {
      v2.paramValues[spec.id] = spec.defaultValue;
    }
  }

  // Derived mapping (not a direct alias): v1's boolean `shuffle` implies a
  // v2 preset-selection policy, so a migrated session keeps behaving the
  // way it used to rather than silently reverting to Sequential.
  if (auto it = v1.paramValues.find("shuffle"); it != v1.paramValues.end()) {
    constexpr float kSequential = 0.0f;
    constexpr float kShuffleNoRepeat = 1.0f;
    v2.paramValues["presetSelectionPolicy"] = (it->second != 0.0f) ? kShuffleNoRepeat : kSequential;
  }

  return v2;
}

std::string serializeStateSchemaV2(const StateSchemaV2& state) {
  std::ostringstream out;
  out << "schemaVersion=" << state.schemaVersion << "\n";
  out << "presetAbsolutePath=" << state.presetAbsolutePath << "\n";
  out << "playlistFolderPath=" << state.playlistFolderPath << "\n";
  out << "editorWidth=" << state.editorWidth << "\n";
  out << "editorHeight=" << state.editorHeight << "\n";
  for (const auto& [id, value] : state.paramValues) {
    out << "param." << id << "=" << value << "\n";
  }
  return out.str();
}

StateSchemaV2 deserializeStateSchemaV2(const std::string& text) {
  StateSchemaV2 state;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);

    if (key == "schemaVersion") {
      state.schemaVersion = std::stoi(value);
    } else if (key == "presetAbsolutePath") {
      state.presetAbsolutePath = value;
    } else if (key == "playlistFolderPath") {
      state.playlistFolderPath = value;
    } else if (key == "editorWidth") {
      state.editorWidth = std::stoi(value);
    } else if (key == "editorHeight") {
      state.editorHeight = std::stoi(value);
    } else if (key.rfind("param.", 0) == 0) {
      state.paramValues[key.substr(6)] = std::stof(value);
    }
  }
  return state;
}

} // namespace milkdawp::core
