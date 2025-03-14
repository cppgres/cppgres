#pragma once

#include "type.hpp"

namespace cppgres {

/**
 * @brief Runtime-typed value of `record` type
 *
 * These records don't have a structure known upfront, for example, when a value
 * of this type (`record`) are passed into a function.
 */
struct record {

  friend struct datum_conversion<record>;

  record(HeapTupleHeader heap_tuple)
      : heap_tuple(heap_tuple),
        tupdesc(ffi_guard(::lookup_rowtype_tupdesc)(HeapTupleHeaderGetTypeId(heap_tuple),
                                                    HeapTupleHeaderGetTypMod(heap_tuple))) {
#if PG_MAJORVERSION_NUM < 18
    tuple.t_len = HeapTupleHeaderGetDatumLength(tupdesc);
#else
    tuple.t_len = HeapTupleHeaderGetDatumLength(heap_tuple);
#endif
    tuple.t_data = heap_tuple;
  }

  /**
   * @brief Number of attributes in the record
   */
  int attributes() const { return tupdesc->natts; }

  /**
   * @brief Type of attribute using a 0-based index
   *
   * @throws std::out_of_range
   */
  type attribute_type(int n) const {
    check_bounds(n);
    return {.oid = TupleDescAttr(tupdesc, n)->atttypid};
  }

  /**
   * @brief Name of attribute using a 0-based index
   *
   * @throws std::out_of_range
   */
  std::string_view attribute_name(int n) const {
    check_bounds(n);
    return {NameStr(TupleDescAttr(tupdesc, n)->attname)};
  }

  /**
   * @brief Get attribute value (datum) using a 0-based index
   *
   * @throws std::out_of_range
   */
  nullable_datum get_attribute(int n) {
    bool isnull;
    check_bounds(n);
    datum d(heap_getattr(&tuple, n + 1, tupdesc, &isnull));
    return isnull ? nullable_datum() : nullable_datum(d);
  }

  /**
   * @brief Get attribute by name
   *
   * @throws std::out_of_range
   */
  nullable_datum operator[](std::string_view name) {
    for (int i = 0; i < attributes(); i++) {
      if (attribute_name(i) == name) {
        return get_attribute(i);
      }
    }
    throw std::out_of_range(std::format("no attribute by the name of {}", name));
  }

  /**
   * @brief Get attribute by 0-based index
   *
   * @throws std::out_of_range
   */
  nullable_datum operator[](int n) { return get_attribute(n); }

private:
  inline void check_bounds(int n) const {
    if (n + 1 > attributes() || n < 0) {
      throw std::out_of_range(std::format(
          "attribute index {} is out of bounds for record with the size of {}", n, attributes()));
    }
  }

  HeapTupleHeader heap_tuple;
  HeapTupleData tuple;
  TupleDesc tupdesc;
};

template <> struct datum_conversion<record> {
  static record from_datum(const datum &d, std::optional<memory_context> ctx) {
    return {reinterpret_cast<HeapTupleHeader>(ffi_guarded(::pg_detoast_datum)(
        reinterpret_cast<struct ::varlena *>(d.operator const ::Datum &())))};
  }

  static datum into_datum(const record &t) { return datum(PointerGetDatum(t.heap_tuple)); }
};

template <> struct type_traits<record> {
  static bool is(type &t) { return t.oid == RECORDOID; }
  static constexpr type type_for() { return {.oid = RECORDOID}; }
};

} // namespace cppgres
