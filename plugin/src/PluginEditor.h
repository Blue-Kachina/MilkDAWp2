// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace milkdawp::plugin {

/// Skeleton editor (Phase 0.2). Phase 3.3 replaces this with the video-first
/// OutputSurface + ControlDrawer described in development_roadmap.md §4.9.
class MilkDAWpAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
  explicit MilkDAWpAudioProcessorEditor(MilkDAWpAudioProcessor&);

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  MilkDAWpAudioProcessor& processorRef;
  juce::Label placeholderLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MilkDAWpAudioProcessorEditor)
};

} // namespace milkdawp::plugin
