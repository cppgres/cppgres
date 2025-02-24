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

static const char *get_library_name();

#define _assert(expr)                                                                              \
  ({                                                                                               \
    auto value = (expr);                                                                           \
    if (!value) {                                                                                  \
      cppgres::report(NOTICE, "assertion failure %s:%d (`%s`): %s", __FILE_NAME__, __LINE__,       \
                      __func__, #expr);                                                            \
    }                                                                                              \
    value;                                                                                         \
  })

struct test_case {
  inline static std::unordered_map<std::string_view, test_case *> test_cases =
      std::unordered_map<std::string_view, test_case *>{};
  test_case(std::string_view name, bool (*function)(test_case &c)) : function(function) {
    test_cases[name] = this;
  }
  bool operator()() { return function(*this); }
  bool (*function)(test_case &c);
};

#define add_test(test) static test_case t__##test(std::string_view(#test), test);

namespace tests {

static bool nullable_datum_enforcement(test_case &) {
  cppgres::nullable_datum d;
  assert(d.is_null());
  try {
    cppgres::ffi_guarded(::DatumGetDateADT)(d);
    _assert(false);
  } catch (cppgres::null_datum_exception &e) {
    return true;
  }
  // try the same unguarded, it won't (shouldn't) call it anyway
  try {
    ::DatumGetDateADT(d);
    _assert(false);
  } catch (cppgres::null_datum_exception &e) {
    return true;
  }
  return false;
}
add_test(nullable_datum_enforcement);

static bool catch_error(test_case &) {
  try {
    cppgres::ffi_guarded(::get_role_oid)("this_role_does_not_exist", false);
    _assert(false);
  } catch (cppgres::pg_exception &e) {
    return true;
  }
  return false;
}
add_test(catch_error);

postgres_function(raise_exception,
                  []() -> std::optional<bool> { throw std::runtime_error("raised an exception"); });

static bool exception_to_error(test_case &) {
  bool result = false;
  cppgres::ffi_guarded(::SPI_connect)();
  auto stmt =
      std::format("create or replace function raise_exception() returns bool language 'c' as '{}'",
                  get_library_name());
  cppgres::ffi_guarded(::SPI_execute)(stmt.c_str(), false, 0);
  cppgres::ffi_guarded(::BeginInternalSubTransaction)(nullptr);
  try {
    cppgres::ffi_guarded(::SPI_execute)("select raise_exception()", false, 0);
  } catch (cppgres::pg_exception &e) {
    result = _assert(std::string_view(e.message()) == "exception: raised an exception");
    cppgres::ffi_guarded(::RollbackAndReleaseCurrentSubTransaction)();
  }
  cppgres::ffi_guarded(::SPI_finish)();
  return result;
}
add_test(exception_to_error);

static bool alloc_set_context(test_case &) {
  bool result = true;
  cppgres::alloc_set_memory_context c;
  result = result && _assert(c.alloc(100));
  return result;
}
add_test(alloc_set_context);

static bool allocator(test_case &) {
  bool result = true;

  cppgres::memory_context_allocator<std::string> alloc0;
  result = result && _assert(alloc0.memory_context() == cppgres::memory_context());

  auto old_ctx = ::CurrentMemoryContext;
  auto ctx = cppgres::alloc_set_memory_context();
  ::CurrentMemoryContext = ctx;
  result = result && _assert(alloc0.memory_context() != cppgres::memory_context());
  ::CurrentMemoryContext = old_ctx;

  cppgres::memory_context_allocator<std::string> alloc(std::move(cppgres::top_memory_context),
                                                       true);
  std::vector<std::string, cppgres::memory_context_allocator<std::string>> vec(alloc);
  vec.emplace_back("a");

  return result;
}
add_test(allocator);

static bool current_memory_context(test_case &) {
  bool result = true;

  cppgres::always_current_memory_context ctx;
  cppgres::memory_context ctx0(ctx);
  result = result && _assert(ctx == ctx0);

  ::CurrentMemoryContext = ::TopMemoryContext;

  result = result && _assert(ctx != ctx0);

  return result;
}
add_test(current_memory_context);

static bool memory_context_for_ptr(test_case &) {
  return _assert(cppgres::memory_context::for_pointer(::palloc0(100)) == cppgres::memory_context());
}
add_test(memory_context_for_ptr);

bool spi(test_case &) {
  bool result = true;
  cppgres::spi_executor spi;
  auto res = spi.query<std::tuple<std::optional<int64_t>>>(
      "select $1 + i from generate_series(1,100) i", 1LL);

  int i = 0;
  for (auto &re : res) {
    i++;
    result = result && _assert(std::get<0>(re) == i + 1);
  }
  result = result && _assert(std::get<0>(res.begin()[0]) == 2);
  return result;
}
add_test(spi);

static bool varlena_text(test_case &) {
  bool result = true;
  auto nd = cppgres::nullable_datum(::PointerGetDatum(::cstring_to_text("test")));
  auto s = cppgres::from_nullable_datum<cppgres::text>(nd);
  std::string_view str = *s;
  result = result && _assert(str == "test");

  // Try memory context being gone
  {
    auto ctx = cppgres::alloc_set_memory_context();
    auto p = ::CurrentMemoryContext;
    _assert(p != ctx);
    ::CurrentMemoryContext = ctx;
    auto nd1 = cppgres::nullable_datum(::PointerGetDatum(::cstring_to_text("test1")));
    auto s1 = cppgres::from_nullable_datum<cppgres::text>(nd1);
    _assert(s1->memory_context() == ctx);
    ::CurrentMemoryContext = p;
    ctx.reset();

    bool exception_raised = false;
    try {
      std::string_view str1 = *s1;
    } catch (cppgres::pointer_gone_exception &e) {
      exception_raised = true;
    }
    result = result && _assert(exception_raised);
  }

  return result;
}
add_test(varlena_text);

} // namespace tests

postgres_function(cppgres_tests, []() -> std::optional<bool> {
  bool result = true;
  for (auto t : test_case::test_cases) {
    auto name = t.first;
    test_case *test = t.second;
    auto _result = (*test)();
    result = result && _result;
    cppgres::report(NOTICE, "%s: %s", name.data(), _result ? "passed" : "failed");
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
    cppgres::ffi_guarded(::SPI_connect)();
    auto stmt =
        std::format("create or replace function cppgres_tests() returns bool language 'c' as '{}'",
                    get_library_name());
    cppgres::ffi_guarded(::SPI_execute)(stmt.c_str(), false, 0);
    cppgres::ffi_guarded(::SPI_finish)();
  }
}
