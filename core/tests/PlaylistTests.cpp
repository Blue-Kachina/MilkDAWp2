// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <deque>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Playlist.h"

using namespace milkdawp::core;

namespace {
class TempPresetFolder {
public:
  TempPresetFolder() {
    root_ = std::filesystem::temp_directory_path() /
            std::filesystem::path("milkdawp_playlist_test_" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root_ / "subdir");

    write(root_ / "b.milk");
    write(root_ / "a.milk");
    write(root_ / "subdir" / "c.milk");
    write(root_ / "not_a_preset.txt");
  }

  ~TempPresetFolder() { std::filesystem::remove_all(root_); }

  [[nodiscard]] std::string path() const { return root_.string(); }

private:
  static void write(const std::filesystem::path& p) { std::ofstream(p) << "dummy"; }
  std::filesystem::path root_;
};
} // namespace

TEST_CASE("Playlist::scanFolder finds only .milk files, recursively, in stable order",
          "[core][Playlist]") {
  TempPresetFolder folder;
  auto entries = Playlist::scanFolder(folder.path());

  REQUIRE(entries.size() == 3);
  CHECK(entries[0].relativePath == "a.milk");
  CHECK(entries[1].relativePath == "b.milk");
  CHECK(entries[2].relativePath == std::filesystem::path("subdir") / "c.milk");

  // Rescanning gives the exact same order.
  auto rescan = Playlist::scanFolder(folder.path());
  REQUIRE(rescan.size() == entries.size());
  for (std::size_t i = 0; i < entries.size(); ++i) {
    CHECK(rescan[i].relativePath == entries[i].relativePath);
  }
}

TEST_CASE("Playlist::scanFolder on a nonexistent folder returns empty", "[core][Playlist]") {
  CHECK(Playlist::scanFolder("Z:/definitely/does/not/exist/milkdawp").empty());
}

namespace {
std::vector<PlaylistEntry> makeEntries(std::size_t n) {
  std::vector<PlaylistEntry> entries;
  for (std::size_t i = 0; i < n; ++i) {
    entries.push_back({"/presets/" + std::to_string(i) + ".milk", std::to_string(i) + ".milk", 1.0f});
  }
  return entries;
}
} // namespace

TEST_CASE("Playlist sequential policy wraps in both directions", "[core][Playlist]") {
  Playlist playlist(makeEntries(3));
  playlist.setPolicy(PlaylistPolicy::Sequential);
  std::mt19937 rng(1);

  CHECK(playlist.currentIndex() == 0);
  CHECK(playlist.advanceNext(rng) == 1);
  CHECK(playlist.advanceNext(rng) == 2);
  CHECK(playlist.advanceNext(rng) == 0); // wraps forward
  CHECK(playlist.advancePrevious() == 2); // history-based: back to where we came from
  CHECK(playlist.advancePrevious() == 1);
}

TEST_CASE("Playlist lock prevents both advanceNext and advancePrevious", "[core][Playlist]") {
  Playlist playlist(makeEntries(4));
  playlist.setLocked(true);
  std::mt19937 rng(1);

  CHECK(playlist.advanceNext(rng) == 0);
  playlist.setCurrentIndex(2); // explicit selection still works while locked
  CHECK(playlist.advanceNext(rng) == 2);
  CHECK(playlist.advancePrevious() == 2);
}

TEST_CASE("Playlist setCurrentIndex clamps out-of-range values", "[core][Playlist]") {
  Playlist playlist(makeEntries(3));
  playlist.setCurrentIndex(999);
  CHECK(playlist.currentIndex() == 2);
}

TEST_CASE("Playlist ShuffleNoRepeat never repeats within the history window",
          "[core][Playlist]") {
  Playlist playlist(makeEntries(10));
  playlist.setPolicy(PlaylistPolicy::ShuffleNoRepeat);
  playlist.setHistoryWindowSize(5);
  std::mt19937 rng(42);

  std::deque<std::size_t> recent;
  for (int i = 0; i < 200; ++i) {
    auto next = playlist.advanceNext(rng);
    // The newly-picked index must not equal any of the last 5 picks.
    for (auto r : recent) {
      CHECK(next != r);
    }
    recent.push_back(next);
    if (recent.size() > 5) {
      recent.pop_front();
    }
  }
}

TEST_CASE("Playlist ShuffleNoRepeat visits every entry given enough advances",
          "[core][Playlist]") {
  Playlist playlist(makeEntries(5));
  playlist.setPolicy(PlaylistPolicy::ShuffleNoRepeat);
  playlist.setHistoryWindowSize(2);
  std::mt19937 rng(7);

  std::set<std::size_t> visited;
  for (int i = 0; i < 100; ++i) {
    visited.insert(playlist.advanceNext(rng));
  }
  CHECK(visited.size() == 5);
}

TEST_CASE("Playlist Weighted policy strongly favors a much higher weight",
          "[core][Playlist]") {
  auto entries = makeEntries(3);
  entries[0].weight = 1000.0f;
  entries[1].weight = 1.0f;
  entries[2].weight = 1.0f;
  Playlist playlist(std::move(entries));
  playlist.setPolicy(PlaylistPolicy::Weighted);
  playlist.setHistoryWindowSize(0); // allow immediate repeats so weighting alone is measured
  std::mt19937 rng(123);

  std::size_t heavyPicks = 0;
  constexpr int trials = 500;
  for (int i = 0; i < trials; ++i) {
    if (playlist.advanceNext(rng) == 0) {
      ++heavyPicks;
    }
  }

  CHECK(heavyPicks > trials * 0.9);
}

TEST_CASE("Playlist advancePrevious retraces actual play history, not just index-1",
          "[core][Playlist]") {
  Playlist playlist(makeEntries(5));
  playlist.setPolicy(PlaylistPolicy::ShuffleNoRepeat);
  std::mt19937 rng(99);

  std::vector<std::size_t> path{playlist.currentIndex()};
  for (int i = 0; i < 5; ++i) {
    path.push_back(playlist.advanceNext(rng));
  }

  // path.back() is where we are now; walk backward through path[size-2..0].
  for (std::size_t stepsBack = 1; stepsBack < path.size(); ++stepsBack) {
    const std::size_t expected = path[path.size() - 1 - stepsBack];
    CHECK(playlist.advancePrevious() == expected);
  }
}
