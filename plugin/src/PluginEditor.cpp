// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"

namespace milkdawp::plugin {

MilkDAWpAudioProcessorEditor::MilkDAWpAudioProcessorEditor(MilkDAWpAudioProcessor& processor)
    : AudioProcessorEditor(&processor), processorRef(processor), outputSurface(processor.renderEngine()) {
  addAndMakeVisible(outputSurface);

  diagnosticsLabel.setJustificationType(juce::Justification::topLeft);
  diagnosticsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  diagnosticsLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.5f));
  // Must be a child of outputSurface, not a sibling: JUCE only composites a
  // GL-attached component's own paint() (and its children's) over the GL
  // content each frame. A sibling Component added at the editor level gets
  // drawn first and then overwritten by the GL surface's buffer swap --
  // exactly the "overlapping sibling peer" risk §4.11 flagged for the
  // drawer, hit here for real on Windows.
  outputSurface.addAndMakeVisible(diagnosticsLabel);

  setResizable(true, true);
  // Reads whatever size was last persisted on the processor (defaults to
  // 480x270 if none was -- see MilkDAWpAudioProcessor::editorWidth_). This
  // is what makes editor-size persistence immune to host construction
  // order: the size lives on the processor, not applied imperatively to an
  // editor that might not exist yet when setStateInformation runs (§2.9's
  // Cubase lesson).
  setSize(processorRef.editorWidth(), processorRef.editorHeight());

  timerCallback(); // show correct text immediately, not just after the first tick
  startTimerHz(2);
}

MilkDAWpAudioProcessorEditor::~MilkDAWpAudioProcessorEditor() { stopTimer(); }

void MilkDAWpAudioProcessorEditor::resized() {
  outputSurface.setBounds(getLocalBounds());
  // Relative to outputSurface's own local bounds now that it's the parent.
  diagnosticsLabel.setBounds(outputSurface.getLocalBounds().removeFromTop(80).reduced(8));
  processorRef.setEditorSize(getWidth(), getHeight());
}

void MilkDAWpAudioProcessorEditor::timerCallback() {
  auto& engine = processorRef.renderEngine();
  juce::String text;
  text << "GL context created " << engine.glContextCreationCount() << " time(s) since plugin load.\n";
  text << "projectM: " << (engine.isAvailable() ? "available" : juce::String("unavailable (" + engine.unavailableReason() + ")"));
  diagnosticsLabel.setText(text, juce::dontSendNotification);
}

} // namespace milkdawp::plugin
