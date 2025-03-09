/**
* \file
 */
#pragma once

#include "exception.hpp"
#include "guard.hpp"
#include "memory.hpp"

namespace cppgres {

pg_exception::pg_exception(::MemoryContext mcxt) : mcxt(mcxt) {
  ::CurrentMemoryContext = mcxt;
  error = ffi_guarded(::CopyErrorData)();
  ffi_guarded(::FlushErrorState)();
}

} // namespace cppgres
