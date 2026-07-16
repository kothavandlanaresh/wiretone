#pragma once

#include <string_view>

namespace wiretone::core {

inline constexpr int version_major = 0;
inline constexpr int version_minor = 1;
inline constexpr int version_patch = 0;

[[nodiscard]] std::string_view version_string() noexcept;
[[nodiscard]] std::string_view product_name() noexcept;

} // namespace wiretone::core
