#pragma once

#include "datum.hpp"
#include "imports.h"

namespace cppgres {

template <typename T, typename... Args>
concept aggregate =
    /*all_from_nullable_datum<Args...>() &&*/ convertible_from_nullable_datum<typename T::type> &&
    convertible_into_nullable_datum<typename T::type> &&
    requires(T agg, typename T::type &&typ, Args... args) {
      { T(std::move(typ)) };
      { agg.operator typename T::type() } -> std::same_as<typename T::type>;
      { agg.state_transition(std::forward<Args>(args)...) } -> std::same_as<T &>;
    };

template <typename A, typename... Arg> requires aggregate<A, Arg...>
typename A::type aggregate_function(typename A::type agg, Arg... args) {
  return A(std::move(agg)).state_transition(args...);
}
} // namespace cppgres
