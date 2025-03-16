/**
 * \file
 */
#pragma once

#include <optional>
#include <string>
#include <utility>

#include "datum.hpp"
#include "syscache.hpp"
#include "type.hpp"

namespace cppgres {

template <> struct type_traits<void> {
  static bool is(const type &t) { return t.oid == VOIDOID; }
  static constexpr type type_for() { return type{.oid = VOIDOID}; }
};

template <typename S> struct type_traits<S, std::enable_if_t<utils::is_std_tuple<S>::value>> {
  static bool is(const type &t) {
    if (t.oid == RECORDOID) {
      return true;
    } else if constexpr (std::tuple_size_v<S> == 1) {
      // special case when we have a tuple of 1 matching the type
      return type_traits<std::tuple_element_t<0, S>>::is(t);
    }
    return false;
  }
  static constexpr type type_for() { return type{.oid = RECORDOID}; }
};

template <> struct type_traits<bool> {
  static bool is(const type &t) { return t.oid == BOOLOID; }
  static constexpr type type_for() { return type{.oid = BOOLOID}; }
};

template <> struct type_traits<int64_t> {
  static bool is(const type &t) { return t.oid == INT8OID || t.oid == INT4OID || t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT8OID}; }
};

template <> struct type_traits<int32_t> {
  static bool is(const type &t) { return t.oid == INT4OID || t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT4OID}; }
};

template <> struct type_traits<int16_t> {
  static bool is(const type &t) { return t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT2OID}; }
};

template <> struct type_traits<int8_t> {
  static bool is(const type &t) { return t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT2OID}; }
};

template <> struct type_traits<text> {
  static bool is(const type &t) { return t.oid == TEXTOID; }
  static constexpr type type_for() { return type{.oid = TEXTOID}; }
};

template <> struct type_traits<std::string_view> {
  static bool is(const type &t) { return t.oid == TEXTOID; }
  static constexpr type type_for() { return type{.oid = TEXTOID}; }
};

template <> struct type_traits<std::string> {
  static bool is(const type &t) { return t.oid == TEXTOID; }
  static constexpr type type_for() { return type{.oid = TEXTOID}; }
};

template <> struct type_traits<byte_array> {
  static bool is(const type &t) { return t.oid == BYTEAOID; }
  static constexpr type type_for() { return type{.oid = BYTEAOID}; }
};

template <> struct type_traits<bytea> {
  static bool is(const type &t) { return t.oid == BYTEAOID; }
  static constexpr type type_for() { return type{.oid = BYTEAOID}; }
};

template <> struct type_traits<char *> {
  static bool is(const type &t) { return t.oid == CSTRINGOID; }
  static constexpr type type_for() { return type{.oid = CSTRINGOID}; }
};

template <> struct type_traits<const char *> {
  static bool is(const type &t) { return t.oid == CSTRINGOID; }
  static constexpr type type_for() { return type{.oid = CSTRINGOID}; }
};

template <std::size_t N> struct type_traits<const char[N]> {
  static bool is(const type &t) { return t.oid == CSTRINGOID; }
  static constexpr type type_for() { return type{.oid = CSTRINGOID}; }
};

template <flattenable F> struct type_traits<expanded_varlena<F>> {
  static bool is(const type &t) { return t.oid == F::type().oid; }
  static constexpr type type_for() { return F::type(); }
};

template <> struct datum_conversion<datum> {
  static datum from_datum(const datum &d, std::optional<memory_context>) { return d; }

  static datum into_datum(const datum &t) { return t; }
};

template <> struct datum_conversion<nullable_datum> {
  static nullable_datum from_datum(const datum &d, std::optional<memory_context>) {
    return nullable_datum(d);
  }

  static datum into_datum(const nullable_datum &t) { return t; }
};

template <> struct datum_conversion<oid> {
  static oid from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<oid>(d.operator const ::Datum &());
  }

  static datum into_datum(const oid &t) { return datum(static_cast<::Datum>(t)); }
};

template <> struct datum_conversion<size_t> {
  static size_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<size_t>(d.operator const ::Datum &());
  }

  static datum into_datum(const size_t &t) { return datum(static_cast<::Datum>(t)); }
};

template <> struct datum_conversion<int64_t> {
  static int64_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<int64_t>(d.operator const ::Datum &());
  }

  static datum into_datum(const int64_t &t) { return datum(static_cast<::Datum>(t)); }
};

template <> struct datum_conversion<int32_t> {
  static int32_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<int32_t>(d.operator const ::Datum &());
  }
  static datum into_datum(const int32_t &t) { return datum(static_cast<::Datum>(t)); }
};

template <> struct datum_conversion<int16_t> {
  static int16_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<int16_t>(d.operator const ::Datum &());
  }
  static datum into_datum(const int16_t &t) { return datum(static_cast<::Datum>(t)); }
};

template <> struct datum_conversion<bool> {
  static bool from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<bool>(d.operator const ::Datum &());
  }
  static datum into_datum(const bool &t) { return datum(static_cast<::Datum>(t)); }
};

// Specializations for text and bytea:
template <> struct datum_conversion<text> {
  static text from_datum(const datum &d, std::optional<memory_context> ctx) { return text{d, ctx}; }

  static datum into_datum(const text &t) { return t.get_datum(); }
};

template <> struct datum_conversion<bytea> {
  static bytea from_datum(const datum &d, std::optional<memory_context> ctx) {
    return bytea{d, ctx};
  }

  static datum into_datum(const bytea &t) { return t.get_datum(); }
};

template <> struct datum_conversion<byte_array> {
  static byte_array from_datum(const datum &d, std::optional<memory_context> ctx) {
    return bytea{d, ctx};
  }

  static datum into_datum(const byte_array &t) {
    // This is not perfect if the data was already allocated with Postgres
    // But once we're with the byte_array (std::span) we've lost this information
    // TODO: can we do any better here?
    return bytea(t, memory_context()).get_datum();
  }
};

// Specializations for std::string_view and std::string.
// Here we re-use the conversion for text.
template <> struct datum_conversion<std::string_view> {
  static std::string_view from_datum(const datum &d, std::optional<memory_context> ctx) {
    return datum_conversion<text>::from_datum(d, ctx);
  }

  static datum into_datum(const std::string_view &t) {
    size_t sz = t.size();
    void *result = ffi_guard{::palloc}(sz + VARHDRSZ);
    SET_VARSIZE(result, t.size() + VARHDRSZ);
    memcpy(VARDATA(result), t.data(), sz);
    return datum(reinterpret_cast<::Datum>(result));
  }
};

template <> struct datum_conversion<std::string> {
  static std::string from_datum(const datum &d, std::optional<memory_context> ctx) {
    // Convert the text to a std::string_view then construct a std::string.
    return std::string(datum_conversion<text>::from_datum(d, ctx).operator std::string_view());
  }

  static datum into_datum(const std::string &t) {
    return datum_conversion<std::string_view>::into_datum(t);
  }
};

template <> struct datum_conversion<const char *> {
  static const char *from_datum(const datum &d, std::optional<memory_context> ctx) {
    return DatumGetPointer(d);
  }

  static datum into_datum(const char *const &t) { return datum(PointerGetDatum(t)); }
};

template <std::size_t N> struct datum_conversion<char[N]> {
  static const char *from_datum(const datum &d, std::optional<memory_context> ctx) {
    return DatumGetPointer(d);
  }

  static datum into_datum(const char (&t)[N]) { return datum(PointerGetDatum(t)); }
};

template <typename T> struct datum_conversion<T, std::enable_if_t<expanded_varlena_type<T>>> {
  static T from_datum(const datum &d, std::optional<memory_context> ctx) { return {d, ctx}; }

  static datum into_datum(const T &t) { return t.get_expanded_datum(); }
};

/**
 * @brief Type identified by its name
 *
 * @note Once constructed, the resolved type stays the same and doesn't change during
 *       the lifetime of the value.
 */
struct named_type : public type {
  /**
   * @brief Type identified by an unqualified name
   *
   * @param name unqualified type name
   */
  named_type(const std::string_view name) : type(::TypenameGetTypid(name.data())) {}
  /**
   * @brief Type identified by a qualified name
   *
   * @param schema schema name
   * @param name type name
   */
  named_type(const std::string_view schema, const std::string_view name)
      : type(([&]() {
          cppgres::oid nsoid = ffi_guard{::LookupExplicitNamespace}(schema.data(), false);
          cppgres::oid oid = InvalidOid;
          if (OidIsValid(nsoid)) {
            oid = (*syscache<Form_pg_type, const char *, cppgres::oid>(
                       TYPENAMENSP, std::string(name).c_str(), nsoid))
                      .oid;
          }
          return oid;
        })()) {}
};

} // namespace cppgres
