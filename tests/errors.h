#pragma once

#include "tests.h"

namespace tests {

add_test(catch_error, ([](test_case &) {
           try {
             cppgres::ffi_guarded(::get_role_oid)("this_role_does_not_exist", false);
             _assert(false);
           } catch (cppgres::pg_exception &e) {
             return true;
           }
           return false;
         }));

postgres_function(raise_exception,
                  ([]() -> bool { throw std::runtime_error("raised an exception"); }));

add_test(exception_to_error, ([](test_case &) {
           bool result = false;
           cppgres::spi_executor spi;
           auto stmt = std::format(
               "create or replace function raise_exception() returns bool language 'c' as '{}'",
               get_library_name());
           spi.execute(stmt);
           cppgres::internal_subtransaction sub(false);
           try {
             spi.execute("select raise_exception()");
           } catch (cppgres::pg_exception &e) {
             result = _assert(std::string_view(e.message()) == "exception: raised an exception");
           }
           return result;
         }));

postgres_function(produce_error, ([]() {
                    cppgres::ffi_guarded(::get_role_oid)("this_role_does_not_exist", false);
                    _assert(false);
                    return false;
                  }));

add_test(handle_produced_error, ([](test_case &) {
           bool result = false;
           cppgres::spi_executor spi;
           auto stmt = std::format(
               "create or replace function produce_error() returns bool language 'c' as '{}'",
               get_library_name());
           spi.execute(stmt);
           cppgres::internal_subtransaction sub(false);
           try {
             spi.execute("select produce_error()");
           } catch (cppgres::pg_exception &e) {
             result = _assert(std::string_view(e.message()) ==
                              R"(role "this_role_does_not_exist" does not exist)");
           }
           return result;
         }));
} // namespace tests
