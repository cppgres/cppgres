#pragma once

namespace cppgres::utils::function_traits {
// Primary template (will be specialized below)
template <typename T> struct function_traits;

// Specialization for function pointers.
template <typename R, typename... Args> struct function_traits<R (*)(Args...)> {
  using argument_types = std::tuple<Args...>;
  static constexpr std::size_t arity = sizeof...(Args);
};

// Specialization for function references.
template <typename R, typename... Args> struct function_traits<R (&)(Args...)> {
  using argument_types = std::tuple<Args...>;
  static constexpr std::size_t arity = sizeof...(Args);
};

// Specialization for function types themselves.
template <typename R, typename... Args> struct function_traits<R(Args...)> {
  using argument_types = std::tuple<Args...>;
  static constexpr std::size_t arity = sizeof...(Args);
};

// Specialization for member function pointers (e.g. for lambdas' operator())
template <typename C, typename R, typename... Args>
struct function_traits<R (C:: *)(Args...) const> {
  using argument_types = std::tuple<Args...>;
  static constexpr std::size_t arity = sizeof...(Args);
};

// Fallback for functors/lambdas that are not plain function pointers.
// This will delegate to the member function pointer version.
template <typename T> struct function_traits : function_traits<decltype(&T::operator())> {};
} // namespace cppgres::utils::function_traits
