#pragma once

#include "datum.h"
#include "guard.h"
#include "memory.h"
#include "types.h"

#include <iterator>
#include <optional>
#include <stack>
#include <vector>

namespace cppgres {

template <typename Tuple, std::size_t... Is>
constexpr bool all_convertible_from_nullable(std::index_sequence<Is...>) {
  return ((convertible_from_nullable_datum<
              utils::remove_optional_t<utils::tuple_element_t<Is, Tuple>>>) &&
          ...);
}

template <typename T>
concept datumable_tuple = requires {
  typename utils::tuple_size<T>::type;
} && all_convertible_from_nullable<T>(std::make_index_sequence<utils::tuple_size_v<T>>{});

template <typename T>
concept convertible_into_nullable_datum_and_has_a_type =
    convertible_into_nullable_datum<T> && has_a_type<T>;

template <convertible_from_nullable_datum... Args> struct spi_plan {
  friend struct spi_executor;

  spi_plan(spi_plan &&p) : kept(p.kept), plan(p.plan), ctx(std::move(p.ctx)) { p.kept = false; }

  operator ::SPIPlanPtr() {
    if (ctx.resets() > 0) {
      throw pointer_gone_exception();
    }
    return plan;
  }

  void keep() {
    ffi_guarded(::SPI_keepplan)(*this);
    kept = true;
  }

  ~spi_plan() {
    if (kept) {
      ffi_guarded(::SPI_freeplan)(*this);
    }
  }

private:
  spi_plan(::SPIPlanPtr plan)
      : kept(false), plan(plan), ctx(tracking_memory_context(memory_context::for_pointer(plan))) {}

  bool kept;
  ::SPIPlanPtr plan;
  tracking_memory_context<memory_context> ctx;
};

struct executor {};

struct spi_executor : public executor {
  spi_executor() : before_spi(::CurrentMemoryContext) {
    ffi_guarded(::SPI_connect)();
    spi = ::CurrentMemoryContext;
    ::CurrentMemoryContext = before_spi;
    executors.push(this);
  }
  ~spi_executor() {
    ffi_guarded(::SPI_finish)();
    executors.pop();
  }

  template <datumable_tuple T> struct result_iterator {
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    ::SPITupleTable *tuptable;
    size_t index;
    mutable std::vector<std::optional<T>> tuples;

    constexpr result_iterator() noexcept {}

    constexpr result_iterator(::SPITupleTable *tuptable) noexcept
        : tuptable(tuptable), index(0),
          tuples(std::vector<std::optional<T>>(tuptable->numvals, std::nullopt)) {
      tuples.reserve(tuptable->numvals);
    }
    constexpr result_iterator(::SPITupleTable *tuptable, size_t n) noexcept
        : tuptable(tuptable), index(n),
          tuples(std::vector<std::optional<T>>(tuptable->numvals, std::nullopt)) {
      tuples.reserve(tuptable->numvals);
    }

    bool operator==(size_t end_index) const { return index == end_index; }
    bool operator!=(size_t end_index) const { return index != end_index; }

    constexpr T &operator*() const { return this->operator[](static_cast<difference_type>(index)); }

    constexpr result_iterator &operator++() noexcept {
      index++;
      return *this;
    }
    constexpr result_iterator operator++(int) noexcept {
      index++;
      return this;
    }

    constexpr result_iterator &operator--() noexcept {
      index++;
      return this;
    }
    constexpr result_iterator operator--(int) noexcept {
      index--;
      return this;
    }

    constexpr result_iterator operator+(const difference_type n) const noexcept {
      return result_iterator(tuptable, index + n);
    }

    result_iterator &operator+=(difference_type n) noexcept {
      index += n;
      return this;
    }

    constexpr result_iterator operator-(difference_type n) const noexcept {
      return result_iterator(tuptable, index - n);
    }

    result_iterator &operator-=(difference_type n) noexcept {
      index -= n;
      return this;
    }

    constexpr difference_type operator-(const result_iterator &other) const noexcept {
      return index - other.index;
    }

    T &operator[](difference_type n) const {
      if (tuples.at(n).has_value()) {
        return tuples.at(n).value();
      }
      auto ret = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return T{([&] {
          bool isnull;
          ::Datum value =
              ffi_guarded(::SPI_getbinval)(tuptable->vals[n], tuptable->tupdesc, Is + 1, &isnull);
          ::NullableDatum datum = {.value = value, .isnull = isnull};
          auto nd = nullable_datum(datum);
          return from_nullable_datum<utils::tuple_element_t<Is, T>>(nd);
        }())...};
      }(std::make_index_sequence<utils::tuple_size_v<T>>{});
      tuples.emplace(std::next(tuples.begin(), n), std::in_place, ret);
      return tuples.at(n).value();
    }

    constexpr bool operator==(const result_iterator &other) const noexcept {
      return tuptable == other.tuptable && index == other.index;
    }
    constexpr bool operator!=(const result_iterator &other) const noexcept {
      return !(tuptable == other.tuptable && index == other.index);
    }
    constexpr bool operator<(const result_iterator &other) const noexcept {
      return index < other.index;
    }
    constexpr bool operator>(const result_iterator &other) const noexcept {
      return index > other.index;
    }
    constexpr bool operator<=(const result_iterator &other) const noexcept {
      return index <= other.index;
    }
    constexpr bool operator>=(const result_iterator &other) const noexcept {
      return index >= other.index;
    }

  private:
  };

  template <datumable_tuple Ret, convertible_into_nullable_datum... Args> struct results {
    ::SPITupleTable *table;

    results(::SPITupleTable *table) : table(table) {
      auto natts = table->tupdesc->natts;
      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (([&] {
           auto oid = ffi_guarded(::SPI_gettypeid)(table->tupdesc, Is + 1);
           auto t = type{.oid = oid};
           if (!type_traits<utils::tuple_element_t<Is, Ret>>::is(t)) {
             throw std::invalid_argument(
                 std::format("invalid return type in position {} ({}), got OID {}", Is,
                             utils::type_name<utils::tuple_element_t<Is, Ret>>(), oid));
           }
         }()),
         ...);
      }(std::make_index_sequence<sizeof...(Args)>{});
      if (natts != utils::tuple_size_v<Ret>) {
        throw std::runtime_error(
            std::format("expected %d return values, got %d", utils::tuple_size_v<Ret>, natts));
      }
    }

    result_iterator<Ret> begin() const { return result_iterator<Ret>(table); }
    size_t end() const { return table->numvals; }
  };

  template <datumable_tuple Ret, convertible_into_nullable_datum_and_has_a_type... Args>
  results<Ret, Args...> query(std::string_view query, Args &&...args) {
    if (executors.top() != this) {
      throw std::runtime_error("not a current SPI executor");
    }
    constexpr size_t nargs = sizeof...(Args);
    std::array<::Oid, nargs> types = {type_for<Args>().oid...};
    std::array<::Datum, nargs> datums = {into_nullable_datum(args)...};
    std::array<const char, nargs> nulls = {into_nullable_datum(args).is_null() ? 'n' : ' ' ...};
    auto rc = ffi_guarded(::SPI_execute_with_args)(query.data(), nargs, types.data(), datums.data(),
                                                   nulls.data(), false, 0);
    if (rc == SPI_OK_SELECT) {
      //      static_assert(std::random_access_iterator<result_iterator<Ret>>);
      return results<Ret, Args...>(SPI_tuptable);
    } else {
      throw std::runtime_error("spi error");
    }
  }

  template <convertible_into_nullable_datum_and_has_a_type... Args>
  spi_plan<Args...> plan(std::string_view query) {
    if (executors.top() != this) {
      throw std::runtime_error("not a current SPI executor");
    }
    constexpr size_t nargs = sizeof...(Args);
    std::array<::Oid, nargs> types = {type_for<Args>().oid...};
    return spi_plan<Args...>(ffi_guarded(::SPI_prepare)(query.data(), nargs, types.data()));
  }

  template <datumable_tuple Ret, convertible_into_nullable_datum... Args>
  results<Ret, Args...> query(spi_plan<Args...> &query, Args &&...args) {
    if (executors.top() != this) {
      throw std::runtime_error("not a current SPI executor");
    }
    constexpr size_t nargs = sizeof...(Args);
    std::array<::Datum, nargs> datums = {into_nullable_datum(args)...};
    std::array<const char, nargs> nulls = {into_nullable_datum(args).is_null() ? 'n' : ' ' ...};
    auto rc = ffi_guarded(::SPI_execute_plan)(query, datums.data(), nulls.data(), false, 0);
    if (rc == SPI_OK_SELECT) {
      //      static_assert(std::random_access_iterator<result_iterator<Ret>>);
      return results<Ret, Args...>(SPI_tuptable);
    } else {
      throw std::runtime_error("spi error");
    }
  }

  template <convertible_into_nullable_datum_and_has_a_type... Args>
  uint64_t execute(std::string_view query, Args &&...args) {
    if (executors.top() != this) {
      throw std::runtime_error("not a current SPI executor");
    }
    constexpr size_t nargs = sizeof...(Args);
    std::array<::Oid, nargs> types = {type_for<Args>().oid...};
    std::array<::Datum, nargs> datums = {into_nullable_datum(args)...};
    std::array<const char, nargs> nulls = {into_nullable_datum(args).is_null() ? 'n' : ' ' ...};
    auto rc = ffi_guarded(::SPI_execute_with_args)(query.data(), nargs, types.data(), datums.data(),
                                                   nulls.data(), false, 0);
    if (rc >= 0) {
      return SPI_processed;
    } else {
      throw std::runtime_error("spi error");
    }
  }

private:
  ::MemoryContext before_spi;
  ::MemoryContext spi;
  static inline std::stack<spi_executor *> executors;
};

} // namespace cppgres
