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

add_test(srf, ([](test_case &) {
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

struct srf_pfr_res {
  int32_t a, b;
};

postgres_function(srf_pfr, ([]() {
                    std::array<srf_pfr_res, 3> results{{{1, 10}, {2, 20}, {3, 30}}};
                    return results;
                  }));

add_test(
    srf_pfr, ([](test_case &) {
      bool result = true;
      cppgres::spi_executor spi;
      auto stmt = std::format(
          "create or replace function srf_pfr() returns table (a int, b int) language 'c' as '{}'",
          get_library_name());
      spi.execute(stmt);
      auto res = spi.query<srf_pfr_res>("select * from srf_pfr()");
      result = result && _assert(res.begin()[1].b == 20);
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
           cppgres::internal_subtransaction sub(false);
           try {
             spi.query<std::tuple<int32_t, int32_t>>("select non_srf()");
           } catch (cppgres::pg_exception &e) {
             exception_raised =
                 _assert(std::string_view(e.message()) == "caller is not expecting a set");
           }
           result = result && exception_raised;
           return result;
         }));

add_test(srf_mismatch_size, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto stmt = std::format("create or replace function srf_mismatch_size() returns table "
                                   "(a int) language 'c' as '{}', 'srf'",
                                   get_library_name());
           spi.execute(stmt);
           bool exception_raised = false;
           cppgres::internal_subtransaction sub(false);
           try {
             spi.query<std::tuple<int32_t, int32_t>>("select * from srf_mismatch_size()");
           } catch (cppgres::pg_exception &e) {
             exception_raised = true;
           }
           result = result && exception_raised;
           return result;
         }));

add_test(srf_mismatch_types, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto stmt = std::format("create or replace function srf_mismatch_types() returns table "
                                   "(a int, b text) language 'c' as '{}', 'srf'",
                                   get_library_name());
           spi.execute(stmt);
           bool exception_raised = false;
           cppgres::internal_subtransaction sub(false);
           try {
             spi.query<std::tuple<int32_t, int32_t>>("select * from srf_mismatch_types()");
           } catch (cppgres::pg_exception &e) {
             exception_raised = true;
           }
           result = result && exception_raised;
           return result;
         }));

} // namespace tests
