// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "milkdawp/core/ParameterModel.h"
#include "milkdawp/core/StateSchema.h"

namespace milkdawp::plugin {

namespace {

core::TransportInfo extractTransportInfo(juce::AudioPlayHead* playHead) {
  core::TransportInfo info; // defaults: not playing, 0 bpm/ppq, 4/4, sample 0
  if (playHead == nullptr) {
    return info;
  }
  const auto position = playHead->getPosition();
  if (!position.hasValue()) {
    return info;
  }
  info.isPlaying = position->getIsPlaying();
  info.bpm = position->getBpm().orFallback(0.0);
  info.ppqPosition = position->getPpqPosition().orFallback(0.0);
  info.timeSigNumerator = position->getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature{}).numerator;
  info.samplePos = static_cast<std::uint64_t>(position->getTimeInSamples().orFallback(int64_t{0}));
  return info;
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout MilkDAWpAudioProcessor::createParameterLayout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

  for (const auto& spec : core::allParameters()) {
    const juce::ParameterID id{juce::String(spec.id), 1};
    switch (spec.type) {
    case core::ParameterType::Float:
      params.push_back(std::make_unique<juce::AudioParameterFloat>(id, spec.displayName, spec.minValue,
                                                                     spec.maxValue, spec.defaultValue));
      break;
    case core::ParameterType::Bool:
      params.push_back(
          std::make_unique<juce::AudioParameterBool>(id, spec.displayName, spec.defaultValue != 0.0f));
      break;
    case core::ParameterType::Int:
      params.push_back(std::make_unique<juce::AudioParameterInt>(id, spec.displayName,
                                                                   static_cast<int>(spec.minValue),
                                                                   static_cast<int>(spec.maxValue),
                                                                   static_cast<int>(spec.defaultValue)));
      break;
    case core::ParameterType::Choice: {
      juce::StringArray choices;
      for (const auto& choice : spec.choices) {
        choices.add(choice);
      }
      params.push_back(std::make_unique<juce::AudioParameterChoice>(id, spec.displayName, choices,
                                                                      static_cast<int>(spec.defaultValue)));
      break;
    }
    }
  }

  return {params.begin(), params.end()};
}

MilkDAWpAudioProcessor::MilkDAWpAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
  renderEngine_ = milkdawp::engine::RenderEngine::create();
  beatSensitivityParam_ = apvts.getRawParameterValue("beatSensitivity");
  transitionDurationParam_ = apvts.getRawParameterValue("transitionDurationSeconds");
}

void MilkDAWpAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  const int numChannels = juce::jmax(1, getTotalNumInputChannels());
  const std::size_t capacityFrames = static_cast<std::size_t>(juce::jmax(samplesPerBlock, 1)) * 8;
  audioRing_ = std::make_unique<core::AudioRing>(capacityFrames, numChannels);
  interleaveScratch_.assign(static_cast<std::size_t>(juce::jmax(samplesPerBlock, 1)) *
                                 static_cast<std::size_t>(numChannels),
                             0.0f);
  hostTransport_ = std::make_unique<core::HostTransport>(sampleRate);
}

void MilkDAWpAudioProcessor::releaseResources() {}

void MilkDAWpAudioProcessor::pushChangedRenderParameters() {
  if (!renderEngine_) {
    return;
  }
  if (beatSensitivityParam_ != nullptr) {
    const float value = beatSensitivityParam_->load(std::memory_order_relaxed);
    if (value != lastPushedBeatSensitivity_) {
      renderEngine_->pushParameterUpdate({engine::RenderEngine::ParameterTarget::BeatSensitivity, value});
      lastPushedBeatSensitivity_ = value;
    }
  }
  if (transitionDurationParam_ != nullptr) {
    const float value = transitionDurationParam_->load(std::memory_order_relaxed);
    if (value != lastPushedTransitionDuration_) {
      renderEngine_->pushParameterUpdate({engine::RenderEngine::ParameterTarget::PresetDurationSeconds, value});
      lastPushedTransitionDuration_ = value;
    }
  }
}

MILKDAWP_NONBLOCKING void MilkDAWpAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  // Bit-exact passthrough (§4.1 goal: zero audio impact) -- buffer is only
  // ever read below, never written to.
  const int numChannels = buffer.getNumChannels();
  const int numSamples = buffer.getNumSamples();

  if (audioRing_ && numSamples > 0 &&
      interleaveScratch_.size() >= static_cast<std::size_t>(numSamples) * static_cast<std::size_t>(numChannels)) {
    for (int ch = 0; ch < numChannels; ++ch) {
      const float* src = buffer.getReadPointer(ch);
      for (int i = 0; i < numSamples; ++i) {
        interleaveScratch_[static_cast<std::size_t>(i) * static_cast<std::size_t>(numChannels) +
                            static_cast<std::size_t>(ch)] = src[i];
      }
    }
    audioRing_->write(interleaveScratch_.data(), static_cast<std::size_t>(numSamples));
  }

  const auto transportInfo = extractTransportInfo(getPlayHead());
  transportSnapshot_.publish(transportInfo);
  if (hostTransport_) {
    beatClockSnapshot_.publish(hostTransport_->processTransport(transportInfo));
  }
  pushChangedRenderParameters();
}

juce::AudioProcessorEditor* MilkDAWpAudioProcessor::createEditor() {
  return new MilkDAWpAudioProcessorEditor(*this);
}

void MilkDAWpAudioProcessor::setEditorSize(int width, int height) noexcept {
  editorWidth_ = width;
  editorHeight_ = height;
}

void MilkDAWpAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  core::StateSchemaV2 state;
  state.editorWidth = editorWidth_;
  state.editorHeight = editorHeight_;
  for (const auto& spec : core::allParameters()) {
    if (auto* raw = apvts.getRawParameterValue(juce::String(spec.id))) {
      state.paramValues[spec.id] = raw->load(std::memory_order_relaxed);
    }
  }

  const auto text = core::serializeStateSchemaV2(state);
  destData.setSize(text.size());
  destData.copyFrom(text.data(), 0, text.size());
}

void MilkDAWpAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  if (data == nullptr || sizeInBytes <= 0) {
    return;
  }
  const std::string text(static_cast<const char*>(data), static_cast<std::size_t>(sizeInBytes));
  const auto state = core::deserializeStateSchemaV2(text);

  for (const auto& [id, value] : state.paramValues) {
    const juce::String jid(id);
    if (auto* param = apvts.getParameter(jid)) {
      const auto range = apvts.getParameterRange(jid);
      param->setValueNotifyingHost(range.convertTo0to1(value));
    }
  }

  if (state.editorWidth > 0 && state.editorHeight > 0) {
    setEditorSize(state.editorWidth, state.editorHeight);
  }
}

} // namespace milkdawp::plugin

// This is what the JUCE plugin wrappers call to create the processor instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new milkdawp::plugin::MilkDAWpAudioProcessor();
}
