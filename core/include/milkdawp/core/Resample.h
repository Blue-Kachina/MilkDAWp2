// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <vector>

namespace milkdawp::core {

/// Linear-interpolation resampler (Phase 1.3) from `inputRate` to
/// `outputRate`. Used to bring arbitrary source material to the Analyzer's
/// fixed internal rate (§4.3: "44.1/48 kHz passthrough; others resampled")
/// so the tuning constants in OnsetDetector/TempoTracker stay valid
/// regardless of the source file's sample rate.
///
/// Linear interpolation is not the highest-fidelity resampler available, but
/// it is simple, deterministic, and dependency-free, which matches
/// milkdawp_core's constraints (§4.1); it can be swapped for something
/// higher-order later without changing this function's signature.
[[nodiscard]] std::vector<float> resampleLinear(const std::vector<float>& input, double inputRate,
                                                 double outputRate);

} // namespace milkdawp::core
