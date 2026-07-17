#pragma once

#include "tests.hpp"

namespace tests {

add_test(internal_subtransaction_one, ([](test_case &) {
           bool result = true;
           {
             cppgres::internal_subtransaction sub;
             {
               // subtransactions nest, as procedural-language exception
               // blocks do
               cppgres::internal_subtransaction inner;
             }
           }
           {
             // doing it again is fine
             cppgres::internal_subtransaction sub;
           }
           return result;
         }));

add_test(internal_subtransaction_commit, ([](test_case &) {
           bool result = true;
           {
             cppgres::spi_executor spi;
             spi.execute("create table internal_subtransaction_commit ()");
           }
           {
             cppgres::internal_subtransaction sub;
             cppgres::spi_executor spi;
             spi.execute("insert into internal_subtransaction_commit default values");
           }
           cppgres::spi_executor spi;
           auto res = spi.query<std::tuple<int64_t>>(
               "select count(*) from internal_subtransaction_commit");
           result = result && _assert(std::get<0>(res.begin()[0]) == 1);
           return result;
         }));

add_test(internal_subtransaction_rollback, ([](test_case &) {
           bool result = true;
           {
             cppgres::spi_executor spi;
             spi.execute("create table internal_subtransaction_rollback ()");
           }
           {
             cppgres::internal_subtransaction sub(false);
             cppgres::spi_executor spi;
             spi.execute("insert into internal_subtransaction_rollback default values");
           }
           cppgres::spi_executor spi;
           auto res = spi.query<std::tuple<int64_t>>(
               "select count(*) from internal_subtransaction_rollback");
           result = result && _assert(std::get<0>(res.begin()[0]) == 0);
           return result;
         }));
} // namespace tests
