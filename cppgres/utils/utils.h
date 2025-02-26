#pragma once

#include <optional>
#include <string_view>

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

template <typename T> constexpr std::string_view type_name() {
#ifdef __clang__
  constexpr std::string_view p = __PRETTY_FUNCTION__;
  constexpr std::string_view key = "T = ";
  const auto start = p.find(key) + key.size();
  const auto end = p.find(']', start);
  return p.substr(start, end - start);
#elif defined(__GNUC__)
  constexpr std::string_view p = __PRETTY_FUNCTION__;
  constexpr std::string_view key = "T = ";
  const auto start = p.find(key) + key.size();
  const auto end = p.find(';', start);
  return p.substr(start, end - start);
#elif defined(_MSC_VER)
  constexpr std::string_view p = __FUNCSIG__;
  constexpr std::string_view key = "type_name<";
  const auto start = p.find(key) + key.size();
  const auto end = p.find(">(void)");
  return p.substr(start, end - start);
#else
  return "Unsupported compiler";
#endif
}
} // namespace cppgres::utils
