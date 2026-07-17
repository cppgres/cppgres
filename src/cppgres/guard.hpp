/**
* \file
 */
#pragma once

extern "C" {
#include <setjmp.h>
}

#include <exception>
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
      throw pg_exception(mcxt);
    }
    __builtin_unreachable();
  }
};

/**
 * @brief Runs a callable under @ref cppgres::ffi_guard, degrading any failure to a warning
 *
 * Catches every exception — including Postgres errors surfaced as
 * @ref cppgres::pg_exception — and reports the given message with `elog(WARNING)` instead of
 * propagating. Intended for destructors and other cleanup paths where failure must degrade
 * to a WARNING instead of propagating.
 *
 * @param f callable to run
 * @param warning_message message to report at WARNING level if the callable fails
 */
template <typename Func> void ffi_guard_noexcept(Func f, const char *warning_message) noexcept {
  try {
    ffi_guard{std::move(f)}();
  } catch (...) {
    elog(WARNING, "%s", warning_message);
  }
}

/**
 * @brief Scope guard that runs the callable only when the scope is left via an exception
 *
 * A scope-fail guard (per `std::uncaught_exceptions`): the destructor compares the number of
 * in-flight exceptions against the count captured at construction and, only when the scope is
 * being unwound by an exception, runs the callable through @ref cppgres::ffi_guard_noexcept —
 * so the cleanup itself never throws, degrading any failure to a WARNING with the given
 * message.
 */
template <typename Func> struct scope_fail {
  Func func;
  const char *warning_message;
  int uncaught = std::uncaught_exceptions();

  scope_fail(Func f, const char *warning_message)
      : func(std::move(f)), warning_message(warning_message) {}

  ~scope_fail() {
    if (std::uncaught_exceptions() > uncaught) {
      ffi_guard_noexcept(func, warning_message);
    }
  }

  scope_fail(const scope_fail &) = delete;
  scope_fail &operator=(const scope_fail &) = delete;
  scope_fail(scope_fail &&) = delete;
  scope_fail &operator=(scope_fail &&) = delete;
};

template <typename Func> scope_fail(Func, const char *) -> scope_fail<Func>;

/**
 * @brief Scope guard that always runs the callable on scope exit
 *
 * Unlike @ref cppgres::scope_fail, the destructor runs the callable
 * unconditionally — on normal exit and during unwinding alike — through
 * @ref cppgres::ffi_guard_noexcept, so the cleanup itself never throws,
 * degrading any failure to a WARNING with the given message.
 */
template <typename Func> struct scope_exit {
  Func func;
  const char *warning_message;

  scope_exit(Func f, const char *warning_message)
      : func(std::move(f)), warning_message(warning_message) {}

  ~scope_exit() { ffi_guard_noexcept(func, warning_message); }

  scope_exit(const scope_exit &) = delete;
  scope_exit &operator=(const scope_exit &) = delete;
  scope_exit(scope_exit &&) = delete;
  scope_exit &operator=(scope_exit &&) = delete;
};

template <typename Func> scope_exit(Func, const char *) -> scope_exit<Func>;

/**
 * @brief Wraps a C++ function to catch exceptions and report them as Postgres errors
 *
 * It ensures that if the C++ exception throws an error, it'll be caught and transformed into
 * a Postgres error report.
 *
 * @note Postgres errors caught during the call that were automatically transformed into
 *       @ref cppgres::pg_exception by @ref cppgres::ffi_guard are rethrown to Postgres with
 *       full fidelity (SQLSTATE, detail, hint, context preserved) via
 *       @ref cppgres::pg_exception::rethrow.
 *
 * @tparam Func C++ function to call
 */
template <typename Func> struct exception_guard {
  Func func;

  explicit exception_guard(Func f) : func(std::move(f)) {}

  template <typename... Args>
  auto operator()(Args &&...args) -> decltype(func(std::forward<Args>(args)...)) {
    try {
      return func(std::forward<Args>(args)...);
    } catch (pg_exception &e) {
      e.rethrow();
    } catch (const std::exception &e) {
      report(ERROR, "exception: %s", e.what());
    } catch (...) {
      report(ERROR, "some exception occurred");
    }
    __builtin_unreachable();
  }
};

} // namespace cppgres
