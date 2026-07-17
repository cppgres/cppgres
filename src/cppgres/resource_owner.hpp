/**
 * \file
 */
#pragma once

#include "guard.hpp"
#include "imports.h"

extern "C" {
#include <executor/executor.h>
#include <utils/plancache.h>
#include <utils/resowner.h>
}

namespace cppgres {

/**
 * @brief RAII-managed resource owner
 *
 * Creates a resource owner (`ResourceOwnerCreate`) on construction and, if
 * still owning at destruction, releases the plan cache references it holds
 * and deletes it. Ownership can be relinquished with release().
 */
struct resource_owner {
  explicit resource_owner(const char *name, ::ResourceOwner parent = nullptr)
      : owner(ffi_guard{::ResourceOwnerCreate}(parent, name)) {}

  resource_owner(const resource_owner &) = delete;
  resource_owner &operator=(const resource_owner &) = delete;

  resource_owner(resource_owner &&other) noexcept : owner(other.owner) { other.owner = nullptr; }

  resource_owner &operator=(resource_owner &&other) noexcept {
    if (this != &other) {
      dispose();
      owner = other.owner;
      other.owner = nullptr;
    }
    return *this;
  }

  operator ::ResourceOwner() const { return owner; }

  /**
   * @brief Disown the resource owner and return it
   */
  ::ResourceOwner release() noexcept {
    ::ResourceOwner o = owner;
    owner = nullptr;
    return o;
  }

  ~resource_owner() noexcept { dispose(); }

private:
  void dispose() noexcept {
    if (owner == nullptr) {
      return;
    }
    try {
      // Releasing plan cache references when there are none is harmless,
      // so always do both.  (Renamed in Postgres 17.)
#if PG_MAJORVERSION_NUM >= 17
      ffi_guard{::ReleaseAllPlanCacheRefsInOwner}(owner);
#else
      ffi_guard{::ResourceOwnerReleaseAllPlanCacheRefs}(owner);
#endif
      ffi_guard{::ResourceOwnerDelete}(owner);
    } catch (...) {
      elog(WARNING, "cppgres: deleting resource owner failed");
    }
    owner = nullptr;
  }

  ::ResourceOwner owner;
};

/**
 * @brief RAII-managed executor state (`EState`)
 *
 * Creates an executor state (`CreateExecutorState`) on construction and, if
 * still owning at destruction, frees it. Ownership can be relinquished with
 * release().
 */
struct executor_state {
  executor_state() : estate(ffi_guard{::CreateExecutorState}()) {}

  executor_state(const executor_state &) = delete;
  executor_state &operator=(const executor_state &) = delete;

  executor_state(executor_state &&other) noexcept : estate(other.estate) {
    other.estate = nullptr;
  }

  executor_state &operator=(executor_state &&other) noexcept {
    if (this != &other) {
      dispose();
      estate = other.estate;
      other.estate = nullptr;
    }
    return *this;
  }

  operator ::EState *() const { return estate; }

  /**
   * @brief Disown the executor state and return it
   */
  ::EState *release() noexcept {
    ::EState *e = estate;
    estate = nullptr;
    return e;
  }

  ~executor_state() noexcept { dispose(); }

private:
  void dispose() noexcept {
    if (estate == nullptr) {
      return;
    }
    try {
      ffi_guard{::FreeExecutorState}(estate);
    } catch (...) {
      elog(WARNING, "cppgres: freeing executor state failed");
    }
    estate = nullptr;
  }

  ::EState *estate;
};

} // namespace cppgres
