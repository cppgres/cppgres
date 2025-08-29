#pragma once

#include "tests.hpp"

namespace tests {

add_test(interrupt_holdoff_count, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           cppgres::internal_subtransaction tx;
           bool exception_raised = false;

           cppgres::ffi_guard{::LWLockAcquire}(AddinShmemInitLock, LW_EXCLUSIVE);
           result = result && _assert(::InterruptHoldoffCount > 0);

           try {
             cppgres::internal_subtransaction tx;
             spi.execute("create tabl a()");
           } catch (std::exception &e) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);
           result = result && _assert(::InterruptHoldoffCount > 0);

           cppgres::ffi_guard{::LWLockRelease}(AddinShmemInitLock);

           return result;
         }));

} // namespace tests
