// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/ui/DrawerState.h"

using namespace milkdawp::ui;

TEST_CASE("DrawerStateMachine starts revealed and stays open past autoHideSeconds before any interaction",
          "[ui][DrawerState]") {
  DrawerStateMachine drawer({/*startPinned=*/false, /*firstRunRevealed=*/true, /*autoHideSeconds=*/3.0});
  CHECK(drawer.state() == DrawerVisualState::Revealed);

  drawer.tick(100.0); // long past 3s, but no interaction has happened yet
  CHECK(drawer.state() == DrawerVisualState::Revealed);
}

TEST_CASE("DrawerStateMachine auto-hides autoHideSeconds after the first pointer activity",
          "[ui][DrawerState]") {
  DrawerStateMachine drawer({false, true, 3.0});

  drawer.onPointerActivity(10.0);
  CHECK(drawer.state() == DrawerVisualState::Revealed);

  drawer.tick(12.0); // 2s elapsed
  CHECK(drawer.state() == DrawerVisualState::Revealed);

  drawer.tick(13.5); // 3.5s elapsed
  CHECK(drawer.state() == DrawerVisualState::Hidden);
}

TEST_CASE("DrawerStateMachine pointer activity resets the auto-hide countdown", "[ui][DrawerState]") {
  DrawerStateMachine drawer({false, true, 3.0});

  drawer.onPointerActivity(0.0);
  drawer.tick(2.9);
  CHECK(drawer.state() == DrawerVisualState::Revealed);

  drawer.onPointerActivity(2.9); // renewed just before it would have hidden
  drawer.tick(5.0);              // only 2.1s since the renewed activity
  CHECK(drawer.state() == DrawerVisualState::Revealed);

  drawer.tick(6.0); // 3.1s since renewed activity
  CHECK(drawer.state() == DrawerVisualState::Hidden);
}

TEST_CASE("DrawerStateMachine defaults to pinned when configured, and never auto-hides while pinned",
          "[ui][DrawerState]") {
  DrawerStateMachine drawer({/*startPinned=*/true, true, 3.0});
  CHECK(drawer.state() == DrawerVisualState::Pinned);
  CHECK(drawer.isPinned());

  drawer.onPointerActivity(0.0);
  drawer.tick(1000.0);
  CHECK(drawer.state() == DrawerVisualState::Pinned);
}

TEST_CASE("DrawerStateMachine::toggleRevealHide is a no-op while pinned", "[ui][DrawerState]") {
  DrawerStateMachine drawer({true, true, 3.0});
  drawer.toggleRevealHide(0.0);
  CHECK(drawer.state() == DrawerVisualState::Pinned);
}

TEST_CASE("DrawerStateMachine::toggleRevealHide hides and reveals once unpinned", "[ui][DrawerState]") {
  DrawerStateMachine drawer({false, true, 3.0});
  drawer.onPointerActivity(0.0); // consume first-interaction guard, revealed
  CHECK(drawer.state() == DrawerVisualState::Revealed);

  drawer.toggleRevealHide(0.1);
  CHECK(drawer.state() == DrawerVisualState::Hidden);

  drawer.toggleRevealHide(0.2);
  CHECK(drawer.state() == DrawerVisualState::Revealed);
}

TEST_CASE("DrawerStateMachine::setPinned(false) starts the auto-hide countdown fresh", "[ui][DrawerState]") {
  DrawerStateMachine drawer({true, true, 3.0});
  CHECK(drawer.state() == DrawerVisualState::Pinned);

  drawer.setPinned(false, 100.0);
  CHECK(drawer.state() == DrawerVisualState::Revealed); // unpinning doesn't hide immediately

  drawer.tick(102.0); // 2s since unpin
  CHECK(drawer.state() == DrawerVisualState::Revealed);

  drawer.tick(103.5); // 3.5s since unpin
  CHECK(drawer.state() == DrawerVisualState::Hidden);
}

TEST_CASE("DrawerStateMachine::togglePin flips pin state and reveals when pinning", "[ui][DrawerState]") {
  DrawerStateMachine drawer({false, false, 3.0}); // no first-run reveal, so it starts Hidden
  CHECK(drawer.state() == DrawerVisualState::Hidden);

  drawer.togglePin(0.0);
  CHECK(drawer.state() == DrawerVisualState::Pinned);

  drawer.togglePin(0.0);
  CHECK(drawer.state() == DrawerVisualState::Revealed); // unpinning keeps it visible, countdown starts
}

TEST_CASE("DrawerStateMachine with firstRunRevealed=false starts hidden", "[ui][DrawerState]") {
  DrawerStateMachine drawer({false, false, 3.0});
  CHECK(drawer.state() == DrawerVisualState::Hidden);
  CHECK_FALSE(drawer.isVisible());
}
