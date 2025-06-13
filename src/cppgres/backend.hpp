#pragma once

#include "imports.h"
#include "memory.hpp"

#include <any>
#include <concepts>
#include <future>
#include <map>
#include <ranges>

namespace cppgres {

namespace backend_type {

/**
 * @brief Backend type
 *
 * A copy of Postgres' BackendType with minor renaming
 */
enum type {
  invalid = B_INVALID,
  backend = B_BACKEND,
  autovac_launcher = B_AUTOVAC_LAUNCHER,
  autovac_worker = B_AUTOVAC_WORKER,
  bg_worker = B_BG_WORKER,
  wal_sender = B_WAL_SENDER,
#if PG_MAJORVERSION_NUM >= 17
  slotsync_worker = B_SLOTSYNC_WORKER,
  standalone_backend = B_STANDALONE_BACKEND,
#endif
  archiver = B_ARCHIVER,
  bg_writer = B_BG_WRITER,
  checkpointer = B_CHECKPOINTER,
  startup = B_STARTUP,
  wal_receiver = B_WAL_RECEIVER,
#if PG_MAJORVERSION_NUM >= 17
  wal_summarizer = B_WAL_SUMMARIZER,
#endif
  wal_writer = B_WAL_WRITER,
  logger = B_LOGGER
};
} // namespace backend_type

/**
 * @brief Backend management
 */
struct backend {
  /**
   * @brief get current backend type
   * @return backend type
   */
  static backend_type::type type() { return static_cast<backend_type::type>(::MyBackendType); };

  /**
   * @brief Register a callback for when Postgres will be exiting
   *
   * This allows passing a lambda with a closure (not just a plain C function / lambda without a
   * closure), it'll initialize the closure in the top memory context.
   */
  template <typename T> requires requires(T t, int code) {
    { t(code) };
  }
  static void atexit(T &&func) {
    T *raw_mem = top_memory_context().alloc<T>();
    T *allocation = new (raw_mem) T(std::forward<T>(func));

    ffi_guard{::on_proc_exit}(
        [](int code, ::Datum datum) {
          T *func = reinterpret_cast<T *>(DatumGetPointer(datum));
          (*func)(code);
        },
        PointerGetDatum(allocation));
  }
};

/**
 * @brief Backend name registry
 *
 * Backend-local registration of variables that can be accessed by extensions. It builds on the
 * concept of "rendezvous variables" in Postgres, bringing the following:
 *
 * * out-of-order initialization
 * * uninitialized value avoidance
 *
 */
struct backend_name_registry {

private:
  struct inner_t {
    std::map<std::string, std::any> map;
    std::map<std::string, std::vector<std::function<void(void *)>>> cbs;
  };

public:
  backend_name_registry(const char *name = "cppgres::backend_name_registry") {
    void **ptr = cppgres::ffi_guard{::find_rendezvous_variable}(name);
    if (*ptr == nullptr) {
      std::allocator<inner_t> alloc;
      inner = alloc.allocate(1);
      std::construct_at(inner);
      // Only set the pointer when `inner` is constructed
      *ptr = inner;
    } else {
      inner = reinterpret_cast<inner_t *>(*ptr);
    }
  }

  template <typename T> void on_initialized(const char *name, std::function<void(T &)> &&func) {
    auto val = inner->map.find(name);
    if (val == inner->map.end()) {
      auto &cbs = inner->cbs[name];
      cbs.push_back([func = std::move(func)](void *t) { func(*static_cast<T *>(t)); });
    } else {
      func(reinterpret_cast<T &>(std::any_cast<T &>(val->second)));
    }
  }

  template <typename T> std::optional<std::reference_wrapper<T>> find(const char *name) const {
    auto val = inner->map.find(name);
    if (val == inner->map.end()) {
      return std::nullopt;
    }
    return std::any_cast<T &>(val->second);
  }

  template <typename T> requires(std::default_initializable<T> && noexcept(T{}))
  T &find_or_create(const char *name) {
    auto val = inner->map.find(name);
    if (val == inner->map.end()) {
      T t{};
      for (auto &cb : inner->cbs[name]) {
        cb(&t);
      }
      inner->cbs[name].clear();
      val = inner->map.insert({name, t}).first;
    }
    return std::any_cast<T &>(val->second);
  }

  void clear() {
    inner->map.clear();
    inner->cbs.clear();
  }

private:
  inner_t *inner;
};

} // namespace cppgres
