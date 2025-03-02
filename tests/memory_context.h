#pragma once

#include "tests.h"

namespace tests {

add_test(alloc_set_context, ([](test_case &) {
           bool result = true;
           cppgres::alloc_set_memory_context c;
           result = result && _assert(c.alloc(100));
           return result;
         }));

add_test(allocator, ([](test_case &) {
           bool result = true;

           cppgres::memory_context_allocator<std::string> alloc0;
           result = result && _assert(alloc0.memory_context() == cppgres::memory_context());

           auto old_ctx = ::CurrentMemoryContext;
           auto ctx = cppgres::alloc_set_memory_context();
           ::CurrentMemoryContext = ctx;
           result = result && _assert(alloc0.memory_context() != cppgres::memory_context());
           ::CurrentMemoryContext = old_ctx;

           cppgres::memory_context_allocator<std::string> alloc(
               std::move(cppgres::top_memory_context), true);
           std::vector<std::string, cppgres::memory_context_allocator<std::string>> vec(alloc);
           vec.emplace_back("a");

           return result;
         }));

add_test(current_memory_context, ([](test_case &) {
           bool result = true;

           cppgres::always_current_memory_context ctx;
           cppgres::memory_context ctx0(ctx);
           result = result && _assert(ctx == ctx0);

           ::CurrentMemoryContext = ::TopMemoryContext;

           result = result && _assert(ctx != ctx0);

           return result;
         }));

add_test(memory_context_for_ptr, ([](test_case &) {
           return _assert(cppgres::memory_context::for_pointer(::palloc0(100)) ==
                          cppgres::memory_context());
         }));

add_test(memory_context_scope, ([](test_case &) {
           bool result = true;
           cppgres::memory_context before;
           {
             [[maybe_unused]] auto scope =
                 cppgres::memory_context_scope(cppgres::top_memory_context);
             cppgres::memory_context now;
             result = result && _assert(now == cppgres::top_memory_context);
             result = result && _assert(now != before);
           }
           cppgres::memory_context now;
           result = result && _assert(now == before);
           return result;
         }));
} // namespace tests
