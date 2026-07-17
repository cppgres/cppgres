/**
 * \file
 */
#pragma once

#include "imports.h"

#include <iterator>
#include <type_traits>

namespace cppgres {

/**
 * @brief Typed, range-for-iterable view over a PostgreSQL ::List.
 *
 * The element type selects the cell accessor: pointer types read `lfirst`,
 * `int` reads `lfirst_int`, ::Oid reads `lfirst_oid`.
 *
 * A NIL (null) list is an empty range.
 *
 * ```
 * for (auto *tle : cppgres::list<TargetEntry *>(query->targetList)) { ... }
 * ```
 */
template <typename T = void *> struct list {
  explicit list(::List *l) : list_(l) {}

  struct iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    ::ListCell *cell = nullptr;

    T operator*() const {
      if constexpr (std::is_pointer_v<T>) {
        return static_cast<T>(lfirst(cell));
      } else if constexpr (std::is_same_v<T, ::Oid>) {
        return lfirst_oid(cell);
      } else {
        static_assert(std::is_same_v<T, int>, "unsupported list element type");
        return lfirst_int(cell);
      }
    }

    iterator &operator++() {
      ++cell;
      return *this;
    }
    iterator operator++(int) {
      auto ret = *this;
      ++cell;
      return ret;
    }
    bool operator==(const iterator &) const = default;
  };

  iterator begin() const { return {list_ == NIL ? nullptr : &list_->elements[0]}; }
  iterator end() const { return {list_ == NIL ? nullptr : &list_->elements[list_->length]}; }

  size_t size() const { return list_ == NIL ? 0 : static_cast<size_t>(list_->length); }
  bool empty() const { return list_ == NIL; }

  operator ::List *() const { return list_; }

private:
  ::List *list_;
};

} // namespace cppgres
