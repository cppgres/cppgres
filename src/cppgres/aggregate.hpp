#pragma once

#include "function.hpp"
#include "imports.h"

namespace cppgres {

template <class T, class... Args>
concept aggregate = requires(T t, Args &&...args) {
  { t.update(args...) };
};

template <class T, class... Args>
concept aggregate_with_finalfunc = aggregate<T, Args...> && requires(T t) {
  { t.finalize() } -> convertible_into_datum;
};

template <class Agg, typename... InTs> datum aggregate_sfunc(value state, InTs... args) {

  MemoryContext aggctx;
  if (!ffi_guard{::AggCheckCallContext}(current_postgres_function::call_info().operator*(),
                                        &aggctx)) {
    report(ERROR, "not aggregate context");
  }

  if constexpr (!convertible_into_datum<Agg> && aggregate_with_finalfunc<Agg, InTs...>) {
    Agg *state0;
    if (state.get_nullable_datum().is_null()) {
      state0 = memory_context(aggctx).alloc<Agg>();
      std::construct_at(state0);
    } else {
      state0 = reinterpret_cast<Agg *>(
          from_nullable_datum<void *>(state.get_nullable_datum(), state.get_type().oid));
    }

    state0->update(args...);

    return datum_conversion<void *>::into_datum(reinterpret_cast<void *>(state0));
  } else if constexpr (convertible_into_datum<Agg>) {
    Agg state0 = datum_conversion<Agg>::from_nullable_datum(state.get_nullable_datum(), ANYOID);
    state0.update(args...);
    return datum_conversion<Agg>::into_datum(state0);
  }
  report(ERROR, "not supported");
  __builtin_unreachable();
}

template <class Agg, typename... InTs> nullable_datum aggregate_ffunc(value state) {
  if constexpr (aggregate_with_finalfunc<Agg, InTs...>) {
    Agg *state0;
    state0 = reinterpret_cast<Agg *>(
        from_nullable_datum<void *>(state.get_nullable_datum(), state.get_type().oid));
    return into_nullable_datum(state0->finalize());
  } else {
    report(ERROR, "this aggregate does not support final function");
    __builtin_unreachable();
  }
}

} // namespace cppgres

#define declare_aggregate(name, typname, ...)                                                      \
  static_assert(::cppgres::aggregate<typname, ##__VA_ARGS__>);                                     \
  static_assert(::cppgres::convertible_into_datum<typname> ||                                      \
                    ::cppgres::aggregate_with_finalfunc<typname, ##__VA_ARGS__>,                   \
                "must be convertible to datum or have finalize()");                                \
  postgres_function(name##_sfunc, (cppgres::aggregate_sfunc<typname, ##__VA_ARGS__>));             \
  postgres_function(name##_ffunc, (cppgres::aggregate_ffunc<typname, ##__VA_ARGS__>));
