/**
* \file
 */
#pragma once

extern "C" {
#include <setjmp.h>
}

#include <iostream>
#include <utility>

#include "error.h"
#include "exception.h"
#include "utils/function_traits.h"

namespace cppgres {

template <typename Func> struct ffi_guard {
  Func func;

  ffi_guard(Func f) : func(std::move(f)) {}

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

template <typename Func> auto ffi_guarded(Func f) { return ffi_guard<Func>{f}; }

} // namespace cppgres
