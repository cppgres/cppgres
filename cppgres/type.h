#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "datum.h"
#include "guard.h"
#include "imports.h"
#include "utils/utils.h"

extern "C" {
#include "utils/varlena.h"
#include "varatt.h"
}

namespace cppgres {
struct type {
  ::Oid oid;

  template <typename T> bool is() {
    if constexpr (utils::is_optional<T>) {
      return is<utils::remove_optional_t<T>>();
    } else {
      return false;
    }
  }

  std::string_view name() { return ::format_type_be(oid); }
};

struct non_by_value_type : public type {
  friend struct datum;

  non_by_value_type(struct datum &datum)
      : datum(datum), ctx(tracking_memory_context(memory_context::for_pointer(ptr(false)))) {}

  non_by_value_type(const non_by_value_type &other) : datum(other.datum), ctx(other.ctx) {}
  non_by_value_type(non_by_value_type &&other) noexcept
      : datum(std::move(other.datum)), ctx(std::move(other.ctx)) {}
  non_by_value_type &operator=(non_by_value_type &&other) noexcept {
    datum = std::move(other.datum);
    ctx = std::move(other.ctx);
    return *this;
  }

  memory_context memory_context() { return memory_context::for_pointer(ptr()); }

protected:
  datum datum;
  tracking_memory_context<cppgres::memory_context> ctx;
  void *ptr(bool tracked = true) {
    if (tracked && ctx.resets() > 0) {
      throw pointer_gone_exception();
    }
    return ffi_guarded(::pg_detoast_datum)(
        reinterpret_cast<struct ::varlena *>(datum.operator ::Datum &()));
  }
};

struct varlena : public non_by_value_type {
  using non_by_value_type::non_by_value_type;

  operator void *() { return VARDATA_ANY(ptr()); }
};

struct text : public varlena {
  using varlena::varlena;

  operator std::string_view() {
    void *value = *this;
    return {static_cast<char *>(value), VARSIZE_ANY_EXHDR(this->ptr())};
  }
};

using byte_array = std::span<const std::byte>;

struct bytea : public varlena {
  using varlena::varlena;

  operator byte_array() {
    void *value = *this;
    return {reinterpret_cast<std::byte *>(value), VARSIZE_ANY_EXHDR(this->ptr())};
  }
};

template <typename T> constexpr type type_for() {

  if constexpr (utils::is_optional<T>) {
    return type_for<utils::remove_optional_t<T>>();
  } else {
    return type{.oid = InvalidOid};
  }
}

} // namespace cppgres
