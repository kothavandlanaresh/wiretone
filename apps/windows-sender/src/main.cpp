#include "wiretone/core/version.hpp"

#include <iostream>

int main() {
    std::cout << wiretone::core::product_name() << " sender shell "
              << wiretone::core::version_string() << '\n';
    std::cout << "Phase 0 foundation is active. WASAPI capture is not implemented yet.\n";
    return 0;
}
