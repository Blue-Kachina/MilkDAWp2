// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "milkdawp/engine/EngineInfo.h"

namespace milkdawp::engine {

bool hasProjectM() noexcept {
#if MILKDAWP_HAS_PROJECTM
  return true;
#else
  return false;
#endif
}

} // namespace milkdawp::engine
