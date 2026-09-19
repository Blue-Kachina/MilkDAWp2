// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace milkdawp::plugin {

MilkDAWpAudioProcessor::MilkDAWpAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

void MilkDAWpAudioProcessor::prepareToPlay(double, int) {}

void MilkDAWpAudioProcessor::releaseResources() {}

void MilkDAWpAudioProcessor::processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) {
  // Bit-exact passthrough (§4.1 goal: zero audio impact). Real analysis-ring
  // writes land in Phase 3.1; this skeleton intentionally does nothing.
}

juce::AudioProcessorEditor* MilkDAWpAudioProcessor::createEditor() {
  return new MilkDAWpAudioProcessorEditor(*this);
}

void MilkDAWpAudioProcessor::getStateInformation(juce::MemoryBlock&) {
  // StateSchema v2 (Phase 1.14) and v1 migration land in Phase 3.2.
}

void MilkDAWpAudioProcessor::setStateInformation(const void*, int) {}

} // namespace milkdawp::plugin

// This is what the JUCE plugin wrappers call to create the processor instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new milkdawp::plugin::MilkDAWpAudioProcessor();
}
