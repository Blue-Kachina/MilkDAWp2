// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"

namespace milkdawp::plugin {

MilkDAWpAudioProcessorEditor::MilkDAWpAudioProcessorEditor(MilkDAWpAudioProcessor& processor)
    : AudioProcessorEditor(&processor), processorRef(processor) {
  placeholderLabel.setText("MilkDAWp 2 (Phase 0 skeleton)", juce::dontSendNotification);
  placeholderLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(placeholderLabel);
  setResizable(true, true);
  setSize(480, 270);
}

void MilkDAWpAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MilkDAWpAudioProcessorEditor::resized() { placeholderLabel.setBounds(getLocalBounds()); }

} // namespace milkdawp::plugin
