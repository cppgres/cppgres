#pragma once

#include "datum.h"
#include "guard.h"
#include "imports.h"
#include "types.h"
#include "utils/function_traits.h"
#include "utils/utils.h"

#include <array>
#include <iostream>
#include <tuple>
#include <typeinfo>

namespace cppgres {

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
      } -> convertible_into_nullable_datum;
    };

template <datumable_function Func> struct postgres_function {
  Func func;

  explicit postgres_function(Func f) : func(f) {}

  // Use the function_traits to extract argument types.
  using traits = utils::function_traits::function_traits<Func>;
  using argument_types = typename traits::argument_types;
  static constexpr std::size_t arity = traits::arity;

  auto operator()(FunctionCallInfo fc) -> ::Datum {

    argument_types t;
    if (arity != fc->nargs) {
      std::cout << "expected " << arity << " args" << std::endl;
    } else {

      [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (([&] {
           auto ptyp = remove_optional_t<std::remove_reference_t<decltype(std::get<Is>(t))>>();
           auto typ = type{.oid = ffi_guarded(::get_fn_expr_argtype)(fc->flinfo, Is)};
           if (!typ.template is<decltype(ptyp)>()) {
             report(ERROR, "unexpected type in position %d, can't convert `%s` into `%.*s`", Is,
                    typ.name().data(), utils::type_name<decltype(ptyp)>().length(),
                    utils::type_name<decltype(ptyp)>().data());
           }
           std::get<Is>(t) = from_nullable_datum<decltype(ptyp)>(nullable_datum(fc->args[Is]));
         }()),
         ...);
      }(std::make_index_sequence<std::tuple_size_v<decltype(t)>>{});
    }

    try {
      auto result = std::apply(func, t);
      nullable_datum nd = into_nullable_datum(result);

      if (nd.is_null()) {
        fc->isnull = true;
      }
      return nd;
    } catch (const pg_exception &e) {
      error(e);
    } catch (const std::exception &e) {
      report(ERROR, "exception: %s", e.what());
    } catch (...) {
      report(ERROR, "some exception occurred");
    }
    __builtin_unreachable();
  }
};

} // namespace cppgres
