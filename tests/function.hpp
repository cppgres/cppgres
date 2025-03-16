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
               cppgres::datum(cppgres::ffi_guard{::OidFunctionCall1Coll}(
                   func_oid, 0, PointerGetDatum(::cstring_to_text("test")))),
               cppgres::memory_context());
           result = result && _assert(v == "test");
           return result;
         }));

add_test(syscache_type_inference_priority, ([](test_case &) {
           // This one ensures that we don't impose the number of args called but the number
           // of args receivable when there's lack of typing information.
           //
           // This is the case with things like "input functions" – regardless of the signature,
           // `InputFunctionCall` will supply three argument and there are no types given.
           //
           // We are effectively simulating this here.
           bool result = true;
           cppgres::spi_executor spi;
           spi.execute(std::format("create function infer(text) returns text language c as '{}'",
                                   get_library_name()));
           auto func_oid = spi.query<cppgres::oid>("select 'infer'::regproc").begin()[0];
           auto v = cppgres::datum_conversion<std::string_view>::from_datum(
               cppgres::datum(cppgres::ffi_guard{::OidFunctionCall2Coll}(
                   func_oid, 0, PointerGetDatum(::cstring_to_text("test")), cppgres::datum(0))),
               cppgres::memory_context());
           result = result && _assert(v == "test");
           return result;
         }));

add_test(enforce_return_type, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;

           bool exception_raised = false;
           spi.execute(std::format("create function _sig1() returns int language c as '{}'",
                                   get_library_name()));
           {
             cppgres::internal_subtransaction xact(false);
             try {
               spi.query<int32_t>("select _sig1()");
             } catch (cppgres::pg_exception &e) {
               exception_raised =
                   true && _assert(std::string_view(e.message()) ==
                                   "unexpected return type, can't convert `integer` into `bool`");
             }
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(current_postgres_function, ([](test_case &) {
           bool result = true;

           auto ci = cppgres::current_postgres_function::call_info();
           result = result && _assert(ci.has_value());
           cppgres::syscache<Form_pg_proc, cppgres::oid> proc((*ci)->flinfo->fn_oid);
           result = result && _assert(std::string_view(NameStr((*proc).proname)) == "cppgres_test");

           return result;
         }));
} // namespace tests
