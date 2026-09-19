// SPDX-FileCopyrightText: 2026 The MilkDAWp contributors
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "milkdawp/core/Version.h"

TEST_CASE("version() matches the project version", "[core][version]") {
  const auto v = milkdawp::core::version();
  CHECK(v.major == 2);
  CHECK(v.minor == 0);
  CHECK(v.patch == 0);
}

TEST_CASE("versionString() matches version()", "[core][version]") {
  CHECK(std::string(milkdawp::core::versionString()) == "2.0.0");
}
