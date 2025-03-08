#pragma once

#include "datum.h"
#include "guard.h"
#include "imports.h"

namespace cppgres {

template <typename T> struct syscache_traits {
  static constexpr ::SysCacheIdentifier cache_id = USERMAPPINGUSERSERVER;
};

template <> struct syscache_traits<Form_pg_type> {
  static constexpr ::SysCacheIdentifier cache_id = TYPEOID;
};

template <typename T>
concept syscached = requires(T t) {
  { *t };
  { syscache_traits<T>::cache_id } -> std::same_as<const ::SysCacheIdentifier &>;
};

template <syscached T, convertible_into_datum... D> struct syscache {
  syscache(const D &...key) : syscache(syscache_traits<T>::cache_id, key...) {}
  syscache(::SysCacheIdentifier cache_id, const D &...key)
      requires(sizeof...(key) > 0 && sizeof...(key) < 5)
  {
    datum keys[4] = {datum_conversion<D>::into_datum(key)...};
    tuple = ffi_guarded(::SearchSysCache)(cache_id, keys[0], keys[1], keys[2], keys[3]);

    if (!HeapTupleIsValid(tuple)) {
      throw std::runtime_error("invalid tuple");
    }
  }

  ~syscache() { ReleaseSysCache(tuple); }

  decltype(*std::declval<T>()) &operator*() { return *reinterpret_cast<T>(GETSTRUCT(tuple)); }
  const decltype(*std::declval<T>()) &operator*() const {
    return *reinterpret_cast<T>(GETSTRUCT(tuple));
  }

private:
  HeapTuple tuple;
};

} // namespace cppgres
