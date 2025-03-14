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

} // namespace cppgres
