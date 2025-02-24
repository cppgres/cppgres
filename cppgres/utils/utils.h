#pragma once

#include <optional>

namespace cppgres::utils {

// Primary template: if T is not an optional, just yield T.
template <typename T> struct remove_optional {
  using type = T;
};

// Partial specialization for std::optional.
template <typename T> struct remove_optional<std::optional<T>> {
  using type = T;
};

// Convenience alias template.
template <typename T> using remove_optional_t = typename utils::remove_optional<T>::type;

template <typename T>
concept is_optional =
    requires { typename T::value_type; } && std::same_as<T, std::optional<typename T::value_type>>;

} // namespace cppgres::utils
