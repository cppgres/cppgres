#pragma once

#include <cppgres.hpp>
#include <unordered_map>

struct test_case {
  inline static std::unordered_map<std::string_view, test_case *> test_cases =
      std::unordered_map<std::string_view, test_case *>{};
  test_case(std::string_view name, bool (*function)(test_case &c));
  bool operator()();
  bool (*function)(test_case &c);
};

#define add_test(name, code) static test_case t__##name(std::string_view(#name), code);

#define _assert(expr)                                                                              \
  ({                                                                                               \
    auto value = (expr);                                                                           \
    if (!value) {                                                                                  \
      cppgres::report(NOTICE, "assertion failure %s:%d (`%s`): %s", __FILE_NAME__, __LINE__,       \
                      __func__, #expr);                                                            \
    }                                                                                              \
    value;                                                                                         \
  })

static const char *get_library_name();
