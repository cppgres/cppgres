#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "datum.h"
#include "guard.h"
#include "imports.h"
#include "utils/utils.h"

namespace cppgres {

struct type;

template <typename T> struct type_traits {
  static bool is(type &t) {
    if constexpr (utils::is_optional<T>) {
      return type_traits<utils::remove_optional_t<T>>::is(t);
    } else {
      return false;
    }
  }
  static bool is(type &&t) { return is(std::move(t)); }
};

struct type {
  ::Oid oid;

  std::string_view name() { return ::format_type_be(oid); }
};

struct non_by_value_type : public type {
  friend struct datum;

  non_by_value_type(const struct datum &datum)
      : value_datum(datum), ctx(tracking_memory_context(memory_context::for_pointer(ptr(false)))) {}

  non_by_value_type(const non_by_value_type &other)
      : value_datum(other.value_datum), ctx(other.ctx) {}
  non_by_value_type(non_by_value_type &&other) noexcept
      : value_datum(std::move(other.value_datum)), ctx(std::move(other.ctx)) {}
  non_by_value_type &operator=(non_by_value_type &&other) noexcept {
    value_datum = std::move(other.value_datum);
    ctx = std::move(other.ctx);
    return *this;
  }

  memory_context get_memory_context() { return memory_context::for_pointer(ptr()); }

  datum get_datum() const { return value_datum; }

protected:
  datum value_datum;
  tracking_memory_context<cppgres::memory_context> ctx;
  void *ptr(bool tracked = true) {
    if (tracked && ctx.resets() > 0) {
      throw pointer_gone_exception();
    }
    return reinterpret_cast<void *>(value_datum.operator const ::Datum &());
  }
};

struct varlena : public non_by_value_type {
  using non_by_value_type::non_by_value_type;

  operator void *() { return VARDATA_ANY(ptr()); }

protected:
  void *ptr(bool tracked = true) {
    void *datum_ptr = non_by_value_type::ptr(tracked);
    return ffi_guarded(::pg_detoast_datum)(reinterpret_cast<struct ::varlena *>(datum_ptr));
  }
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

template <typename T>
concept flattenable = requires(T t, std::span<std::byte> span) {
  { T() } -> std::same_as<T>;
  { t.flat_size() } -> std::same_as<std::size_t>;
  { t.flatten_into(span) };
};

template <flattenable T> struct expanded_varlena : public varlena {
  using varlena::varlena;

  expanded_varlena()
      : varlena(({
          auto ctx = memory_context(std::move(alloc_set_memory_context()));
          auto *e = ctx.alloc<expanded>();
          e->inner = T();
          init(&e->hdr, ctx);
          report(NOTICE, "ptr %d", reinterpret_cast<uintptr_t>(e));
          report(NOTICE, "vptr0 %d", reinterpret_cast<uintptr_t>(&e->hdr));
          datum(::EOHPGetRWDatum(&e->hdr));
        })) {}

  operator T *() {
    expanded *ptr = reinterpret_cast<expanded *>(non_by_value_type::ptr());
    if (VARATT_IS_EXTERNAL_EXPANDED(ptr)) {
      return reinterpret_cast<T *>(ffi_guarded(::DatumGetEOHP)(this->value_datum));
    } else {
      // ensure it is detoasted
      //      expanded *ptr = reinterpret_cast<expanded *>(varlena::ptr());
      //      ptr = varlena::operator void *();
      auto ctx = memory_context(std::move(alloc_set_memory_context()));
      auto *value = ctx.alloc<expanded>();
      init(&value->hdr, ctx);
      return &value->inner;
    }
  }

  expanded_varlena(datum datum) : varlena(datum) {}

private:
  struct expanded {
    ::ExpandedObjectHeader hdr;
    T inner;
  };

  static void init(ExpandedObjectHeader *hdr, memory_context &ctx) {

    const ::ExpandedObjectMethods eom = {
        // get flat size
        [](ExpandedObjectHeader *eohptr) {
          auto *e = reinterpret_cast<expanded *>(eohptr);
          T *inner = &e->inner;
          return inner->flat_size();
        },
        // flatten into
        [](ExpandedObjectHeader *eohptr, void *result, size_t allocated_size) {
          auto *e = reinterpret_cast<expanded *>(eohptr);
          T *inner = &e->inner;
          auto bytes = reinterpret_cast<std::byte *>(result);
          std::span<std::byte> buffer(bytes, allocated_size);
          return inner->flatten_into(buffer);
        }};

    ffi_guarded(::EOH_init_header)(hdr, &eom, ctx);
  }
};

template <typename T> constexpr type type_for() {
  return type_for<std::remove_reference_t<utils::remove_optional_t<T>>>();
}

template <typename T>
concept has_a_type = requires {
  { type_for<T>() } -> std::same_as<type>;
};

} // namespace cppgres
