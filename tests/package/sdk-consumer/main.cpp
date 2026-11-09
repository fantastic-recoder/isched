// SPDX-License-Identifier: MPL-2.0
#include <isched/isched.hpp>

int main() {
    using namespace isched::v0_0_1::backend;
    set_lifecycle_trace(true);
    if (!is_lifecycle_trace_enabled()) {
        return 1;
    }
    set_lifecycle_trace(false);
    return is_lifecycle_trace_enabled() ? 2 : 0;
}
