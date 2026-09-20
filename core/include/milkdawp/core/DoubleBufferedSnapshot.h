// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <type_traits>

namespace milkdawp::core {

/// Single-writer, multi-reader lock-free snapshot publisher (§4.2: "writes
/// transport info into an atomic snapshot"). Not implemented as
/// `std::atomic<T>` directly because most real snapshot payloads (e.g.
/// `TransportInfo`) are larger than the platform's lock-free CAS width, so
/// `std::atomic<T>` would silently fall back to an internal lock -- exactly
/// what the audio thread must never touch. Instead the writer publishes into
/// one of two buffers and flips a single `int` index, which *is* always
/// lock-free; a reader either sees the old or the new snapshot in full,
/// never a torn mix of the two.
///
/// Only one thread may call publish(); any number of threads may call
/// read() concurrently with it and with each other.
template <typename T> class DoubleBufferedSnapshot {
  static_assert(std::is_trivially_copyable_v<T>, "DoubleBufferedSnapshot only carries trivially-copyable payloads");

public:
  /// Writer thread only. Never allocates, never blocks.
  void publish(const T& value) noexcept {
    const int writeIndex = 1 - activeIndex_.load(std::memory_order_relaxed);
    buffers_[static_cast<std::size_t>(writeIndex)] = value;
    activeIndex_.store(writeIndex, std::memory_order_release);
  }

  /// Any thread. Returns the most recently published value (or a
  /// default-constructed T if publish() has never been called).
  [[nodiscard]] T read() const noexcept {
    return buffers_[static_cast<std::size_t>(activeIndex_.load(std::memory_order_acquire))];
  }

private:
  std::array<T, 2> buffers_{};
  std::atomic<int> activeIndex_{0};
};

} // namespace milkdawp::core
