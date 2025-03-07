/**
* \file
 */
#pragma once

#include <optional>
#include <string>
#include <utility>

#include "datum.h"
#include "type.h"

namespace cppgres {

template <> struct type_traits<bool> {
  static bool is(type &t) { return t.oid == BOOLOID; }
  static constexpr type type_for() { return type{.oid = BOOLOID}; }
};

template <> struct type_traits<int64_t> {
  static bool is(type &t) { return t.oid == INT8OID || t.oid == INT4OID || t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT8OID}; }
};

template <> struct type_traits<int32_t> {
  static bool is(type &t) { return t.oid == INT4OID || t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT4OID}; }
};

template <> struct type_traits<int16_t> {
  static bool is(type &t) { return t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT2OID}; }
};

template <> struct type_traits<int8_t> {
  static bool is(type &t) { return t.oid == INT2OID; }
  static constexpr type type_for() { return type{.oid = INT2OID}; }
};

template <> struct type_traits<text> {
  static bool is(type &t) { return t.oid == TEXTOID; }
  static constexpr type type_for() { return type{.oid = TEXTOID}; }
};

template <> struct type_traits<std::string_view> {
  static bool is(type &t) { return t.oid == TEXTOID; }
  static constexpr type type_for() { return type{.oid = TEXTOID}; }
};

template <> struct type_traits<std::string> {
  static bool is(type &t) { return t.oid == TEXTOID; }
  static constexpr type type_for() { return type{.oid = TEXTOID}; }
};

template <> struct type_traits<byte_array> {
  static bool is(type &t) { return t.oid == BYTEAOID; }
  static constexpr type type_for() { return type{.oid = BYTEAOID}; }
};

template <> struct type_traits<bytea> {
  static bool is(type &t) { return t.oid == BYTEAOID; }
  static constexpr type type_for() { return type{.oid = BYTEAOID}; }
};

template <flattenable F> struct type_traits<expanded_varlena<F>> {
  static bool is(type &t) { return t.oid == F::type().oid; }
  static constexpr type type_for() { return F::type(); }
};

template <> datum into_datum(const size_t &t) { return datum(static_cast<::Datum>(t)); }
template <> datum into_datum(const int64_t &t) { return datum(static_cast<::Datum>(t)); }
template <> datum into_datum(const int32_t &t) { return datum(static_cast<::Datum>(t)); }
template <> datum into_datum(const int16_t &t) { return datum(static_cast<::Datum>(t)); }
template <> datum into_datum(const bool &t) { return datum(static_cast<::Datum>(t)); }
template <> datum into_datum(const text &t) { return t.get_datum(); }
template <> datum into_datum(const std::string_view &t) {
  size_t sz = t.size();
  void *result = ffi_guarded(::palloc)(sz + VARHDRSZ);
  SET_VARSIZE(result, t.size() + VARHDRSZ);
  memcpy(VARDATA(result), t.data(), sz);
  return datum(reinterpret_cast<::Datum>(result));
}
template <> datum into_datum(const std::string &t) { return into_datum(std::string_view(t)); }
template <flattenable F> datum into_datum(const expanded_varlena<F> &t) {
  return t.get_expanded_datum();
}

template <> struct datum_conversion<size_t> {
  static size_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<size_t>(d.operator const ::Datum &());
  }
};

template <> struct datum_conversion<int64_t> {
  static int64_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<int64_t>(d.operator const ::Datum &());
  }
};

template <> struct datum_conversion<int32_t> {
  static int32_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<int32_t>(d.operator const ::Datum &());
  }
};

template <> struct datum_conversion<int16_t> {
  static int16_t from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<int16_t>(d.operator const ::Datum &());
  }
};

template <> struct datum_conversion<bool> {
  static bool from_datum(const datum &d, std::optional<memory_context>) {
    return static_cast<bool>(d.operator const ::Datum &());
  }
};

// Specializations for text and bytea:
template <> struct datum_conversion<text> {
  static text from_datum(const datum &d, std::optional<memory_context> ctx) { return text{d, ctx}; }
};

template <> struct datum_conversion<bytea> {
  static bytea from_datum(const datum &d, std::optional<memory_context> ctx) {
    return bytea{d, ctx};
  }
};

// Specializations for std::string_view and std::string.
// Here we re-use the conversion for text.
template <> struct datum_conversion<std::string_view> {
  static std::string_view from_datum(const datum &d, std::optional<memory_context> ctx) {
    return datum_conversion<text>::from_datum(d, ctx);
  }
};

template <> struct datum_conversion<std::string> {
  static std::string from_datum(const datum &d, std::optional<memory_context> ctx) {
    // Convert the text to a std::string_view then construct a std::string.
    return std::string(datum_conversion<text>::from_datum(d, ctx).operator std::string_view());
  }
};

template <typename T> struct datum_conversion<T, std::enable_if_t<expanded_varlena_type<T>>> {
  static T from_datum(const datum &d, std::optional<memory_context> ctx) { return {d, ctx}; }
};

} // namespace cppgres
