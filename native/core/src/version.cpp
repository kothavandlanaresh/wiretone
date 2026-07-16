#include "wiretone/core/version.hpp"

namespace wiretone::core {

std::string_view version_string() noexcept {
    return "0.1.0";
}

std::string_view product_name() noexcept {
    return "WireTone";
}

} // namespace wiretone::core
