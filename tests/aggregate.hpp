#pragma once

#include "tests.hpp"

struct aggregate_test {
  int32_t x;
  aggregate_test() : x(1) {}
  void update(int32_t v) { x += v; }
  int32_t finalize() { return x; }
};

declare_aggregate(aggregate, aggregate_test, int32_t);

struct aggregate_convertible_test {
  int32_t x;
  aggregate_convertible_test() : x(1) {}
  aggregate_convertible_test(int x_) : x(x_) {}
  void update(int32_t v) { x += v; }
};

namespace cppgres {
template <> struct datum_conversion<aggregate_convertible_test> {
  static aggregate_convertible_test
  from_nullable_datum(const nullable_datum &d, const oid oid,
                      std::optional<memory_context> context = std::nullopt) {
    if (d.is_null()) {
      return {};
    } else {
      return {datum_conversion<int32_t>::from_datum(d, oid, context)};
    }
  }

  static aggregate_convertible_test
  from_datum(const datum &d, const oid oid, std::optional<memory_context> context = std::nullopt) {
    return {datum_conversion<int32_t>::from_datum(d, oid, context)};
  }
  static datum into_datum(const aggregate_convertible_test &d) {
    return datum_conversion<int32_t>::into_datum(d.x);
  }
};
} // namespace cppgres

declare_aggregate(aggregate_convertible, aggregate_convertible_test, int32_t);

namespace tests {

add_test(aggregate_simple, [](test_case &) {
  bool result = true;
  cppgres::spi_executor spi;
  spi.execute(cppgres::fmt::format("create or replace function aggregate_sfunc(internal, int) "
                                   "returns internal language c as '{}'",
                                   get_library_name()));
  spi.execute(cppgres::fmt::format(
      "create or replace function aggregate_ffunc(internal) returns int language c as '{}'",
      get_library_name()));
  spi.execute(
      "create aggregate agg (int) (sfunc = aggregate_sfunc, finalfunc = aggregate_ffunc, stype "
      "= internal)");
  auto res = spi.query<int32_t>("select agg(v) from (values (1), (2), (3)) as t(v)");
  result = result && _assert(res.begin()[0] == 7);
  return result;
});

add_test(aggregate_convertible, [](test_case &) {
  bool result = true;
  cppgres::spi_executor spi;
  spi.execute(
      cppgres::fmt::format("create or replace function aggregate_convertible_sfunc(int, int) "
                           "returns int language c as '{}'",
                           get_library_name()));
  spi.execute("create aggregate agg (int) (sfunc = aggregate_convertible_sfunc, stype = int)");
  auto res = spi.query<int32_t>("select agg(v) from (values (1), (2), (3)) as t(v)");
  result = result && _assert(res.begin()[0] == 7);
  return result;
});

}; // namespace tests
