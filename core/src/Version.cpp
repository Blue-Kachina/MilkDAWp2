// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/core/Version.h"

namespace milkdawp::core {

Version version() noexcept { return Version{2, 0, 0}; }

const char* versionString() noexcept { return "2.0.0"; }

} // namespace milkdawp::core
