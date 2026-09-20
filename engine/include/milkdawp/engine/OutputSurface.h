// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

#include "milkdawp/engine/RenderEngine.h"

namespace milkdawp::engine {

/// The embedded primary-window half of Phase 2.4's `OutputSurface`, and the
/// live instrument for Phase 2.3's context-persistence spike (see
/// `RenderEngine`'s class comment). A thin `Component` that attaches
/// `RenderEngine`'s persistent `OpenGLContext` to itself on construction and
/// detaches -- never destroys -- it on destruction, so creating/destroying
/// this `Component` (exactly what happens every time a plugin editor opens
/// and closes) does not by itself tear down GL state the way v1's
/// editor-owned canvas did (§2.4).
///
/// Currently renders a simple time-based colour cycle rather than a real
/// projectM frame: there is no build with projectM actually present to test
/// against yet on any machine this has run on, so wiring `RenderEngine::
/// renderFrame()` in here is a follow-up once that's available. The colour
/// cycle exists purely so there is something visibly alive on screen -- the
/// concrete, human-checkable signal for "does the context survive editor
/// close/reopen" that this spike needs.
class OutputSurface final : public juce::Component, private juce::OpenGLRenderer {
public:
  explicit OutputSurface(RenderEngine& engine);
  ~OutputSurface() override;

private:
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;

  RenderEngine& engine_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputSurface)
};

} // namespace milkdawp::engine
