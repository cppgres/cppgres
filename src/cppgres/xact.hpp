/**
 * \file
 */
#pragma once

#include <stack>

extern "C" {
#include <access/xact.h>
}

namespace cppgres {

struct internal_subtransaction {
  internal_subtransaction(bool commit = true)
      : owner(::CurrentResourceOwner), commit(commit), name("") {
    if (txns.empty()) {
      ffi_guard{::BeginInternalSubTransaction}(nullptr);
      txns.push(this);
    } else {
      throw std::runtime_error("internal subtransaction already started");
    }
  }

  internal_subtransaction(std::string_view name, bool commit = true)
      : owner(::CurrentResourceOwner), commit(commit), name(name) {
    if (txns.empty()) {
      ffi_guard{::BeginInternalSubTransaction}(this->name.c_str());
      txns.push(this);
    } else {
      throw std::runtime_error("internal subtransaction already started");
    }
  }

  ~internal_subtransaction() {
    txns.pop();
    if (commit) {
      ffi_guard{::ReleaseCurrentSubTransaction}();
    } else {
      ffi_guard{::RollbackAndReleaseCurrentSubTransaction}();
    }
    ::CurrentResourceOwner = owner;
  }

private:
  ::ResourceOwner owner;
  bool commit;
  std::string name;
  static inline std::stack<internal_subtransaction *> txns;
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
