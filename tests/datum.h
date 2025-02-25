#pragma once

#include "tests.h"

namespace tests {

add_test(nullable_datum_enforcement, [](test_case &) {
  cppgres::nullable_datum d;
  assert(d.is_null());
  try {
    cppgres::ffi_guarded(::DatumGetDateADT)(d);
    _assert(false);
  } catch (cppgres::null_datum_exception &e) {
    return true;
  }
  // try the same unguarded, it won't (shouldn't) call it anyway
  try {
    ::DatumGetDateADT(d);
    _assert(false);
  } catch (cppgres::null_datum_exception &e) {
    return true;
  }
  return false;
});

add_test(varlena_text, [](test_case &) {
  bool result = true;
  auto nd = cppgres::nullable_datum(::PointerGetDatum(::cstring_to_text("test")));
  auto s = cppgres::from_nullable_datum<cppgres::text>(nd);
  std::string_view str = *s;
  result = result && _assert(str == "test");

  // Try memory context being gone
  {
    auto ctx = cppgres::alloc_set_memory_context();
    auto p = ::CurrentMemoryContext;
    _assert(p != ctx);
    ::CurrentMemoryContext = ctx;
    auto nd1 = cppgres::nullable_datum(::PointerGetDatum(::cstring_to_text("test1")));
    auto s1 = cppgres::from_nullable_datum<cppgres::text>(nd1);
    _assert(s1->memory_context() == ctx);
    ::CurrentMemoryContext = p;
    ctx.reset();

    bool exception_raised = false;
    try {
      std::string_view str1 = *s1;
    } catch (cppgres::pointer_gone_exception &e) {
      exception_raised = true;
    }
    result = result && _assert(exception_raised);
  }

  return result;
});

add_test(varlena_bytea, ([](test_case &) {
           bool result = true;
           auto nd = cppgres::nullable_datum(::PointerGetDatum(::cstring_to_text("test")));
           auto s = cppgres::from_nullable_datum<cppgres::bytea>(nd);
           cppgres::byte_array ba = *s;
           result = result && _assert(ba[0] == std::byte('t'));
           result = result && _assert(ba[1] == std::byte('e'));
           result = result && _assert(ba[2] == std::byte('s'));
           result = result && _assert(ba[3] == std::byte('t'));
           return result;
         }));

} // namespace tests
