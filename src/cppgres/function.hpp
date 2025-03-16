/**
 * \file
 */
#pragma once

#include "datum.hpp"
#include "guard.hpp"
#include "imports.h"
#include "record.hpp"
#include "set.hpp"
#include "types.hpp"
#include "utils/function_traits.hpp"
#include "utils/utils.hpp"

#include <array>
#include <complex>
#include <iostream>
#include <stack>
#include <tuple>
#include <typeinfo>

namespace cppgres {

template <typename T>
concept convertible_into_nullable_datum_or_set_iterator_or_void =
    convertible_into_nullable_datum<T> || datumable_iterator<T> || std::same_as<T, void>;

/**
 * @brief Function that operates on values of Postgres types
 *
 * These functions take @ref cppgres::convertible_from_nullable_datum arguments and returns
 * @ref cppgres::convertible_into_nullable_datum, or @ref cppgres::datumable_iterator
 * (for [Set-Returning
 * Functions](https://www.postgresql.org/docs/current/xfunc-c.html#XFUNC-C-RETURN-SET)).
 */
template <typename Func>
concept datumable_function =
    requires { typename utils::function_traits::function_traits<Func>::argument_types; } &&
    all_from_nullable_datum<
        typename utils::function_traits::function_traits<Func>::argument_types>::value &&
    requires(Func f) {
      {
        std::apply(
            f,
            std::declval<typename utils::function_traits::function_traits<Func>::argument_types>())
      } -> convertible_into_nullable_datum_or_set_iterator_or_void;
    };

struct current_postgres_function {

  static std::optional<bool> atomic() {
    if (!calls.empty()) {
      auto ctx = calls.top()->context;
      if (ctx != nullptr && IsA(ctx, CallContext)) {
        return reinterpret_cast<::CallContext *>(ctx)->atomic;
      }
    }

    return std::nullopt;
  }

  template <datumable_function Func> friend struct postgres_function;

private:
  struct handle {
    ~handle() { calls.pop(); }

    friend struct current_postgres_function;

  private:
    handle() {}
  };

  static handle push(::FunctionCallInfo fcinfo) {
    calls.push(fcinfo);
    return handle{};
  }
  static void pop() { calls.pop(); }

  static inline std::stack<::FunctionCallInfo> calls;
};

/**
 * @brief Postgres function implemented in C++
 *
 * It wraps a function to handle conversion of arguments and return, enforce arity, convert C++
 * exceptions to errors. Additionally, it handles [Set-Returning
 * Functions](https://www.postgresql.org/docs/current/xfunc-c.html#XFUNC-C-RETURN-SET) (SRFs),
 * implemented by functions returning @ref cppgres::datumable_iterator.
 *
 * @tparam Func function (or lambda) that conforms to the @ref cppgres::datumable_function concept.
 */
template <datumable_function Func> struct postgres_function {
  Func func;

  explicit postgres_function(Func f) : func(f) {}

  // Use the function_traits to extract argument types.
  using traits = utils::function_traits::function_traits<Func>;
  using argument_types = typename traits::argument_types;
  using return_type = utils::function_traits::invoke_result_from_tuple_t<Func, argument_types>;
  static constexpr std::size_t arity = traits::arity;

  /**
   * Invoke the function as per Postgres convention
   */
  auto operator()(FunctionCallInfo fc) -> ::Datum {

    return exception_guard([&] {
      // return type
      auto rettype = type{.oid = ffi_guard{::get_fn_expr_rettype}(fc->flinfo)};
      auto retset = fc->flinfo->fn_retset;

      if constexpr (datumable_iterator<return_type>) {
        // Check this before checking the type of `retset`
        auto rsinfo = reinterpret_cast<::ReturnSetInfo *>(fc->resultinfo);
        if (rsinfo == nullptr) {
          report(ERROR, "caller is not expecting a set");
        }
      }

      if (!OidIsValid(rettype.oid)) {
        // TODO: not very efficient to look it up every time
        syscache<Form_pg_proc, Oid> cache(fc->flinfo->fn_oid);
        rettype = type{.oid = (*cache).prorettype};
        retset = (*cache).proretset;
      }

      if (retset) {
        if constexpr (datumable_iterator<return_type>) {
          using set_value_type = set_iterator_traits<return_type>::value_type;
          if (!type_traits<set_value_type>::is(rettype)) {
            report(ERROR, "unexpected set's return type, can't convert `%s` into `%.*s`",
                   rettype.name().data(), utils::type_name<set_value_type>().length(),
                   utils::type_name<set_value_type>().data());
          }
        } else {
          report(ERROR,
                 "unexpected return type, set is expected, but `%.*s` does not conform to "
                 "`cppgres::datumable_iterator`",
                 utils::type_name<return_type>().length(), utils::type_name<return_type>().data());
        }
      } else if (!type_traits<return_type>::is(rettype)) {
        report(ERROR, "unexpected return type, can't convert `%s` into `%.*s`",
               rettype.name().data(), utils::type_name<return_type>().length(),
               utils::type_name<return_type>().data());
      }

      // arguments
      short accounted_for_args = 0;
      auto t = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return argument_types{([&]() -> utils::tuple_element_t<Is, argument_types> {
          using ptyp = utils::tuple_element_t<Is, argument_types>;
          auto typ = type{.oid = ffi_guard{::get_fn_expr_argtype}(fc->flinfo, Is)};
          if (!OidIsValid(typ.oid)) {
            // TODO: not very efficient to look it up every time
            syscache<Form_pg_proc, Oid> cache(fc->flinfo->fn_oid);
            if ((*cache).proargtypes.dim1 > Is) {
              typ = type{.oid = (*cache).proargtypes.values[Is]};
            }
          }
          if (!type_traits<ptyp>::is(typ)) {
            report(ERROR, "unexpected type in position %d, can't convert `%s` into `%.*s`", Is,
                   typ.name().data(), utils::type_name<ptyp>().length(),
                   utils::type_name<ptyp>().data());
          }
          accounted_for_args++;
          return from_nullable_datum<ptyp>(nullable_datum(fc->args[Is]));
        }())...};
      }(std::make_index_sequence<utils::tuple_size_v<argument_types>>{});

      if (arity != accounted_for_args) {
        report(ERROR, "expected %d arguments, got %d instead", arity, accounted_for_args);
      }

      auto call_handle = current_postgres_function::push(fc);

      if constexpr (datumable_iterator<return_type>) {
        auto rsinfo = reinterpret_cast<::ReturnSetInfo *>(fc->resultinfo);
        // TODO: For now, let's assume materialized model
        using set_value_type = set_iterator_traits<return_type>::value_type;
        if constexpr (std::same_as<set_value_type, record>) {

          auto natts = rsinfo->expectedDesc == nullptr ? -1 : rsinfo->expectedDesc->natts;

          rsinfo->returnMode = SFRM_Materialize;

          memory_context_scope scope(memory_context(rsinfo->econtext->ecxt_per_query_memory));

          ::Tuplestorestate *tupstore = ffi_guard{::tuplestore_begin_heap}(
              (rsinfo->allowedModes & SFRM_Materialize_Random) == SFRM_Materialize_Random, false,
              work_mem);
          rsinfo->setResult = tupstore;

          auto res = std::apply(func, t);

          bool checked = false;
          for (auto r : res) {
            auto nargs = r.attributes();
            if (!checked) {
              if (rsinfo->expectedDesc != nullptr && nargs != natts) {
                throw std::runtime_error(
                    std::format("expected record with {} value{}, got {} instead", nargs,
                                nargs == 1 ? "" : "s", natts));
              }
              if (rsinfo->expectedDesc != nullptr &&
                  !r.get_tuple_descriptor().equal_types(
                      cppgres::tuple_descriptor(rsinfo->expectedDesc))) {
                throw std::runtime_error("expected and returned records do not match");
              }
              checked = true;
            }

            ffi_guard{::tuplestore_puttuple}(tupstore, r);
          }
          fc->isnull = true;
          return ::Datum(0);
        } else {
          constexpr auto nargs = utils::tuple_size_v<set_value_type>;

          auto natts = rsinfo->expectedDesc->natts;

          if (nargs != natts) {
            throw std::runtime_error(std::format("expected set with {} value{}, got {} instead",
                                                 nargs, nargs == 1 ? "" : "s", natts));
          }

          [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (([&] {
               auto oid = ffi_guard{::SPI_gettypeid}(rsinfo->expectedDesc, Is + 1);
               auto t = type{.oid = oid};
               using typ = utils::tuple_element_t<Is, set_value_type>;
               if (!type_traits<typ>::is(t)) {
                 throw std::invalid_argument(
                     std::format("invalid type in record's position {} ({}), got OID {}", Is,
                                 utils::type_name<typ>(), oid));
               }
             }()),
             ...);
          }(std::make_index_sequence<nargs>{});

          rsinfo->returnMode = SFRM_Materialize;

          memory_context_scope scope(memory_context(rsinfo->econtext->ecxt_per_query_memory));

          ::Tuplestorestate *tupstore = ffi_guard{::tuplestore_begin_heap}(
              (rsinfo->allowedModes & SFRM_Materialize_Random) == SFRM_Materialize_Random, false,
              work_mem);
          rsinfo->setResult = tupstore;

          auto result = std::apply(func, t);

          for (auto it : result) {
            CHECK_FOR_INTERRUPTS();
            std::array<::Datum, nargs> values = std::apply(
                [](auto &&...elems) -> std::array<::Datum, sizeof...(elems)> {
                  return {into_nullable_datum(elems)...};
                },
                utils::tie(it));
            std::array<bool, nargs> isnull = std::apply(
                [](auto &&...elems) -> std::array<bool, sizeof...(elems)> {
                  return {into_nullable_datum(elems).is_null()...};
                },
                utils::tie(it));
            ffi_guard{::tuplestore_putvalues}(tupstore, rsinfo->expectedDesc, values.data(),
                                              isnull.data());
          }

          fc->isnull = true;
          return ::Datum(0);
        }
      } else {
        if constexpr (std::same_as<return_type, void>) {
          std::apply(func, t);
          return ::Datum(0);
        } else {
          auto result = std::apply(func, t);
          nullable_datum nd = into_nullable_datum(result);
          if (nd.is_null()) {
            fc->isnull = true;
            return ::Datum(0);
          }
          return nd.operator const ::Datum &();
        }
      }
    })();
    __builtin_unreachable();
  }
};

} // namespace cppgres
