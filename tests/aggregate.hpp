#pragma once

#include "tests.hpp"

namespace tests {

struct datum_state_aggregate_test {
  using type = std::optional<int64_t>;
  int64_t val;
  explicit datum_state_aggregate_test(type &&t) : val(t.value_or(0)) {}
  datum_state_aggregate_test &state_transition(int64_t i) {
    val += i;
    return *this;
  }
  operator type() { return val; }
};

postgres_function(aggtest, (cppgres::aggregate_function<datum_state_aggregate_test, int64_t>));

add_test(simple_agg, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           spi.execute(cppgres::fmt::format(
               "create function aggtest(bigint, bigint) returns bigint language c as '{}'",
               get_library_name()));
           spi.execute("create aggregate aggtest (bigint) (sfunc = aggtest, stype = bigint)");
           auto res = spi.query<int64_t>("select aggtest(i) from generate_series(1,1000) i");
           result = result && _assert(res.begin()[0] == 500500);
           return result;
         }));

} // namespace tests
