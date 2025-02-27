#include <cassert>
#include <tuple>
#include <unordered_map>

#include <chrono>
#include <thread>

#include <cppgres.h>

extern "C" {
PG_MODULE_MAGIC;

#include <executor/spi.h>

#include <dlfcn.h>

#include <utils/acl.h>
#include <utils/date.h>
}

#include "datum.h"
#include "errors.h"
#include "function.h"
#include "memory_context.h"
#include "spi.h"
#include "tests.h"

test_case::test_case(std::string_view name, bool (*function)(test_case &c)) : function(function) {
  test_cases[name] = this;
}
bool test_case::operator()() { return function(*this); }

postgres_function(cppgres_test, ([](std::string_view name) {
                    auto test = test_case::test_cases.at(name);
                    if (test) {
                      return (*test)();
                    } else {
                      cppgres::report(ERROR, "test `%s` not found", (name).data());
                    }
                    return false;
                  }));

postgres_function(cppgres_tests, []() -> bool {
  bool result = true;
  for (auto t : test_case::test_cases) {
    auto name = t.first;
    try {
      cppgres::spi_executor spi;
      auto res = spi.query<std::tuple<bool>>("select cppgres_test($1)", name);
      bool _result = std::get<0>(res.begin()[0]);
      result = result && _result;
      cppgres::report(NOTICE, "%s: %s", name.data(), _result ? "passed" : "failed");
    } catch (std::exception &e) {
      cppgres::report(NOTICE, "%s: exception: %s", name.data(), e.what());
      result = result && false;
    }
  }
  return result;
});

static const char *find_absolute_library_path(const char *filename) {
  const char *result = filename;
#ifdef __linux__
  // Not a great solution, but not aware of anything else yet.
  // This code below reads /proc/self/maps and finds the path to the
  // library by matching the base address of omni_ext shared library.

  FILE *f = fopen("/proc/self/maps", "r");
  if (NULL == f) {
    return result;
  }

  // Get the base address of omni_ext shared library
  Dl_info info;
  dladdr(get_library_name, &info);

  // We can keep this name around forever as it'll be used to create handles
  char *path = MemoryContextAllocZero(TopMemoryContext, NAME_MAX + 1);
  char *format = psprintf("%%lx-%%*x %%*s %%*s %%*s %%*s %%%d[^\n]", NAME_MAX);

  uintptr_t base;
  while (fscanf(f, (const char *)format, &base, path) >= 1) {
    if (base == (uintptr_t)info.dli_fbase) {
      result = path;
      goto done;
    }
  }
done:
  pfree(format);
  fclose(f);
#endif
  return result;
}

static const char *get_library_name() {
  const char *library_name = NULL;
  // If we have already determined the name, return it
  if (library_name) {
    return library_name;
  }
  Dl_info info;
  ::dladdr((void *)cppgres_tests, &info);
  library_name = info.dli_fname;
  if (index(library_name, '/') == NULL) {
    // Not a full path, try to determine it. On some systems it will be a full path, on some it
    // won't.
    library_name = find_absolute_library_path(library_name);
  }
  return library_name;
}

#if defined(_MSC_VER)
#include <intrin.h>
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() raise(SIGSTOP)
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" void _PG_init(void) {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    const char *env = std::getenv("CPPGRES_DEBUG");
    if (env && std::strcmp(env, "1") == 0) {
#ifdef _WIN32
      DWORD pid = GetCurrentProcessId();
#else
      pid_t pid = getpid();
#endif
      cppgres::report(NOTICE,
                      "CPPGRES_DEBUG is set. Waiting to be connected to and resumed (pid %d)", pid);
      DEBUG_BREAK();
    }
    cppgres::spi_executor spi;
    spi.execute(
        std::format("create or replace function cppgres_tests() returns bool language 'c' as '{}'",
                    get_library_name()));
    spi.execute(std::format(
        "create or replace function cppgres_test(text) returns bool language 'c' as '{}'",
        get_library_name()));
  }
}
