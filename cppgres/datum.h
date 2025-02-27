#pragma once

#include "imports.h"
#include "utils/utils.h"

#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace cppgres {

struct datum {
  template <typename T> friend datum into_datum(T &d);

  operator ::Datum &() { return _datum; }

  operator const ::Datum &() { return _datum; }

private:
  ::Datum _datum;
  explicit datum(::Datum datum) : _datum(datum) {}
  friend struct nullable_datum;
};

class null_datum_exception : public std::exception {
  const char *what() const noexcept override { return "passed datum is null"; }
};

struct nullable_datum {

  template <typename T> friend T from_nullable_datum(nullable_datum &d);
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
  explicit nullable_datum(datum d) : _ndatum({.value = d._datum, .isnull = false}) {}

private:
  union {
    ::NullableDatum _ndatum;
    datum _datum;
  };

};

template <typename T> datum into_datum(T &v);

template <typename T>
concept convertible_into_datum = requires(T t) {
  { cppgres::into_datum(t) } -> std::same_as<datum>;
};

template <typename T> T from_datum(datum &);

template <typename T>
concept convertible_from_datum = requires(datum d) {
  { cppgres::template from_datum<T>(d) } -> std::same_as<T>;
};

template <typename T> struct unsupported_type {};

template <typename T>
requires convertible_from_datum<T> ||
         (utils::is_optional<T> && convertible_from_datum<utils::remove_optional_t<T>>)
T from_nullable_datum(nullable_datum &d) {
  if constexpr (utils::is_optional<T>) {
    if (d.is_null()) {
      return std::nullopt;
    }
    if constexpr (convertible_from_datum<utils::remove_optional_t<T>>) {
      return std::optional(from_datum<utils::remove_optional_t<T>>(d));
    } else {
      static_assert("no viable conversion");
    }
  } else {
    if (d.is_null()) {
      throw std::runtime_error(
          std::format("datum is null and can't be coerced into {}", utils::type_name<T>()));
    }
    return from_datum<T>(d);
  }
}

template <typename T> nullable_datum into_nullable_datum(T &v) {
  if constexpr (utils::is_optional<T> && convertible_into_datum<utils::remove_optional_t<T>>) {
    if (v.has_value()) {
      return nullable_datum(into_datum(v.value()));
    } else {
      return nullable_datum();
    }
  } else if constexpr (convertible_into_datum<utils::remove_optional_t<T>>) {
    return nullable_datum(into_datum(v));
  } else {
    return into_nullable_datum<unsupported_type<T>>(v);
  }
}

template <typename T>
concept convertible_from_nullable_datum = requires(nullable_datum d) {
  { cppgres::template from_nullable_datum<T>(d) } -> std::same_as<T>;
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
