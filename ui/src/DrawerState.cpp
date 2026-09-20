// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/ui/DrawerState.h"

namespace milkdawp::ui {

DrawerStateMachine::DrawerStateMachine(Config config)
    : pinned_(config.startPinned), revealed_(config.startPinned || config.firstRunRevealed),
      autoHideSeconds_(config.autoHideSeconds) {}

DrawerVisualState DrawerStateMachine::state() const noexcept {
  if (pinned_) {
    return DrawerVisualState::Pinned;
  }
  return revealed_ ? DrawerVisualState::Revealed : DrawerVisualState::Hidden;
}

void DrawerStateMachine::markFirstInteraction(double nowSeconds) {
  hasHadFirstInteraction_ = true;
  lastActivityTime_ = nowSeconds;
}

void DrawerStateMachine::onPointerActivity(double nowSeconds) {
  markFirstInteraction(nowSeconds);
  revealed_ = true;
}

void DrawerStateMachine::tick(double nowSeconds) {
  if (!hasHadFirstInteraction_ || pinned_ || !revealed_) {
    return;
  }
  if (nowSeconds - lastActivityTime_ >= autoHideSeconds_) {
    revealed_ = false;
  }
}

void DrawerStateMachine::setPinned(bool pinned, double nowSeconds) {
  markFirstInteraction(nowSeconds);
  pinned_ = pinned;
  if (pinned_) {
    revealed_ = true;
  }
}

void DrawerStateMachine::togglePin(double nowSeconds) { setPinned(!pinned_, nowSeconds); }

void DrawerStateMachine::toggleRevealHide(double nowSeconds) {
  markFirstInteraction(nowSeconds);
  if (pinned_) {
    return; // see class comment: unpin first, then hide
  }
  revealed_ = !revealed_;
}

} // namespace milkdawp::ui
