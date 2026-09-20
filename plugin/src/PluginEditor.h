// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "milkdawp/engine/OutputSurface.h"

namespace milkdawp::plugin {

/// Still a Phase 0.2-era skeleton in spirit (no `ControlDrawer`, no
/// video-first layout -- that's Phase 3.3, blocked on the rest of Phase 2's
/// rendering work). What's real here now is `engine::OutputSurface` filling
/// the background: a live instrument for Phase 2.3's GL-context-persistence
/// spike, plus a small diagnostics overlay reporting what it's finding, so
/// that spike can be measured empirically in a real host instead of guessed
/// at (see OutputSurface's and RenderEngine's class comments).
class MilkDAWpAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
  explicit MilkDAWpAudioProcessorEditor(MilkDAWpAudioProcessor&);
  ~MilkDAWpAudioProcessorEditor() override;

  void resized() override;

private:
  void timerCallback() override;

  MilkDAWpAudioProcessor& processorRef;
  engine::OutputSurface outputSurface;
  juce::Label diagnosticsLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MilkDAWpAudioProcessorEditor)
};

} // namespace milkdawp::plugin
