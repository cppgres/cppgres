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
  using types = utils::function_traits::function_traits<decltype(func)>::argument_types;

  ffi_guard(Func f) : func(std::move(f)) {}

  template <typename... Args>
  auto operator()(Args &&...args) -> decltype(func(std::forward<Args>(args)...)) {
    using return_type = decltype(func(std::forward<Args>(args)...));
    types t;

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      (([&] { std::get<Is>(t) = args; }()), ...);
    }(std::make_index_sequence<std::tuple_size_v<decltype(t)>>{});

    int state;
    sigjmp_buf *pbuf;
    ::ErrorContextCallback *cb;
    sigjmp_buf buf;
    ::MemoryContext mcxt = ::CurrentMemoryContext;

    pbuf = ::PG_exception_stack;
    cb = ::error_context_stack;
    ::PG_exception_stack = &buf;

    state = sigsetjmp(buf, 1);
    if (state == 0) {
      if constexpr (std::is_void_v<return_type>) {
        std::apply(func, t);
        ::error_context_stack = cb;
        ::PG_exception_stack = pbuf;
        return;
      } else {
        auto result = std::apply(func, t);
        ::error_context_stack = cb;
        ::PG_exception_stack = pbuf;
        return result;
      }
    } else if (state == 1) {
      ::error_context_stack = cb;
      ::PG_exception_stack = pbuf;
      throw pg_exception(mcxt);
    }
    __builtin_unreachable();
  }
};

template <typename Func> auto ffi_guarded(Func f) { return ffi_guard<Func>{f}; }

} // namespace cppgres
