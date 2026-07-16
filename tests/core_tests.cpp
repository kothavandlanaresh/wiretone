#include "wiretone/core/version.hpp"

#include <iostream>
#include <string_view>

namespace {

int fail(std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace wiretone::core;

    if (product_name() != "WireTone") {
        return fail("product_name() returned an unexpected value");
    }

    if (version_string() != "0.1.0") {
        return fail("version_string() returned an unexpected value");
    }

    if (version_major != 0 || version_minor != 1 || version_patch != 0) {
        return fail("numeric version constants do not match 0.1.0");
    }

    std::cout << "PASS: WireTone native foundation tests\n";
    return 0;
}
