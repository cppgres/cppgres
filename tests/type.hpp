#pragma once

#include "tests.hpp"

struct my_custom_type {
  std::string s;
  int reserved;
};

namespace cppgres {
template <> struct type_traits<::my_custom_type> {
  bool is(type &t) { return t == type_for(); }
  type type_for() { return cppgres::named_type("custom_type"); }
};
template <> struct datum_conversion<::my_custom_type> {
  static ::my_custom_type from_datum(const datum &d, oid oid, std::optional<memory_context> ctx) {
    std::string s(datum_conversion<text>::from_datum(d, oid, ctx).operator std::string_view());
    return {.s = s};
  }
  static datum into_datum(const ::my_custom_type &t) {
    return cppgres::datum_conversion<std::string_view>::into_datum(t.s);
  }
};

} // namespace cppgres

namespace tests {

add_test(type_name_smoke, [](test_case &) {
  auto ty = cppgres::type_traits<std::string_view>().type_for();
  return _assert(ty.name() == "text") && _assert(ty.name(true) == "pg_catalog.text");
});

add_test(named_type, [](test_case &) {
  bool result = true;
  auto ty1 = cppgres::named_type("text");
  result = result && _assert(ty1.name(true) == "pg_catalog.text");
  auto ty2 = cppgres::named_type("pg_catalog", "text");
  result = result && _assert(ty2.name(true) == "pg_catalog.text");
  return result;
});

postgres_function(custom_type_fun, ([](my_custom_type t) {
                    my_custom_type t1 = t;
                    std::reverse(t1.s.begin(), t1.s.end());
                    return t1;
                  }));

add_test(
    custom_type, ([](test_case &) {
      bool result = true;

      cppgres::spi_executor spi;
      spi.execute("create domain custom_type as text");
      spi.execute(cppgres::fmt::format(
          "create function custom_type_fun(custom_type) returns custom_type language c as '{}'",
          get_library_name()));

      {
        auto val = spi.query<my_custom_type>("select custom_type_fun('test')");
        result = result && _assert(val.begin()[0].s == "tset");
      }

      {
        auto val = spi.query<std::tuple<my_custom_type>>("select custom_type_fun('test')");
        result = result && _assert(std::get<0>(val.begin()[0]).s == "tset");
      }

      {
        auto val = spi.query<std::tuple<my_custom_type, my_custom_type>>(
            "select custom_type_fun('test'), custom_type_fun('hi')");
        result = result && _assert(std::get<0>(val.begin()[0]).s == "tset");
        result = result && _assert(std::get<1>(val.begin()[0]).s == "ih");
      }

      return result;
    }));

add_test(tuples_are_records, ([](test_case &) {
           bool result = true;

           using t = std::tuple<std::string, std::string>;
           result =
               result && _assert(cppgres::type_traits<t>().is(cppgres::type{.oid = RECORDOID}));

           return result;
         }));

} // namespace tests
