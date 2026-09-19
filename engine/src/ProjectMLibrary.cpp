// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/engine/ProjectMLibrary.h"

#include <string>

namespace milkdawp::engine {

namespace {

#if JUCE_WINDOWS
constexpr const char* kLibraryFileName = "projectM-4.dll";
#elif JUCE_MAC
constexpr const char* kLibraryFileName = "libprojectM-4.dylib";
#else
constexpr const char* kLibraryFileName = "libprojectM-4.so";
#endif

template <typename Fn>
bool resolveSymbol(juce::DynamicLibrary& lib, const char* name, Fn& outFn) {
  outFn = reinterpret_cast<Fn>(lib.getFunction(name));
  return outFn != nullptr;
}

juce::File currentModuleDirectory() {
  return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
}

bool tryOpen(juce::DynamicLibrary& lib, const juce::File& directory) {
  if (!directory.isDirectory())
    return false;
  const auto candidate = directory.getChildFile(kLibraryFileName);
  if (!candidate.existsAsFile())
    return false;
  return lib.open(candidate.getFullPathName());
}

} // namespace

ProjectMLibrary::~ProjectMLibrary() = default;

ProjectMLibrary::LoadResult ProjectMLibrary::load(const juce::File& bundleDirectoryHint) {
  LoadResult result;
  auto instance = std::unique_ptr<ProjectMLibrary>(new ProjectMLibrary());

  bool opened = false;
  if (bundleDirectoryHint != juce::File())
    opened = tryOpen(instance->library_, bundleDirectoryHint);
  if (!opened)
    opened = tryOpen(instance->library_, currentModuleDirectory());
  if (!opened)
    opened = instance->library_.open(kLibraryFileName);

  if (!opened) {
    result.unavailableReason = std::string("could not locate or load '") + kLibraryFileName +
                                "' (checked the bundle directory, the current module's directory, "
                                "and the platform's default library search path)";
    return result;
  }

  auto& fn = instance->functions_;
  bool allResolved = true;
  std::string missing;
  auto require = [&](const char* name, auto& target) {
    if (!resolveSymbol(instance->library_, name, target)) {
      allResolved = false;
      if (!missing.empty())
        missing += ", ";
      missing += name;
    }
  };

  require("projectm_create", fn.create);
  require("projectm_destroy", fn.destroy);
  require("projectm_load_preset_file", fn.loadPresetFile);
  require("projectm_set_window_size", fn.setWindowSize);
  require("projectm_set_mesh_size", fn.setMeshSize);
  require("projectm_set_fps", fn.setFps);
  require("projectm_set_preset_duration", fn.setPresetDuration);
  require("projectm_set_beat_sensitivity", fn.setBeatSensitivity);
  require("projectm_get_beat_sensitivity", fn.getBeatSensitivity);
  require("projectm_pcm_add_float", fn.pcmAddFloat);
  require("projectm_opengl_render_frame_fbo", fn.openglRenderFrameFbo);
  require("projectm_set_preset_switch_failed_event_callback", fn.setPresetSwitchFailedEventCallback);
  require("projectm_get_version_string", fn.getVersionString);
  require("projectm_free_string", fn.freeString);

  if (!allResolved) {
    result.unavailableReason = "loaded '" + std::string(kLibraryFileName) +
                                "' but it is missing expected symbol(s): " + missing +
                                " (likely an incompatible projectM version)";
    return result;
  }

  const char* rawVersion = fn.getVersionString();
  if (rawVersion == nullptr) {
    result.unavailableReason = "projectm_get_version_string() returned null";
    return result;
  }
  instance->version_ = rawVersion;
  fn.freeString(rawVersion);

  const int majorVersion = [&] {
    const auto dot = instance->version_.find('.');
    const auto majorStr = dot == std::string::npos ? instance->version_ : instance->version_.substr(0, dot);
    try {
      return std::stoi(majorStr);
    } catch (...) {
      return -1;
    }
  }();

  if (majorVersion < kMinimumSupportedMajorVersion) {
    result.unavailableReason = "projectM version " + instance->version_ + " is older than the minimum supported (" +
                                std::to_string(kMinimumSupportedMajorVersion) + ".x)";
    return result;
  }

  result.library = std::move(instance);
  return result;
}

} // namespace milkdawp::engine
