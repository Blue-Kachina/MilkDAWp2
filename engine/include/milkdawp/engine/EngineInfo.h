// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

namespace milkdawp::engine {

/// True when this build was configured with MILKDAWP_WITH_PROJECTM and a
/// projectM CMake target was actually found (see cmake/ProjectMDependency.cmake).
/// RenderEngine/ProjectMLibrary (Phase 2) use this to report Unavailable{reason}
/// instead of failing to link.
[[nodiscard]] bool hasProjectM() noexcept;

} // namespace milkdawp::engine
