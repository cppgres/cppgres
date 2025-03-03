#pragma once

#include <optional>
#include <string>
#include <utility>

#include "datum.h"
#include "type.h"

namespace cppgres {

template <> struct type_traits<bool> {
  static bool is(type &t) { return t.oid == BOOLOID; }
};

template <> struct type_traits<int64_t> {
  static bool is(type &t) { return t.oid == INT8OID || t.oid == INT4OID || t.oid == INT2OID; }
};

template <> struct type_traits<int32_t> {
  static bool is(type &t) { return t.oid == INT4OID || t.oid == INT2OID; }
};

template <> struct type_traits<int16_t> {
  static bool is(type &t) { return t.oid == INT2OID; }
};

template <> struct type_traits<int8_t> {
  static bool is(type &t) { return t.oid == INT2OID; }
};

template <> struct type_traits<text> {
  static bool is(type &t) { return t.oid == TEXTOID; }
};

template <> struct type_traits<std::string_view> {
  static bool is(type &t) { return t.oid == TEXTOID; }
};

template <> struct type_traits<std::string> {
  static bool is(type &t) { return t.oid == TEXTOID; }
};

template <> struct type_traits<byte_array> {
  static bool is(type &t) { return t.oid == BYTEAOID; }
};

template <> constexpr type type_for<int64_t>() { return type{.oid = INT8OID}; }
template <> constexpr type type_for<int32_t>() { return type{.oid = INT4OID}; }
template <> constexpr type type_for<int16>() { return type{.oid = INT2OID}; }
template <> constexpr type type_for<bool>() { return type{.oid = BOOLOID}; }
template <> constexpr type type_for<text>() { return type{.oid = TEXTOID}; }
template <> constexpr type type_for<byte_array>() { return type{.oid = BYTEAOID}; }
template <> constexpr type type_for<std::string_view>() { return type{.oid = TEXTOID}; }
template <> constexpr type type_for<std::string>() { return type{.oid = TEXTOID}; }

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

template <> size_t from_datum(const datum &d) {
  return static_cast<size_t>(d.operator const ::Datum &());
}
template <> int64_t from_datum(const datum &d) {
  return static_cast<int64_t>(d.operator const ::Datum &());
}
template <> int32_t from_datum(const datum &d) {
  return static_cast<int32_t>(d.operator const ::Datum &());
}
template <> int16_t from_datum(const datum &d) {
  return static_cast<int16_t>(d.operator const ::Datum &());
}
template <> bool from_datum(const datum &d) {
  return static_cast<bool>(d.operator const ::Datum &());
}

template <> text from_datum(const datum &d) { return {d}; }
template <> bytea from_datum(const datum &d) { return {d}; }
template <> std::string_view from_datum(const datum &d) { return from_datum<text>(d); }
template <> std::string from_datum(const datum &d) {
  return std::string(from_datum<text>(d).operator std::string_view());
}

} // namespace cppgres
