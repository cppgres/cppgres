/**
 * \file
 */
#pragma once

#include "imports.h"
#include "memory.hpp"
#include "utils/utils.hpp"

#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace cppgres {

using oid = ::Oid;

struct datum {
  template <typename T, typename> friend struct datum_conversion;

  operator const ::Datum &() const { return _datum; }

  operator Pointer() const { return reinterpret_cast<Pointer>(_datum); }

  datum() : _datum(0) {}
  explicit datum(::Datum datum) : _datum(datum) {}

  bool operator==(const datum &other) const { return _datum == other._datum; }

private:
  ::Datum _datum;
  friend struct nullable_datum;
};

class null_datum_exception : public std::exception {
  const char *what() const noexcept override { return "passed datum is null"; }
};

struct nullable_datum {

  template <typename T>
  friend T from_nullable_datum(const nullable_datum &d, std::optional<memory_context> ctx);
  template <typename T> friend nullable_datum into_nullable_datum(const T &d);

  bool is_null() const noexcept { return _ndatum.isnull; }

  operator struct datum &() {
    if (_ndatum.isnull) {
      throw null_datum_exception();
    }
    return _datum;
  }

  operator const ::Datum &() {
    if (_ndatum.isnull) {
      throw null_datum_exception();
    }
    return _datum.operator const ::Datum &();
  }

  operator const struct datum &() const {
    if (_ndatum.isnull) {
      throw null_datum_exception();
    }
    return _datum;
  }

  explicit nullable_datum(::NullableDatum d) : _ndatum(d) {}

  explicit nullable_datum() : _ndatum({.isnull = true}) {}

  explicit nullable_datum(::Datum d) : _ndatum({.value = d, .isnull = false}) {}
  explicit nullable_datum(datum d) : _ndatum({.value = d._datum, .isnull = false}) {}

  bool operator==(const nullable_datum &other) const {
    if (is_null()) {
      return other.is_null();
    }
    return _datum.operator==(other._datum);
  }

private:
  union {
    ::NullableDatum _ndatum;
    datum _datum;
  };

};

/**
 * @brief A trait to convert from and into a @ref cppgres::datum
 *
 * @tparam T C++ type to convert into and from
 */
template <typename T, typename = void> struct datum_conversion {
  /**
   * @brief Convert from a datum
   *
   * Gets an optional @ref cppgres::memory_context when available to be able to determine the source
   * of the (pointer) datum.
   */
  static T from_datum(const datum &, std::optional<memory_context> context = std::nullopt) = delete;
  /**
   * @brief Convert datum into a type
   *
   * Unlike @ref from_datum, gets no memory context.
   */
  static datum into_datum(const T &d) = delete;
};

template <typename T>
concept convertible_into_datum = requires(T t) {
  { datum_conversion<T, void>::into_datum(std::declval<T>()) } -> std::same_as<datum>;
};

template <typename T>
concept convertible_from_datum = requires(datum d, std::optional<memory_context> context) {
  { datum_conversion<T, void>::from_datum(d, context) } -> std::same_as<T>;
};

template <typename T> struct unsupported_type {};

template <typename T>
requires convertible_from_datum<T> ||
         (utils::is_optional<T> && convertible_from_datum<utils::remove_optional_t<T>>)
T from_nullable_datum(const nullable_datum &d,
                      std::optional<memory_context> context = std::nullopt) {
  if constexpr (utils::is_optional<T>) {
    if (d.is_null()) {
      return std::nullopt;
    }
    if constexpr (convertible_from_datum<utils::remove_optional_t<T>>) {
      return std::optional(datum_conversion<utils::remove_optional_t<T>>::from_datum(d, context));
    } else {
      static_assert("no viable conversion");
    }
  } else {
    if (d.is_null()) {
      throw std::runtime_error(
          std::format("datum is null and can't be coerced into {}", utils::type_name<T>()));
    }
    return datum_conversion<T>::from_datum(d, context);
  }
}

template <typename T> nullable_datum into_nullable_datum(const std::optional<T> &v) {
  if (v.has_value()) {
    return nullable_datum(datum_conversion<T>::into_datum(v.value()));
  } else {
    return nullable_datum();
  }
}

template <typename T> nullable_datum into_nullable_datum(const T &v) {
  return nullable_datum(datum_conversion<T>::into_datum(v));
}

template <typename T>
concept convertible_from_nullable_datum = requires {
  {
    cppgres::from_nullable_datum<T>(std::declval<nullable_datum>(),
                                    std::declval<std::optional<memory_context>>())
  } -> std::same_as<T>;
};

template <typename T>
concept convertible_into_nullable_datum = requires {
  { cppgres::into_nullable_datum(std::declval<T>()) } -> std::same_as<nullable_datum>;
};

template <typename T> struct all_from_nullable_datum {
private:
  static constexpr std::size_t N = utils::tuple_size_v<T>;

  template <std::size_t... I> static constexpr bool impl(std::index_sequence<I...>) {
    return (
        ... &&
        convertible_from_nullable_datum<utils::remove_optional_t<utils::tuple_element_t<I, T>>>);
  }

public:
  static constexpr bool value = impl(std::make_index_sequence<N>{});
};

} // namespace cppgres
