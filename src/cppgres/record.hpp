#pragma once

#include "imports.h"
#include "name.hpp"
#include "syscache.hpp"
#include "type.hpp"
#include "types.hpp"

#include <ranges>

namespace cppgres {

/**
 * @brief Tuple descriptor operator
 *
 * Allows to create new or manipulate existing tuple descriptors
 */
struct tuple_descriptor {

  /**
   * @brief Create a tuple descriptor for a given number of attributes
   *
   * Creates in the current memory context
   *
   * @param nattrs number of attributes
   */
  tuple_descriptor(int nattrs)
      : tupdesc(ffi_guard{::CreateTemplateTupleDesc}(nattrs)), blessed(false) {
    for (int i = 0; i < nattrs; i++) {
      operator[](i).attcollation = InvalidOid;
      operator[](i).attisdropped = false;
    }
  }
  /**
   * @brief Create a tuple descriptor for a given `TupleDesc`
   *
   * @param tupdesc existing attribute
   * @param blessed true if already blessed (default)
   */
  tuple_descriptor(TupleDesc tupdesc, bool blessed = true) : tupdesc(tupdesc), blessed(blessed) {}

  /**
   * @brief Copy constructor
   *
   * Creates a copy instance of the tuple descriptor in the current memory contet
   */
  tuple_descriptor(tuple_descriptor &other)
      : tupdesc(ffi_guard{::CreateTupleDescCopyConstr}(other.tupdesc)), blessed(other.blessed) {}

  /**
   * @brief Move constructor
   */
  tuple_descriptor(tuple_descriptor &&other) : tupdesc(other.tupdesc), blessed(other.blessed) {}

  /**
   * @brief Copy assignment
   *
   * Creates a copy instance of the tuple descriptor in the current memory contet
   */
  tuple_descriptor &operator=(tuple_descriptor &other) {
    tupdesc = ffi_guard{::CreateTupleDescCopyConstr}(other.tupdesc);
    blessed = other.blessed;
    return *this;
  }

  /**
   * @brief Number of attributes
   */
  int attributes() const { return tupdesc->natts; }

  /**
   * @brief Get a reference to `Form_pg_attribute`
   *
   * @throws std::out_of_range when attribute index is out of range
   */
  ::FormData_pg_attribute &operator[](int n) const {
    check_bounds(n);
    return *TupleDescAttr(tupdesc, n);
  }

  /**
   * @brief Set attribute type
   *
   * @param n Zero-based attribute index
   * @param type new attribute type
   *
   * @throws std::out_of_range when attribute index is out of range
   */
  void set_type(int n, const type &type) {
    check_not_blessed();
    syscache<Form_pg_type, oid> typ(type.oid);
    auto &att = operator[](n);
    att.atttypid = (*typ).oid;
    att.attcollation = (*typ).typcollation;
    att.attlen = (*typ).typlen;
    att.attstorage = (*typ).typstorage;
    att.attalign = (*typ).typalign;
    att.atttypmod = (*typ).typtypmod;
    att.attbyval = (*typ).typbyval;
  }

  /**
   * @brief Set attribute name
   *
   * @param n Zero-based attribute index
   * @param name new attribute name
   *
   * @throws std::out_of_range when attribute index is out of range
   */
  void set_name(int n, const name &name) {
    check_not_blessed();
    auto &att = operator[](n);
    att.attname = name;
  }

  /**
   * @brief returns a pointer to `TupleDesc`
   *
   * At this point, it'll be prepared and blessed.
   */
  operator TupleDesc() {
    if (!blessed) {
#if PG_MAJORVERSION_NUM >= 18
      for (int i = 0; i < tupdesc->natts; i++) {
        ffi_guard{::populate_compact_attribute}(tupdesc, i);
      }
#endif
      tupdesc = ffi_guard{::BlessTupleDesc}(tupdesc);
      blessed = true;
    }
    return tupdesc;
  }

#if PG_MAJORVERSION_NUM > 16
  /**
   * @brief Determines whether two tuple descriptors have equal row types.
   *
   * This is used to check whether two record types are compatible, whether
   * function return row types are the same, and other similar situations.
   */
  bool equal_row_types(const tuple_descriptor &other) {
    return ffi_guard{::equalRowTypes}(tupdesc, other.tupdesc);
  }
#endif

  /**
   * @brief Determines whether two tuple descriptors have equal row types.
   *
   * This is used to check whether two record types are compatible, whether
   * function return row types are the same, and other similar situations.
   */
  bool equal_types(const tuple_descriptor &other) {
    if (tupdesc->natts != other.tupdesc->natts)
      return false;
    if (tupdesc->tdtypeid != other.tupdesc->tdtypeid)
      return false;

    for (int i = 0; i < other.attributes(); i++) {
      FormData_pg_attribute &att1 = operator[](i);
      FormData_pg_attribute &att2 = other[i];

      if (att1.atttypid != att2.atttypid || att1.atttypmod != att2.atttypmod ||
          att1.attcollation != att2.attcollation || att1.attisdropped != att2.attisdropped ||
          att1.attlen != att2.attlen || att1.attalign != att2.attalign) {
        return false;
      }
    }

    return true;
  }

  /**
   * @brief Compare two TupleDesc structures for logical equality
   *
   * @note This includes checking attribute names.
   */
  bool operator==(const tuple_descriptor &other) {
    return ffi_guard{::equalTupleDescs}(tupdesc, other.tupdesc);
  }

  /**
   * @brief Returns true if the tuple descriptor is blessed
   */
  bool is_blessed() const { return blessed; }

private:
  inline void check_bounds(int n) const {
    if (n + 1 > tupdesc->natts || n < 0) {
      throw std::out_of_range(std::format(
          "attribute index {} is out of bounds for the tuple descriptor with the size of {}", n,
          tupdesc->natts));
    }
  }
  inline void check_not_blessed() const {
    if (blessed) {
      throw std::runtime_error("tuple_descriptor already blessed");
    }
  }

  TupleDesc tupdesc;
  bool blessed;
};

/**
 * @brief Runtime-typed value of `record` type
 *
 * These records don't have a structure known upfront, for example, when a value
 * of this type (`record`) are passed into a function.
 */
struct record {

  friend struct datum_conversion<record>;

  record(HeapTupleHeader heap_tuple, abstract_memory_context &ctx)
      : tupdesc(ffi_guard{::lookup_rowtype_tupdesc}(HeapTupleHeaderGetTypeId(heap_tuple),
                                                    HeapTupleHeaderGetTypMod(heap_tuple))),
        tuple(ctx.template alloc<HeapTupleData>()) {
#if PG_MAJORVERSION_NUM < 18
    tuple->t_len = HeapTupleHeaderGetDatumLength(tupdesc);
#else
    tuple->t_len = HeapTupleHeaderGetDatumLength(heap_tuple);
#endif
    tuple->t_data = heap_tuple;
  }
  record(HeapTupleHeader heap_tuple, abstract_memory_context &&ctx) : record(heap_tuple, ctx) {}

  template <std::input_iterator Iter>
  requires convertible_into_nullable_datum<typename std::iterator_traits<Iter>::value_type>
  record(tuple_descriptor &tupdesc, Iter begin, Iter end)
      : tupdesc(tupdesc), tuple([&]() {
          std::vector<::Datum> values;
          std::vector<uint8_t> nulls;
          for (auto it = begin; it != end; ++it) {
            auto nd = into_nullable_datum(*it);
            values.push_back(nd.is_null() ? ::Datum(0) : nd);
            nulls.push_back(nd.is_null() ? 1 : 0);
          }
          return ffi_guard{::heap_form_tuple}(this->tupdesc, values.data(),
                                              reinterpret_cast<bool *>(nulls.data()));
        }()) {}

  template <convertible_into_nullable_datum... D>
  record(tuple_descriptor &tupdesc, D &&...args)
      : tupdesc(tupdesc), tuple([&]() {
          std::array<nullable_datum, sizeof...(D)> datums = {
              cppgres::into_nullable_datum(std::move(args))...};
          std::array<bool, sizeof...(D)> nulls;
          std::ranges::copy(datums | std::views::transform([](auto &v) { return v.is_null(); }),
                            nulls.begin());
          std::array<::Datum, sizeof...(D)> values;
          std::ranges::copy(
              datums | std::views::transform([](auto &v) { return v.is_null() ? ::Datum(0) : v; }),
              values.begin());
          return ffi_guard{::heap_form_tuple}(this->tupdesc, values.data(), nulls.data());
        }()) {}

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
    datum d(heap_getattr(tuple, n + 1, tupdesc, &isnull));
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

  operator HeapTuple() const { return tuple; }

  /**
   * @brief Returns tuple descriptor
   */
  tuple_descriptor get_tuple_descriptor() const { return tupdesc; }

private:
  inline void check_bounds(int n) const {
    if (n + 1 > attributes() || n < 0) {
      throw std::out_of_range(std::format(
          "attribute index {} is out of bounds for record with the size of {}", n, attributes()));
    }
  }

  TupleDesc tupdesc;
  HeapTuple tuple;
};

template <> struct datum_conversion<record> {
  static record from_datum(const datum &d, std::optional<memory_context> ctx) {
    return {reinterpret_cast<HeapTupleHeader>(ffi_guard{::pg_detoast_datum}(
                reinterpret_cast<struct ::varlena *>(d.operator const ::Datum &()))),
            ctx.has_value() ? ctx.value() : memory_context()};
  }

  static datum into_datum(const record &t) { return datum(PointerGetDatum(t.tuple)); }
};

template <> struct type_traits<record> {
  static bool is(type &t) { return t.oid == RECORDOID; }
  static constexpr type type_for() { return {.oid = RECORDOID}; }
};

} // namespace cppgres
