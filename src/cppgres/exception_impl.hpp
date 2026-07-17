/**
* \file
 */
#pragma once

#include "exception.hpp"
#include "guard.hpp"
#include "memory.hpp"

namespace cppgres {

inline pg_exception::pg_exception(::MemoryContext mcxt) : mcxt(mcxt) {
  ::CurrentMemoryContext = error_cxt =
      memory_context(std::move(alloc_set_memory_context(top_memory_context())));
  error = ffi_guard{::CopyErrorData}();
  ::CurrentMemoryContext = mcxt;
  ffi_guard{::FlushErrorState}();
}

inline pg_exception::~pg_exception() { memory_context(error_cxt).delete_context(); }

[[noreturn]] inline void pg_exception::rethrow() {
  // ReThrowError copies the record into Postgres' error stack (strings into
  // ErrorContext) and then longjmps, which skips ~pg_exception.  Register a
  // reset callback on ErrorContext so our backing context is freed once the
  // error has been fully handled (FlushErrorState resets ErrorContext).
  auto *cb = static_cast<::MemoryContextCallback *>(
      ffi_guard{::MemoryContextAlloc}(::ErrorContext, sizeof(::MemoryContextCallback)));
  cb->func = [](void *arg) { ::MemoryContextDelete(static_cast<::MemoryContext>(arg)); };
  cb->arg = error_cxt;
  ::MemoryContextRegisterResetCallback(::ErrorContext, cb);
  ::ReThrowError(error);
  __builtin_unreachable();
}

} // namespace cppgres
