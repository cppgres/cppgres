#pragma once

#include "tests.hpp"

namespace tests {

// Test signature compliance
postgres_function(_sig1, ([] { return true; }));
postgres_function(_sig2, ([](std::optional<bool> v) { return v; }));
postgres_function(_sig3, ([](bool v) { return v; }));
postgres_function(_sig4, ([](bool v, std::optional<int64_t> n) { return v && n.has_value(); }));
postgres_function(_void_sig_fun, ([] {}));

postgres_function(cstring_fun, ([](const char *s) { return std::string_view(s); }));
postgres_function(to_cstring_fun, ([](std::string_view s) { return s.data(); }));

add_test(cstring_fun_test, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           spi.execute(
               std::format("create function cstring_fun(cstring) returns text language c as '{}'",
                           get_library_name()));
           spi.execute(std::format(
               "create function to_cstring_fun(text) returns cstring language c as '{}'",
               get_library_name()));

           {
             auto res = spi.query<cppgres::text>("select cstring_fun($1)", "hello");
             result = result && _assert(res.begin()[0].operator std::string_view() == "hello");
           }

           {
             auto res =
                 spi.query<const char *>("select to_cstring_fun($1)", std::string_view("hello"));
             result = result && _assert(strncmp(res.begin()[0], "hello", 5) == 0);
           }

           return result;
         }));

postgres_function(_byte_array_sig, ([](const cppgres::byte_array) { return true; }));

postgres_function(infer, ([](std::string_view s) { return s; }));

add_test(syscache_type_inference, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           spi.execute(std::format("create function infer(text) returns text language c as '{}'",
                                   get_library_name()));
           auto func_oid = spi.query<cppgres::oid>("select 'infer'::regproc").begin()[0];
           auto v = cppgres::datum_conversion<std::string_view>::from_datum(
               cppgres::datum(cppgres::ffi_guarded(::OidFunctionCall1Coll)(
                   func_oid, 0, PointerGetDatum(::cstring_to_text("test")))),
               cppgres::memory_context());
           result = result && _assert(v == "test");
           return result;
         }));
} // namespace tests
