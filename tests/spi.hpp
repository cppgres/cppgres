#pragma once

#include "tests.hpp"

extern "C" {
#include <executor/spi_priv.h>
}

namespace tests {

add_test(spi, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto res = spi.query<std::tuple<std::optional<int64_t>>>(
               "select $1 + i from generate_series(1,100) i", static_cast<int64_t>(1LL));

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(std::get<0>(re) == i + 1);
           }
           result = result && _assert(std::get<0>(res.begin()[0]) == 2);
           return result;
         }));

add_test(spi_single, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto res = spi.query<int64_t>("select $1 + i from generate_series(1,100) i",
                                         static_cast<int64_t>(1LL));

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(re == i + 1);
           }
           result = result && _assert(res.begin()[0] == 2);
           return result;
         }));

add_test(spi_empty, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;

           {
             auto res = spi.query<std::tuple<>>("select from generate_series(1,100) i",
                                                static_cast<int64_t>(1LL));
             result = result && _assert(res.count() == 100);
           }

           {
             struct res_t {};
             auto res = spi.query<res_t>("select from generate_series(1,100) i",
                                         static_cast<int64_t>(1LL));
             result = result && _assert(res.count() == 100);
           }

           {
             auto res = spi.query<std::vector<cppgres::value>>(
                 "select from generate_series(1,100) i", static_cast<int64_t>(1LL));
             result = result && _assert(res.count() == 100);
             result = result && _assert(res.begin()[0].size() == 0);
           }

           return result;
         }));

add_test(spi_plural_tuple, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto res = spi.query<std::tuple<int64_t, int64_t>>(
               "select $1 + i, i from generate_series(1,100) i", static_cast<int64_t>(1LL));

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(std::get<0>(re) == i + 1);
             result = result && _assert(std::get<1>(re) == i);
           }
           return result;
         }));

add_test(spi_vector, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto res = spi.query<std::vector<int64_t>>(
               "select $1 + i, i from generate_series(1,100) i", static_cast<int64_t>(1LL));

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(re[0] == i + 1);
             result = result && _assert(re[1] == i);
           }
           return result;
         }));

add_test(spi_vector_mismatch, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;

           bool exception_raised = false;

           try {
             spi.query<std::vector<int64_t>>("select $1 + i, 'test' from generate_series(1,100) i",
                                             static_cast<int64_t>(1LL));
           } catch (std::invalid_argument) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_pfr, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           struct my_struct {
             int64_t val;
           };
           auto res = spi.query<my_struct>("select $1 + i from generate_series(1,100) i",
                                           static_cast<int64_t>(1LL));

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(re.val == i + 1);
           }
           return result;
         }));

add_test(spi_non_opt, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto res = spi.query<std::tuple<int64_t>>("select $1 + i from generate_series(1,100) i",
                                                     static_cast<int64_t>(1LL));

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(std::get<0>(re) == i + 1);
           }
           result = result && _assert(std::get<0>(res.begin()[0]) == 2);

           bool exception_raised = false;
           try {
             spi.query<std::tuple<int64_t>>("select null from generate_series(1,100) i",
                                            static_cast<int64_t>(1LL));
           } catch (std::exception &e) {
             exception_raised = true;
           }
           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_argless, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto res = spi.query<std::tuple<std::optional<int64_t>>>(
               "select i from generate_series(1,100) i");

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(std::get<0>(re) == i);
           }
           result = result && _assert(std::get<0>(res.begin()[0]) == 1);

           auto plan = spi.plan("select i from generate_series(1,100) i");
           auto res1 = spi.query<std::tuple<std::optional<int64_t>>>(plan);
           result = result && _assert(std::get<0>(res1.begin()[1]) == 2);

           return result;
         }));

add_test(spi_type_mismatch, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;

           bool exception_raised = false;
           try {
             spi.query<std::tuple<std::optional<bool>>>(
                 "select $1 + i from generate_series(1,100) i", static_cast<int64_t>(1LL));
           } catch (std::invalid_argument &e) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_plan, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto plan = spi.plan<int64_t>("select $1 + i from generate_series(1,100) i");
           auto res =
               spi.query<std::tuple<std::optional<int64_t>>>(plan, static_cast<int64_t>(1LL));

           int i = 0;
           for (auto &re : res) {
             i++;
             result = result && _assert(std::get<0>(re) == i + 1);
           }
           result = result && _assert(std::get<0>(res.begin()[0]) == 2);
           return result;
         }));

add_test(spi_plan_mismatch, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto plan = spi.plan<int64_t>("select $1 + i from generate_series(1,100) i");

           bool exception_raised = false;
           try {
             spi.query<std::tuple<std::optional<bool>>>(plan, static_cast<int64_t>(1LL));
           } catch (std::invalid_argument &e) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_plan_gone, ([](test_case &) {
           bool result = true;

           auto plan = ({
             cppgres::spi_executor spi;
             spi.plan<int64_t>("select $1 + i from generate_series(1,100) i");
           });

           cppgres::spi_executor spi;

           bool exception_raised = false;
           try {
             spi.query<std::tuple<std::optional<int64_t>>>(plan, static_cast<int64_t>(1LL));
           } catch (cppgres::pointer_gone_exception &e) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_keep_plan, ([](test_case &) {
           bool result = true;

           ::SPIPlanPtr ptr = nullptr;
           auto context = ({
             auto plan = ({
               cppgres::spi_executor spi;
               auto plan = spi.plan<int64_t>("select $1 + i from generate_series(1,100) i");
               plan.keep();
               std::move(plan);
             });

             ptr = plan;

             cppgres::spi_executor spi;
             auto res =
                 spi.query<std::tuple<std::optional<int64_t>>>(plan, static_cast<int64_t>(1LL));

             int i = 0;
             for (auto &re : res) {
               i++;
               result = result && _assert(std::get<0>(re) == i + 1);
             }
             result = result && _assert(std::get<0>(res.begin()[0]) == 2);
             auto p = static_cast<_SPI_plan *>(ptr);
             cppgres::tracking_memory_context(cppgres::memory_context(p->plancxt));
           });

           // Now the plan should be gone – destroyed and inaccessible
           context.resets();
           result = result && _assert(context.resets() > 0);

           return result;
         }));

add_test(spi_interleave, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;

           // Nesting: query by string
           {
             cppgres::spi_executor spi1;

             bool exception_raised = false;
             try {
               spi.query<std::tuple<std::optional<int64_t>>>(
                   "select $1 + i from generate_series(1,100) i", static_cast<int64_t>(1LL));
             } catch (std::runtime_error &e) {
               exception_raised = true;
             }

             result = result && _assert(exception_raised);
           }

           // fine here
           spi.query<std::tuple<std::optional<int64_t>>>(
               "select $1 + i from generate_series(1,100) i", static_cast<int64_t>(1LL));

           // Nesting: plan
           {
             cppgres::spi_executor spi1;

             bool exception_raised = false;
             try {
               auto plan = spi.plan<int64_t>("select $1 + i from generate_series(1,100) i");
             } catch (std::runtime_error &e) {
               exception_raised = true;
             }

             result = result && _assert(exception_raised);
           }

           // fine here
           auto plan = spi.plan<int64_t>("select $1 + i from generate_series(1,100) i");

           // Nesting: query by plan
           {
             cppgres::spi_executor spi1;

             bool exception_raised = false;
             try {
               spi.query<std::tuple<std::optional<int64_t>>>(plan, static_cast<int64_t>(1LL));
             } catch (std::runtime_error &e) {
               exception_raised = true;
             }

             result = result && _assert(exception_raised);
           }

           // fine here
           spi.query<std::tuple<std::optional<int64_t>>>(plan, static_cast<int64_t>(1LL));

           return result;
         }));

add_test(spi_execute, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;
           auto res = spi.execute("create table spi_execute_test (v int)");
           result = result && _assert(res == 0);

           auto res1 = spi.execute(
               "insert into spi_execute_test (v) select $1 from generate_series(1,100)", 1);
           result = result && _assert(res1 == 100);

           return result;
         }));

add_test(spi_ptr_gone, ([](test_case &) {
           bool result = true;
           cppgres::text res = []() {
             cppgres::spi_executor spi;
             return spi.query<cppgres::text>("select 'hello'").begin()[0];
           }();
           bool exception_raised = false;
           try {
             res.operator std::string_view();
           } catch (cppgres::pointer_gone_exception &e) {
             exception_raised = true;
           }
           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_value_type, ([](test_case &) {
           bool result = true;

           {
             cppgres::spi_executor spi;
             auto res = spi.query<cppgres::value>("select $1 + i from generate_series(1,100) i",
                                                  static_cast<int64_t>(1LL));

             int i = 0;
             for (auto &re : res) {
               i++;
               result = result && _assert(re.get_type() == cppgres::type{.oid = INT8OID});

               result = result && _assert(cppgres::from_nullable_datum<int64_t>(
                                              re.get_nullable_datum(), re.get_type().oid) == i + 1);
             }
           }

           {
             cppgres::spi_executor spi;
             auto res = spi.query<std::tuple<cppgres::value>>(
                 "select $1 + i from generate_series(1,100) i", static_cast<int64_t>(1LL));

             int i = 0;
             for (auto &re : res) {
               i++;
               result =
                   result && _assert(std::get<0>(re).get_type() == cppgres::type{.oid = INT8OID});

               result = result && _assert(cppgres::from_nullable_datum<int64_t>(
                                              std::get<0>(re).get_nullable_datum(),
                                              std::get<0>(re).get_type().oid) == i + 1);
             }
           }

           {
             cppgres::spi_executor spi;
             auto res = spi.query<std::vector<cppgres::value>>(
                 "select $1 + i from generate_series(1,100) i", static_cast<int64_t>(1LL));

             int i = 0;
             for (auto &re : res) {
               i++;
               result = result && _assert(re[0].get_type() == cppgres::type{.oid = INT8OID});

               result = result &&
                        _assert(cppgres::from_nullable_datum<int64_t>(
                                    re[0].get_nullable_datum(), re[0].get_type().oid) == i + 1);
             }
           }

           return result;
         }));

add_test(spi_tupdesc_access, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           auto res = spi.query<std::tuple<cppgres::value, cppgres::value>>(
               "select 1 as a, 'a'::text as b");

           auto td = res.get_tuple_descriptor();
           result = result && _assert(td.get_name(0) == "a");
           result = result && _assert(td.get_type(0).oid == INT4OID);
           result = result && _assert(td.get_name(1) == "b");
           result = result && _assert(td.get_type(1).oid == TEXTOID);

           return result;
         }));

add_test(spi_readonly, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           spi.execute("create table q (i int)");

           bool exception_raised = false;

           try {
             spi.query<int>("insert into q default values returning i",
                            cppgres::spi_executor::options(true));
           } catch (...) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_execute_readonly, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;

           bool exception_raised = false;

           try {
             spi.execute("create table q (i int)", cppgres::spi_executor::options(true));
           } catch (...) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_plan_readonly, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           spi.execute("create table q (i int)");

           bool exception_raised = false;

           auto plan = spi.plan("insert into q default values returning i");

           try {
             spi.query<int>(plan, cppgres::spi_executor::options(true));
           } catch (...) {
             exception_raised = true;
           }

           result = result && _assert(exception_raised);

           return result;
         }));

add_test(spi_count, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           auto res = spi.query<int>("select i from generate_series(1,10) i",
                                     cppgres::spi_executor::options(2));

           result = result && _assert(res.count() == 2);

           return result;
         }));

add_test(spi_plan_count, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           auto plan = spi.plan("select i from generate_series(1,10) i");
           auto res = spi.query<int>(plan, cppgres::spi_executor::options(2));

           result = result && _assert(res.count() == 2);

           return result;
         }));

add_test(spi_string_length, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           std::string query = "select trueGARBAGEHERE";
           auto res =
               spi.query<bool>(std::string_view(query.c_str(), query.find_first_of("GARBAGEHERE")));

           result = result && _assert(res.begin()[0]);

           return result;
         }));

add_test(spi_plan_string_length, ([](test_case &) {
           bool result = true;

           cppgres::spi_executor spi;
           std::string query = "select trueGARBAGEHERE";
           auto plan =
               spi.plan(std::string_view(query.c_str(), query.find_first_of("GARBAGEHERE")));
           auto res = spi.query<bool>(plan);

           result = result && _assert(res.begin()[0]);

           return result;
         }));

add_test(spi_result_iterator, ([](test_case &) {
           bool result = true;
           cppgres::spi_executor spi;

           auto res = spi.query<std::tuple<int64_t, std::string>>(
               "select i, 'row_' || i::text from generate_series(1,10) i");

           auto it_begin = res.begin();
           auto it_end = res.end();

           result = result && _assert(it_begin != it_end);
           result = result && _assert(std::get<0>(*it_begin) == 1);
           result = result && _assert(std::get<1>(*it_begin) == "row_1");

           auto it1 = it_begin;
           ++it1;
           result = result && _assert(std::get<0>(*it1) == 2);
           result = result && _assert(std::get<1>(*it1) == "row_2");

           auto it2 = it_begin;
           auto it2_copy = it2++;
           result = result && _assert(std::get<0>(*it2_copy) == 1);
           result = result && _assert(std::get<0>(*it2) == 2);

           for (std::size_t i = 0; i < 10; i++) {
             result = result && _assert(std::get<0>(it_begin[i]) == i + 1);
             result = result && _assert(std::get<1>(it_begin[i]) == "row_" + std::to_string(i + 1));
           }

           // Iterator arithmetic tests
           auto it3 = it_begin + 3;
           result = result && _assert(std::get<0>(*it3) == 4);

           auto it4 = it3 - 2;
           result = result && _assert(std::get<0>(*it4) == 2);

           auto it5 = it_begin;
           it5 += 5;
           result = result && _assert(std::get<0>(*it5) == 6);

           it5 -= 3;
           result = result && _assert(std::get<0>(*it5) == 3);

           auto it6 = it_begin + 7;
           auto it7 = it_begin + 3;
           result = result && _assert((it6 - it7) == 4);
           result = result && _assert((it7 - it6) == -4);

           result = result && _assert(it_begin < it1);
           result = result && _assert(it1 > it_begin);
           result = result && _assert(it_begin <= it1);
           result = result && _assert(it1 >= it_begin);
           result = result && _assert(it_begin == it_begin);
           result = result && _assert(it_begin != it1);

           // Vector results
           {
             auto res_vector =
                 spi.query<std::vector<int64_t>>("select i, i*2 from generate_series(1,3) i");
             auto it_vector = res_vector.begin();

             result = result && _assert((*it_vector)[0] == 1);
             result = result && _assert((*it_vector)[1] == 2);
             result = result && _assert((it_vector[1])[0] == 2);
             result = result && _assert((it_vector[1])[1] == 4);
           }

           // Test caching behavior
           {
             auto res_cache =
                 spi.query<std::tuple<int64_t>>("select i from generate_series(1,3) i");
             auto it_cache = res_cache.begin();

             auto &first_access = it_cache[1];
             auto &second_access = it_cache[1];
             result = result && _assert(&first_access == &second_access);
             result = result && _assert(std::get<0>(first_access) == 2);
           }

           result = result && _assert(it_begin == 0);
           result = result && _assert(it_begin != res.count());

           auto it_at_end = it_begin + res.count();
           result = result && _assert(it_at_end == res.count());
           result = result && _assert(it_at_end != 0);

           return result;
         }));

} // namespace tests
