#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "datum.h"
#include "guard.h"
#include "imports.h"
#include "utils/utils.h"

namespace cppgres {

struct type {
  ::Oid oid;

  std::string_view name() { return ::format_type_be(oid); }
};

template <typename T> struct type_traits {
  static bool is(type &t) {
    if constexpr (utils::is_optional<T>) {
      return type_traits<utils::remove_optional_t<T>>::is(t);
    } else {
      return false;
    }
  }
  static bool is(type &&t) { return is(std::move(t)); }

  static constexpr type type_for() = delete;
};

template <typename T> requires std::is_reference_v<T>
struct type_traits<T> {
  static constexpr type type_for() {
    return type_traits<std::remove_reference_t<utils::remove_optional_t<T>>>::type_for();
  }
};

struct non_by_value_type : public type {
  friend struct datum;

  non_by_value_type(std::pair<const struct datum &, std::optional<memory_context>> init)
      : non_by_value_type(init.first, init.second) {}
  non_by_value_type(const struct datum &datum, std::optional<memory_context> ctx)
      : value_datum(datum),
        ctx(tracking_memory_context(ctx.has_value() ? *ctx : top_memory_context)) {}

  non_by_value_type(const non_by_value_type &other)
      : value_datum(other.value_datum), ctx(other.ctx) {}
  non_by_value_type(non_by_value_type &&other) noexcept
      : value_datum(std::move(other.value_datum)), ctx(std::move(other.ctx)) {}
  non_by_value_type &operator=(non_by_value_type &&other) noexcept {
    value_datum = std::move(other.value_datum);
    ctx = std::move(other.ctx);
    return *this;
  }

  memory_context &get_memory_context() { return ctx.get_memory_context(); }

  datum get_datum() const { return value_datum; }

protected:
  datum value_datum;
  tracking_memory_context<memory_context> ctx;
  void *ptr(bool tracked = true) const {
    if (tracked && ctx.resets() > 0) {
      throw pointer_gone_exception();
    }
    return reinterpret_cast<void *>(value_datum.operator const ::Datum &());
  }
};

static_assert(std::copy_constructible<non_by_value_type>);

struct varlena : public non_by_value_type {
  using non_by_value_type::non_by_value_type;

  operator void *() { return VARDATA_ANY(detoasted_ptr()); }

  datum get_datum() const { return value_datum; }

  bool is_detoasted() const { return detoasted != nullptr; }

protected:
  void *detoasted = nullptr;
  // tracking_memory_context<memory_context> detoasted_ctx;
  void *detoasted_ptr() {
    if (detoasted != nullptr) {
      return detoasted;
    }
    detoasted = ffi_guarded(::pg_detoast_datum)(reinterpret_cast<::varlena *>(ptr()));
    // detoasted_ctx = tracking_memory_context(memory_context::for_pointer(detoasted));
    return detoasted;
  }
};

struct text : public varlena {
  using varlena::varlena;

  operator std::string_view() {
    return {static_cast<char *>(this->operator void *()), VARSIZE_ANY_EXHDR(this->detoasted_ptr())};
  }
};

using byte_array = std::span<const std::byte>;

struct bytea : public varlena {
  using varlena::varlena;

  operator byte_array() {
    return {reinterpret_cast<std::byte *>(this->operator void *()),
            VARSIZE_ANY_EXHDR(this->detoasted_ptr())};
  }
};

template <typename T>
concept flattenable = requires(T t, std::span<std::byte> span) {
  { T::type() } -> std::same_as<type>;
  { t.flat_size() } -> std::same_as<std::size_t>;
  { t.flatten_into(span) };
  { T::restore_from(span) } -> std::same_as<T>;
};

template <flattenable T> struct expanded_varlena : public varlena {
  using flattenable_type = T;
  using varlena::varlena;

  expanded_varlena()
      : varlena(([]() {
          auto ctx = memory_context(std::move(alloc_set_memory_context()));
          auto *e = ctx.alloc<expanded>();
          e->inner = T();
          init(&e->hdr, ctx);
          return std::make_pair(datum(PointerGetDatum(e)), ctx);
        })()),
        detoasted(true) {}

  operator T &() {
    auto *ptr = non_by_value_type::ptr();
    if (detoasted) {
      return (reinterpret_cast<expanded *>(ptr))->inner;
    } else {
      auto *ptr1 = reinterpret_cast<std::byte *>(varlena::operator void *());
      auto ctx = memory_context(std::move(alloc_set_memory_context()));
      auto *value = ctx.alloc<expanded>();
      init(&value->hdr, ctx);
      value->inner = T::restore_from(std::span(ptr1, VARSIZE_ANY_EXHDR(ptr)));
      detoasted = true;
      return value->inner;
    }
  }

  datum get_expanded_datum() const {
    if (!detoasted) {
      throw std::runtime_error("hasn't been expanded yet");
    }
    return datum(
        EOHPGetRWDatum(reinterpret_cast<::ExpandedObjectHeader *>(non_by_value_type::ptr())));
  }

private:
  bool detoasted = false;
  struct expanded {
    ::ExpandedObjectHeader hdr;
    T inner;
  };

  static void init(ExpandedObjectHeader *hdr, memory_context &ctx) {
    using header = int32_t;

    static const ::ExpandedObjectMethods eom = {
        .get_flat_size =
            [](ExpandedObjectHeader *eohptr) {
              auto *e = reinterpret_cast<expanded *>(eohptr);
              T *inner = &e->inner;
              return inner->flat_size() + sizeof(header);
            },
        .flatten_into =
            [](ExpandedObjectHeader *eohptr, void *result, size_t allocated_size) {
              auto *e = reinterpret_cast<expanded *>(eohptr);
              T *inner = &e->inner;
              SET_VARSIZE(reinterpret_cast<header *>(result), allocated_size);
              auto bytes = reinterpret_cast<std::byte *>(result) + sizeof(header);
              std::span buffer(bytes, allocated_size - sizeof(header));
              inner->flatten_into(buffer);
            }};

    ffi_guarded(::EOH_init_header)(hdr, &eom, ctx);
  }
};

template <typename T>
concept expanded_varlena_type = requires { typename T::flattenable_type; };

template <typename T>
concept has_a_type = requires {
  { type_traits<T>::type_for() } -> std::same_as<type>;
};

} // namespace cppgres
