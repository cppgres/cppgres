#include <cppgres.h>

extern "C" {
  PG_MODULE_MAGIC;
}

postgres_function(demo_len, ([](std::string_view t) { return t.length(); }));
