#pragma once

#include "exception.h"
#include "guard.h"
#include "memory.h"

namespace cppgres {

pg_exception::pg_exception(::MemoryContext mcxt) : mcxt(mcxt) {
  ::CurrentMemoryContext = mcxt;
  error = ffi_guarded(::CopyErrorData)();
  ffi_guarded(::FlushErrorState)();
}

} // namespace cppgres
