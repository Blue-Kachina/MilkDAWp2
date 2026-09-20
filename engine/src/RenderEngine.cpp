// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/engine/RenderEngine.h"

namespace milkdawp::engine {

std::unique_ptr<RenderEngine> RenderEngine::create(const Config& config, const juce::File& bundleDirectoryHint) {
  auto loadResult = ProjectMLibrary::load(bundleDirectoryHint);
  return std::unique_ptr<RenderEngine>(
      new RenderEngine(std::move(loadResult.library), std::move(loadResult.unavailableReason), config));
}

RenderEngine::RenderEngine(std::unique_ptr<ProjectMLibrary> library, std::string unavailableReason, Config config)
    : library_(std::move(library)), unavailableReason_(std::move(unavailableReason)), config_(config) {
  if (!library_) {
    return; // unavailableReason_ already carries why; instance_ stays null.
  }

  instance_ = library_->functions().create();
  if (instance_ == nullptr) {
    unavailableReason_ = "projectm_create() returned null";
    library_.reset();
    return;
  }

  const auto& fn = library_->functions();
  fn.setWindowSize(instance_, config_.windowWidth, config_.windowHeight);
  fn.setMeshSize(instance_, config_.meshWidth, config_.meshHeight);
  fn.setFps(instance_, config_.fps);
}

RenderEngine::~RenderEngine() {
  if (instance_ != nullptr && library_) {
    library_->functions().destroy(instance_);
  }
}

bool RenderEngine::pushParameterUpdate(const ParameterUpdate& update) noexcept {
  return parameterQueue_.push(update);
}

void RenderEngine::loadPreset(const std::string& filename, bool smoothTransition) {
  if (!isAvailable()) {
    return;
  }
  library_->functions().loadPresetFile(instance_, filename.c_str(), smoothTransition);
}

void RenderEngine::setPresetSwitchFailedCallback(PresetSwitchFailedCallback callback) {
  presetSwitchFailedCallback_ = std::move(callback);
  if (!isAvailable()) {
    return;
  }
  library_->functions().setPresetSwitchFailedEventCallback(instance_, &RenderEngine::presetSwitchFailedTrampoline,
                                                             this);
}

void RenderEngine::renderFrame(const core::AudioRing& audioRing, unsigned int targetFbo) {
  if (!isAvailable()) {
    return;
  }

  drainParameterUpdates();

  const auto frames = config_.pcmFrameCount;
  const auto channels = audioRing.numChannels();
  pcmScratch_.resize(frames * static_cast<std::size_t>(channels));
  audioRing.copyLatest(pcmScratch_.data(), frames);

  const auto& fn = library_->functions();
  fn.pcmAddFloat(instance_, pcmScratch_.data(), static_cast<std::uint32_t>(frames), channels);
  fn.openglRenderFrameFbo(instance_, targetFbo);
}

void RenderEngine::drainParameterUpdates() {
  while (const auto update = parameterQueue_.pop()) {
    applyParameterUpdate(*update);
  }
}

void RenderEngine::applyParameterUpdate(const ParameterUpdate& update) {
  const auto& fn = library_->functions();
  switch (update.target) {
  case ParameterTarget::BeatSensitivity:
    fn.setBeatSensitivity(instance_, update.value);
    return;
  case ParameterTarget::PresetDurationSeconds:
    fn.setPresetDuration(instance_, static_cast<double>(update.value));
    return;
  }
}

void RenderEngine::presetSwitchFailedTrampoline(const char* filename, const char* message, void* userData) {
  auto* self = static_cast<RenderEngine*>(userData);
  if (self->presetSwitchFailedCallback_) {
    self->presetSwitchFailedCallback_(filename != nullptr ? filename : "", message != nullptr ? message : "");
  }
}

} // namespace milkdawp::engine
