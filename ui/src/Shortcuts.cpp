// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/ui/Shortcuts.h"

namespace milkdawp::ui {

namespace {
bool isUnmodified(const juce::KeyPress& key) {
  const auto mods = key.getModifiers();
  return !mods.isCommandDown() && !mods.isCtrlDown() && !mods.isAltDown();
}
} // namespace

ShortcutAction mapKeyPress(const juce::KeyPress& key, bool isAppShell) noexcept {
  if (key.getKeyCode() == juce::KeyPress::F11Key) {
    return ShortcutAction::ToggleFullscreen;
  }
  if (key.getKeyCode() == juce::KeyPress::escapeKey) {
    return ShortcutAction::ExitFullscreenOrRevealDrawer;
  }
  if (key.getKeyCode() == juce::KeyPress::leftKey) {
    return ShortcutAction::PreviousPreset;
  }
  if (key.getKeyCode() == juce::KeyPress::rightKey) {
    return ShortcutAction::NextPreset;
  }
  if (isAppShell && key.getKeyCode() == juce::KeyPress::spaceKey) {
    return ShortcutAction::ToggleLock;
  }

  if (isUnmodified(key)) {
    switch (key.getKeyCode()) {
    case 'L':
      return ShortcutAction::ToggleLock;
    case 'S':
      return ShortcutAction::ToggleShuffle;
    case 'H':
      return ShortcutAction::ToggleDrawer;
    case 'P':
      return ShortcutAction::TogglePin;
    default:
      break;
    }
  }

  return ShortcutAction::None;
}

} // namespace milkdawp::ui
