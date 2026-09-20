// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/engine/TransitionExecutor.h"

namespace milkdawp::engine {

std::int64_t TransitionExecutor::issueSampleFor(const core::TransitionRequestMessage& request) const noexcept {
  if (request.cutStyle == core::CutStyle::Soft) {
    const auto earlyBySamples = static_cast<std::int64_t>((request.blendSeconds * 0.5) * sampleRate_);
    return request.dueAtSample - earlyBySamples;
  }
  return request.dueAtSample;
}

void TransitionExecutor::drainQueueIntoPending() {
  while (const auto request = queue_.pop()) {
    pending_.push_back(*request);
  }
}

} // namespace milkdawp::engine
