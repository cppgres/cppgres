#pragma once

#include <string>
#include <tuple>

#include <iostream>

#include "exception.h"

namespace cppgres {

void error(pg_exception e) {
  ::errstart(ERROR, TEXTDOMAIN);
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
  ::errmsg("%s", e.message());
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
  ::errfinish(__FILE__, __LINE__, __func__);
  __builtin_unreachable();
}

template <std::size_t N, typename... Args>
void report(int elevel, const char (&fmt)[N], Args... args) {
  ::errstart(elevel, TEXTDOMAIN);
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
  ::errmsg(fmt, args...);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
  ::errfinish(__FILE__, __LINE__, __func__);
  if (elevel >= ERROR) {
    __builtin_unreachable();
  }
}
} // namespace cppgres
