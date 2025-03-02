#pragma once

#include <ranges>
#include <tuple>

#include <ranges>
#include <tuple>

namespace tests {

postgres_function(srf, ([]() {
                    std::array<std::tuple<int32_t, int32_t>, 3> tuples{{{1, 10}, {2, 20}, {3, 30}}};
                    return tuples;
                  }));

add_test(srf_smoke_test, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto stmt = std::format(
               "create or replace function srf() returns table (a int, b int) language 'c' as '{}'",
               get_library_name());
           spi.execute(stmt);
           auto res = spi.query<std::tuple<int32_t, int32_t>>("select * from srf()");
           result = result && _assert(std::get<1>(res.begin()[1]) == 20);
           return result;
         }));

add_test(srf_non_srf, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto stmt = std::format(
               "create or replace function non_srf() returns int language 'c' as '{}', 'srf'",
               get_library_name());
           spi.execute(stmt);
           bool exception_raised = false;
           try {
             spi.query<std::tuple<int32_t, int32_t>>("select non_srf()");
           } catch (cppgres::pg_exception &e) {
             exception_raised = _assert(std::string_view(e.message()) ==
                                        "caller is not expecting a set");
           }
           result = result && exception_raised;
           return result;
         }));

} // namespace tests
