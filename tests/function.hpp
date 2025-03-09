#pragma once

#include "tests.hpp"

namespace tests {

// Test signature compliance
postgres_function(_sig1, ([] { return true; }));
postgres_function(_sig2, ([](std::optional<bool> v) { return v; }));
postgres_function(_sig3, ([](bool v) { return v; }));
postgres_function(_sig4, ([](bool v, std::optional<int64_t> n) { return v && n.has_value(); }));
postgres_function(_void_sig_fun, ([] {}));

} // namespace tests
