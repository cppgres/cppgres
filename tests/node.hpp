#pragma once

#include "tests.hpp"

namespace tests {

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

add_test(node_init, ([](test_case &) {
           bool result = true;
           cppgres::nodes::PlannedStmt pstmt;
           result = result && _assert((*pstmt).type == cppgres::nodes::PlannedStmt::tag);
           return result;
         }));

add_test(node_convert, ([](test_case &) {
           bool result = true;
           ::PlannedStmt *stmt0 = makeNode(PlannedStmt);
           cppgres::nodes::PlannedStmt pstmt(*stmt0);
           result = result && _assert((*pstmt).type == cppgres::nodes::PlannedStmt::tag);
           return result;
         }));

add_test(node_visit, ([](test_case &) {
           bool result = true;
           cppgres::nodes::PlannedStmt pstmt;
           cppgres::visit_node(
               pstmt, Overloaded{
                          [&](cppgres::nodes::PlannedStmt &stmt) { result = result && true; },
                          [&](auto &) { result = result && _assert(false); },
                      });
           ::PlannedStmt *stmt = makeNode(PlannedStmt);
           cppgres::visit_node(
               stmt, Overloaded{
                         [&](cppgres::nodes::PlannedStmt &stmt) { result = result && true; },
                         [&](auto &) { result = result && _assert(false); },
                     });
           return result;
         }));

} // namespace tests
