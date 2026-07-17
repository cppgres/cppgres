/**
 * \file
 */
#pragma once

#include <exception>
#include <string>
#include <string_view>

extern "C" {
#include <access/xact.h>
}

#include "guard.hpp"

namespace cppgres {

using command_id = ::CommandId;

struct transaction_id {

  transaction_id() : id_(InvalidTransactionId) {}
  transaction_id(::TransactionId id) : id_(id) {}
  transaction_id(const transaction_id &id) : id_(id.id_) {}

  static transaction_id current(bool acquire = true) {
    return transaction_id(
        ffi_guard{acquire ? ::GetCurrentTransactionId : ::GetCurrentTransactionIdIfAny}());
  }

  bool is_valid() const { return TransactionIdIsValid(id_); }

  bool operator==(const transaction_id &other) const { return TransactionIdEquals(id_, other.id_); }
  bool operator>(const transaction_id &other) const { return TransactionIdFollows(id_, other.id_); }
  bool operator>=(const transaction_id &other) const {
    return TransactionIdFollowsOrEquals(id_, other.id_);
  }
  bool operator<(const transaction_id &other) const {
    return TransactionIdPrecedes(id_, other.id_);
  }
  bool operator<=(const transaction_id &other) const {
    return TransactionIdPrecedesOrEquals(id_, other.id_);
  }

  bool did_abort() const { return is_valid() && TransactionIdDidAbort(id_); }
  bool did_commit() const { return is_valid() && TransactionIdDidCommit(id_); }

private:
  ::TransactionId id_;
};

static_assert(sizeof(transaction_id) == sizeof(::TransactionId));

/**
 * @brief Internal subtransaction guard
 *
 * Begins an internal subtransaction on construction and finishes it exactly
 * once. Subtransactions nest (as PL exception blocks do).
 *
 * If the scope is left because an exception is propagating, the
 * subtransaction is rolled back; otherwise, at scope exit it is committed
 * (default) or rolled back per the flag given at construction. Alternatively,
 * it can be finished early by calling commit() or rollback() explicitly.
 *
 * `CurrentMemoryContext` and `CurrentResourceOwner` observed at construction
 * are restored on every path.
 */
struct internal_subtransaction {
  internal_subtransaction(bool commit = true)
      : ctx(::CurrentMemoryContext), owner(::CurrentResourceOwner), should_commit(commit),
        uncaught(std::uncaught_exceptions()), finished(false), name("") {
    ffi_guard{::BeginInternalSubTransaction}(nullptr);
    ::CurrentMemoryContext = ctx;
  }

  internal_subtransaction(std::string_view name, bool commit = true)
      : ctx(::CurrentMemoryContext), owner(::CurrentResourceOwner), should_commit(commit),
        uncaught(std::uncaught_exceptions()), finished(false), name(name) {
    ffi_guard{::BeginInternalSubTransaction}(this->name.c_str());
    ::CurrentMemoryContext = ctx;
  }

  internal_subtransaction(const internal_subtransaction &) = delete;
  internal_subtransaction &operator=(const internal_subtransaction &) = delete;
  internal_subtransaction(internal_subtransaction &&) = delete;
  internal_subtransaction &operator=(internal_subtransaction &&) = delete;

  /**
   * @brief Commit the subtransaction now
   */
  void commit() {
    ffi_guard{::ReleaseCurrentSubTransaction}();
    restore();
    finished = true;
  }

  /**
   * @brief Roll the subtransaction back now
   */
  void rollback() {
    ffi_guard{::RollbackAndReleaseCurrentSubTransaction}();
    restore();
    finished = true;
  }

  ~internal_subtransaction() noexcept {
    if (finished) {
      return;
    }
    ffi_guard_noexcept(
        [this] {
          if (std::uncaught_exceptions() > uncaught || !should_commit) {
            ::RollbackAndReleaseCurrentSubTransaction();
          } else {
            ::ReleaseCurrentSubTransaction();
          }
        },
        "cppgres: finishing internal subtransaction failed");
    restore();
  }

private:
  void restore() noexcept {
    ::CurrentMemoryContext = ctx;
    ::CurrentResourceOwner = owner;
  }

  ::MemoryContext ctx;
  ::ResourceOwner owner;
  bool should_commit;
  int uncaught;
  bool finished;
  std::string name;
};

struct transaction {
  transaction(bool commit = true) : should_commit(commit), released(false) {
    ffi_guard([]() {
      if (!::IsTransactionState()) {
        ::SetCurrentStatementStartTimestamp();
        ::StartTransactionCommand();
        ::PushActiveSnapshot(::GetTransactionSnapshot());
      }
    })();
  }

  ~transaction() {
    if (!released) {
      ffi_guard([this]() {
        ::PopActiveSnapshot();
        if (should_commit) {
          ::CommitTransactionCommand();
        } else {
          ::AbortCurrentTransaction();
        }
      })();
    }
  }

  void commit() {
    ffi_guard([]() {
      ::PopActiveSnapshot();
      ::CommitTransactionCommand();
    })();
    released = true;
  }

  void rollback() {
    ffi_guard([]() {
      ::PopActiveSnapshot();
      ::AbortCurrentTransaction();
    })();
    released = true;
  }

private:
  bool should_commit;
  bool released;
};

} // namespace cppgres
