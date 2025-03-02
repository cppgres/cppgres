#pragma once

#include "datum.h"
#include "guard.h"
#include "imports.h"
#include "set.h"
#include "types.h"
#include "utils/function_traits.h"
#include "utils/utils.h"

#include <array>
#include <complex>
#include <iostream>
#include <tuple>
#include <typeinfo>

namespace cppgres {

template <typename T>
concept convertible_into_nullable_datum_or_set_iterator =
    convertible_into_nullable_datum<T> || datumable_iterator<T>;

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
      } -> convertible_into_nullable_datum_or_set_iterator;
    };

template <datumable_function Func> struct postgres_function {
  Func func;

  explicit postgres_function(Func f) : func(f) {}

  // Use the function_traits to extract argument types.
  using traits = utils::function_traits::function_traits<Func>;
  using argument_types = typename traits::argument_types;
  using return_type = utils::function_traits::invoke_result_from_tuple_t<Func, argument_types>;
  static constexpr std::size_t arity = traits::arity;

  auto operator()(FunctionCallInfo fc) -> ::Datum {

    argument_types t;
    if (arity != fc->nargs) {
      report(ERROR, "expected %d arguments, got %d instead", arity, fc->nargs);
    } else {

      try {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
          (([&] {
             auto ptyp =
                 utils::remove_optional_t<std::remove_reference_t<decltype(std::get<Is>(t))>>();
             auto typ = type{.oid = ffi_guarded(::get_fn_expr_argtype)(fc->flinfo, Is)};
             if (!type_traits<decltype(ptyp)>::is(typ)) {
               report(ERROR, "unexpected type in position %d, can't convert `%s` into `%.*s`", Is,
                      typ.name().data(), utils::type_name<decltype(ptyp)>().length(),
                      utils::type_name<decltype(ptyp)>().data());
             }
             std::get<Is>(t) = from_nullable_datum<decltype(ptyp)>(nullable_datum(fc->args[Is]));
           }()),
           ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(t)>>{});
        if constexpr (datumable_iterator<return_type>) {
          // TODO: For now, let's assume materialized model
          auto rsinfo = reinterpret_cast<::ReturnSetInfo *>(fc->resultinfo);
          rsinfo->returnMode = SFRM_Materialize;

          ::MemoryContext per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
          ::MemoryContext oldcontext = ffi_guarded(::MemoryContextSwitchTo)(per_query_ctx);

          ::Tuplestorestate *tupstore =
              ffi_guarded(::tuplestore_begin_heap)(false, false, work_mem);
          rsinfo->setResult = tupstore;

          auto result = std::apply(func, t);

          for (auto it : result) {
            using t = set_iterator_traits<return_type>::value_type;
            constexpr auto nargs = std::tuple_size_v<t>;
            std::array<::Datum, nargs> values = std::apply(
                [](auto &&...elems) -> std::array<::Datum, sizeof...(elems)> {
                  return {into_nullable_datum(elems)...};
                },
                it);
            std::array<const bool, nargs> isnull = std::apply(
                [](auto &&...elems) -> std::array<const bool, sizeof...(elems)> {
                  return {into_nullable_datum(elems).is_null()...};
                },
                it);
            ffi_guarded(::tuplestore_putvalues)(tupstore, rsinfo->expectedDesc, values.data(),
                                                isnull.data());
          }
          ::MemoryContextSwitchTo(oldcontext);

          fc->isnull = true;
          return 0;
        } else {
          auto result = std::apply(func, t);
          nullable_datum nd = into_nullable_datum(result);
          if (nd.is_null()) {
            fc->isnull = true;
            return 0;
          }
          return nd;
        }
      } catch (const pg_exception &e) {
        error(e);
      } catch (const std::exception &e) {
        report(ERROR, "exception: %s", e.what());
      } catch (...) {
        report(ERROR, "some exception occurred");
      }
    }
    __builtin_unreachable();
  }
};

} // namespace cppgres
