#pragma once

#include "tests.hpp"

namespace tests {

add_test(nullable_datum_enforcement, [](test_case &) {
  cppgres::nullable_datum d;
  assert(d.is_null());
  try {
    [[maybe_unused]] auto val = DatumGetDateADT(d);
    _assert(false);
  } catch (cppgres::null_datum_exception &e) {
    return true;
  }
  return false;
});

add_test(nullable_type_to_non_optional, ([](test_case &) {
           bool result = true;
           bool exception_raised = false;

           try {
             auto nd = cppgres::nullable_datum();
             cppgres::from_nullable_datum<int64_t>(nd);
           } catch (std::runtime_error &e) {
             exception_raised = true;
           }
           result = result && _assert(exception_raised);

           return result;
         }));

add_test(varlena_text, [](test_case &) {
  bool result = true;
  auto nd = cppgres::nullable_datum(PointerGetDatum(::cstring_to_text("test")));
  auto s = cppgres::from_nullable_datum<cppgres::text>(nd);
  std::string_view str = s;
  result = result && _assert(s.is_detoasted());
  result = result && _assert(str == "test");

  // Try memory context being gone
  {
    auto ctx = cppgres::memory_context(std::move(cppgres::alloc_set_memory_context()));
    auto p = ::CurrentMemoryContext;
    _assert(p != ctx);
    ::CurrentMemoryContext = ctx;
    auto nd1 = cppgres::nullable_datum(PointerGetDatum(::cstring_to_text("test1")));
    auto s1 = cppgres::from_nullable_datum<cppgres::text>(nd1, ctx);
    _assert(s1.get_memory_context() == ctx);
    ::CurrentMemoryContext = p;
    ctx.reset();

    bool exception_raised = false;
    try {
      s1.operator std::string_view();
    } catch (cppgres::pointer_gone_exception &e) {
      exception_raised = true;
    }
    result = result && _assert(exception_raised);
  }

  return result;
});

add_test(varlena_bytea, ([](test_case &) {
           bool result = true;
           auto nd = cppgres::nullable_datum(PointerGetDatum(::cstring_to_text("test")));
           auto s = cppgres::from_nullable_datum<cppgres::bytea>(nd);
           cppgres::byte_array ba = s;
           result = result && _assert(ba[0] == std::byte('t'));
           result = result && _assert(ba[1] == std::byte('e'));
           result = result && _assert(ba[2] == std::byte('s'));
           result = result && _assert(ba[3] == std::byte('t'));
           return result;
         }));

add_test(varlena_byte_array, ([](test_case &) {
           bool result = true;
           auto nd = cppgres::nullable_datum(PointerGetDatum(::cstring_to_text("test")));
           auto s = cppgres::from_nullable_datum<cppgres::bytea>(nd);
           cppgres::byte_array ba = s;
           const auto d = cppgres::datum_conversion<cppgres::byte_array>::into_datum(ba);
           // NB: below we endure a copy; can we do any better?
           auto ba1 = cppgres::datum_conversion<cppgres::byte_array>::from_datum(d, std::nullopt);

           result = result && _assert(ba1[0] == std::byte('t'));
           result = result && _assert(ba1[1] == std::byte('e'));
           result = result && _assert(ba1[2] == std::byte('s'));
           result = result && _assert(ba1[3] == std::byte('t'));
           return result;
         }));

add_test(varlena_text_into_strings, ([](test_case &) {
           bool result = true;
           auto nd = cppgres::nullable_datum(PointerGetDatum(::cstring_to_text("test")));
           {
             auto str = cppgres::from_nullable_datum<std::string_view>(nd);
             result = result && _assert(str == "test");
           }
           {
             auto str = cppgres::from_nullable_datum<std::string>(nd);
             result = result && _assert(str == "test");
           }

           return result;
         }));

add_test(varlena_text_from_strings, ([](test_case &) {
           bool result = true;

           {
             auto d =
                 cppgres::datum_conversion<std::string_view>::into_datum(std::string_view("test"));
             auto str = cppgres::datum_conversion<std::string_view>::from_datum(d, std::nullopt);
             result = result && _assert(str == "test");
           }

           {
             auto d = cppgres::datum_conversion<std::string>::into_datum(std::string("test"));
             auto str = cppgres::datum_conversion<std::string_view>::from_datum(d, std::nullopt);
             result = result && _assert(str == "test");
           }

           return result;
         }));

// Ensure arbitrary types are not automatically deducted
// to be convertible from into datums.
struct some_type {};
static_assert(!cppgres::convertible_from_datum<some_type>);
static_assert(!cppgres::convertible_into_datum<some_type>);

add_test(lazy_detoast, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           spi.execute("create table a (a text)");
           spi.execute("insert into a values ('hello world')");
           auto res = spi.query<cppgres::text>("select a from a");
           // here we never actually detoast it
           for (auto r : res) {
             result = result && _assert(!r.is_detoasted());
             spi.execute("insert into a values ($1)", r);
           }
           for (auto re : spi.query<std::string_view>("select a from a")) {
             result = result && _assert(re == "hello world");
           }
           return result;
         }));

add_test(eoh_smoke, ([](test_case &e) {
           bool result = true;

           struct my_eoh {
             int a = 0;
             std::size_t flat_size() { return sizeof(a); }

             void flatten_into(std::span<std::byte> buffer) {
               cppgres::report(NOTICE, "flatten_into");
               if (buffer.size_bytes() == flat_size()) {
                 int *to_ptr = reinterpret_cast<int *>(buffer.data());
                 std::span tbuffer(to_ptr, 1);
                 tbuffer[0] = a;
               } else {
                 cppgres::report(ERROR, "wrong buffer size");
               }
             }

             static cppgres::type type() { return {.oid = BYTEAOID}; }

             static my_eoh restore_from(std::span<std::byte> buffer) {
               int *to_ptr = reinterpret_cast<int *>(buffer.data());
               std::span tbuffer(to_ptr, 1);
               return my_eoh{.a = tbuffer[0]};
             }
           };

           static_assert(cppgres::flattenable<my_eoh>);

           // Create a new one
           auto d = cppgres::expanded_varlena<my_eoh>();
           my_eoh &dv = d;
           dv.a = 650;

           // Flatten it. We are hijacking `bytea` varlena type to pass it through
           // to avoid having to define a new type (FIXME)
           cppgres::spi_executor spi;
           spi.execute("create table eoh_smoke_test (v bytea)");
           auto d2 = spi.query<cppgres::expanded_varlena<my_eoh>>(
               "insert into eoh_smoke_test values ($1) returning v", d);
           result = result && _assert(d2.begin()[0].operator my_eoh &().a == dv.a);

           return result;
         }));

} // namespace tests
