/**
* \file
 */
#pragma once

extern "C" {
#include <setjmp.h>
}

#include <iostream>
#include <utility>

#include "error.hpp"
#include "exception.hpp"
#include "utils/function_traits.hpp"

namespace cppgres {

template <typename Func> struct ffi_guard {
  Func func;

  explicit ffi_guard(Func f) : func(std::move(f)) {}

  template <typename... Args>
  auto operator()(Args &&...args) -> decltype(func(std::forward<Args>(args)...)) {
    int state;
    sigjmp_buf *pbuf;
    ::ErrorContextCallback *cb;
    sigjmp_buf buf;
    ::MemoryContext mcxt = ::CurrentMemoryContext;

    pbuf = ::PG_exception_stack;
    cb = ::error_context_stack;
    ::PG_exception_stack = &buf;

    // restore state upon exit
    std::shared_ptr<void> defer(nullptr, [&](...) {
      ::error_context_stack = cb;
      ::PG_exception_stack = pbuf;
    });

    state = sigsetjmp(buf, 1);

    if (state == 0) {
      return func(std::forward<Args>(args)...);
    } else if (state == 1) {
      throw pg_exception();
    }
    __builtin_unreachable();
  }
};

/**
 * @brief Wraps a C++ function to catch exceptions and report them as Postgres errors
 *
 * It ensures that if the C++ exception throws an error, it'll be caught and transformed into
 * a Postgres error report.
 *
 * @note It will also handle Postgres errors caught during the call that were automatically transformed
 *       into @ref cppgres::pg_exception by @ref cppgres::ffi_guard and report them as errors.
 *
 * @tparam Func C++ function to call
 */
template <typename Func> struct exception_guard {
  Func func;

  explicit exception_guard(Func f) : func(std::move(f)) {}

  template <typename... Args>
  auto operator()(Args &&...args) -> decltype(func(std::forward<Args>(args)...)) {
    bool pg_exception = false;
    char *str_err = nullptr;
    bool other_error = false;
    try {
      return func(std::forward<Args>(args)...);
    } catch (const pg_exception &e) {
      pg_exception = true;
    } catch (const std::exception &e) {
      str_err = pstrdup(e.what());
    } catch (...) {
      other_error = true;
    }
    if (pg_exception) {
      PG_RE_THROW();
    }
    if (str_err) {
      report(ERROR, "exception: %s", str_err);
    }
    if (other_error) {
      report(ERROR, "some exception occured");
    }
    __builtin_unreachable();
  }
};

} // namespace cppgres
