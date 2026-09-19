// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace milkdawp::core {

/// In-place iterative radix-2 Cooley-Tukey FFT (Phase 1.3). `size` must be a
/// power of two. `data` holds `size` complex samples on entry and the
/// (unnormalized) DFT on return, in natural (not bit-reversed) order.
///
/// Own implementation rather than a dependency (§7 Phase 1.3): milkdawp_core
/// has no external dependencies by design (§4.1), and 2048-point transforms
/// at a 512-sample hop are nowhere near performance-critical enough to
/// justify one.
void fft(std::vector<std::complex<float>>& data);

/// Real-input convenience wrapper: copies `real` (size `size`) into a
/// complex buffer with zero imaginary parts and runs fft() in place.
/// `real.size()` must equal `size` and be a power of two.
std::vector<std::complex<float>> fftReal(const std::vector<float>& real);

/// True if `n` is a power of two (n > 0).
[[nodiscard]] constexpr bool isPowerOfTwo(std::size_t n) noexcept { return n > 0 && (n & (n - 1)) == 0; }

} // namespace milkdawp::core
