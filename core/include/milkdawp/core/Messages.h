// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

// POD message types crossing threads (§4.2) and the lock-free queue template
// that carries them. Every message type here is trivially copyable and
// contains no owning pointers, `juce::String`, or STL containers: strings
// cross threads only as interned preset IDs (a plain std::uint32_t handle
// into a table the message-thread-owned PresetLibrary maintains), never as
// heap-allocated text.

namespace milkdawp::core {

enum class CutStyle : std::uint8_t { Hard = 0, Soft = 1 };

/// UI/message thread -> analysis/render thread: a parameter changed.
struct ParameterChangeMessage {
  std::uint32_t parameterId;
  float value;
};
static_assert(std::is_trivially_copyable_v<ParameterChangeMessage>);

/// Analysis thread (TransitionScheduler) -> render thread: execute this
/// transition when the render thread's current frame reaches dueAtSample.
struct TransitionRequestMessage {
  std::uint32_t presetId;
  CutStyle cutStyle;
  float blendSeconds;
  std::int64_t dueAtSample;
};
static_assert(std::is_trivially_copyable_v<TransitionRequestMessage>);

/// Preset I/O thread -> render/message thread: the result of loading (or
/// failing to load) a preset, and how long it took.
struct PresetLoadResultMessage {
  std::uint32_t presetId;
  bool success;
  std::uint32_t loadTimeMicros;
};
static_assert(std::is_trivially_copyable_v<PresetLoadResultMessage>);

/// Render/analysis thread -> message thread: a snapshot for UI polling.
/// The message thread reads this via a lock-free read, never by blocking on
/// any other thread (§4.2).
struct StatusSnapshotMessage {
  float bpm;
  float beatConfidence;
  std::int64_t nextBeatSample;
  std::uint32_t currentPresetId;
};
static_assert(std::is_trivially_copyable_v<StatusSnapshotMessage>);

/// Fixed-capacity, lock-free single-producer/single-consumer queue of
/// trivially-copyable messages. Capacity must be a power of two.
///
/// Multiple producers are not supported by this template (see §4.2: each
/// thread pair in the architecture -- UI->engine, scheduler->render,
/// preset-IO->message -- is a single producer to a single consumer; nothing
/// in the current design needs true MPSC).
template <typename T, std::size_t Capacity> class SpscQueue {
  static_assert(std::is_trivially_copyable_v<T>, "SpscQueue only carries trivially-copyable messages");
  static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
  /// Producer only. Returns false (and does nothing) if the queue is full.
  bool push(const T& value) noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t nextHead = (head + 1) & indexMask;
    if (nextHead == tail_.load(std::memory_order_acquire)) {
      return false; // full
    }
    slots_[head] = value;
    head_.store(nextHead, std::memory_order_release);
    return true;
  }

  /// Consumer only. Returns std::nullopt if the queue is empty.
  std::optional<T> pop() noexcept {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return std::nullopt; // empty
    }
    T value = slots_[tail];
    tail_.store((tail + 1) & indexMask, std::memory_order_release);
    return value;
  }

  [[nodiscard]] bool emptyHint() const noexcept {
    return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
  }

private:
  static constexpr std::size_t indexMask = Capacity - 1;

  std::array<T, Capacity> slots_{};
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
};

} // namespace milkdawp::core
