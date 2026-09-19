// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <juce_gui_extra/juce_gui_extra.h>

namespace milkdawp::app {

/// Skeleton app shell (Phase 0.2). Phase 4 replaces this with the video-first
/// main window, device input, MIDI learn, and preferences from §4.9/§7 Phase 4.
class MilkDAWpApplication final : public juce::JUCEApplication {
public:
  const juce::String getApplicationName() override { return "MilkDAWp"; }
  const juce::String getApplicationVersion() override { return "2.0.0"; }
  bool moreThanOneInstanceAllowed() override { return false; }

  void initialise(const juce::String&) override {
    mainWindow = std::make_unique<MainWindow>(getApplicationName());
  }

  void shutdown() override { mainWindow = nullptr; }

private:
  class MainWindow final : public juce::DocumentWindow {
  public:
    explicit MainWindow(const juce::String& name)
        : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons) {
      setUsingNativeTitleBar(true);
      setContentOwned(new juce::Label({}, "MilkDAWp 2 (Phase 0 skeleton)"), true);
      centreWithSize(480, 270);
      setVisible(true);
    }

    void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
  };

  std::unique_ptr<MainWindow> mainWindow;
};

} // namespace milkdawp::app

START_JUCE_APPLICATION(milkdawp::app::MilkDAWpApplication)
