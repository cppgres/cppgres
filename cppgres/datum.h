#pragma once

#include "imports.h"
#include "utils/utils.h"

#include <cstdint>
#include <optional>
#include <string>

namespace cppgres {

struct datum {

  operator ::Datum &() { return _datum; }

  operator const ::Datum &() { return _datum; }

private:
  ::Datum _datum;
  explicit datum(::Datum datum) : _datum(datum) {}
  friend class nullable_datum;
};

class null_datum_exception : public std::exception {
  const char *what() const noexcept override { return "passed datum is null"; }
};

struct nullable_datum {

  template <typename T> friend std::optional<T> from_nullable_datum(nullable_datum &d);
  template <typename T> friend nullable_datum into_nullable_datum(T &d);

  bool is_null() const noexcept { return _ndatum.isnull; }

  operator struct datum &() {
    if (_ndatum.isnull) {
      throw null_datum_exception();
    }
    return _datum;
  }

  operator ::Datum &() {
    if (_ndatum.isnull) {
      throw null_datum_exception();
    }
    return _datum;
  }

  operator const struct datum &() const {
    if (_ndatum.isnull) {
      throw null_datum_exception();
    }
    return _datum;
  }

  explicit nullable_datum(::NullableDatum d) : _ndatum(d) {}

  explicit nullable_datum() : _ndatum({.isnull = true}) {}

  template <typename T> static nullable_datum from(T t) = delete;

  explicit nullable_datum(::Datum d) : _ndatum({.value = d, .isnull = false}) {}

private:
  union {
    ::NullableDatum _ndatum;
    datum _datum;
  };

};

template <typename T> std::optional<T> from_nullable_datum(nullable_datum &d);
template <typename T> nullable_datum into_nullable_datum(T &v);

template <typename T>
concept convertible_from_nullable_datum = requires(nullable_datum d) {
  { cppgres::template from_nullable_datum<T>(d) } -> std::same_as<std::optional<T>>;
};

template <typename T>
concept convertible_into_nullable_datum = requires(T t) {
  { cppgres::into_nullable_datum(t) } -> std::same_as<nullable_datum>;
};

template <typename Tuple> struct all_from_nullable_datum;

template <typename... Ts> struct all_from_nullable_datum<std::tuple<Ts...>> {
  static constexpr bool value =
      (... && (convertible_from_nullable_datum<utils::remove_optional_t<Ts>>));
};

} // namespace cppgres
