/**
 * \file cppgres.hpp
 *
 * \mainpage Cppgres: Postgres extensions in C++
 *
 * \note __GitHub__ repository: [cppgres/cppgres](https://github.com/cppgres/cppgres)
 *
 * Cppgres allows you to build Postgres extensions using C++: a high-performance, feature-rich
 * language already supported by the same compiler toolchains used to develop for Postgres,
 * like GCC and Clang.
 *
 * \subsection Features
 *
 * - Header-only library
 * - Compile and runtime safety checks
 * - Automatic type mapping
 * - Ergonomic executor API
 * - Modern C+++20 interface & implementation
 * - Direct integration with C
 *
 * \subsection qstart Quick start example
 *
 * ```
 * #include <cppgres.hpp>
 *
 * extern "C" {
 *  PG_MODULE_MAGIC;
 * }
 *
 * postgres_function(demo_len, ([](std::string_view t) { return t.length(); }));
 * ```
 *
 */
#pragma once

#include "cppgres/datum.hpp"
#include "cppgres/error.hpp"
#include "cppgres/exception_impl.hpp"
#include "cppgres/executor.hpp"
#include "cppgres/function.hpp"
#include "cppgres/guard.hpp"
#include "cppgres/imports.h"
#include "cppgres/memory.hpp"
#include "cppgres/set.hpp"
#include "cppgres/types.hpp"
#include "cppgres/xact.hpp"

/**
 * @brief Export a C++ function as a Postgres function.
 *
 * Its argument types must conform to the @ref cppgres::convertible_from_nullable_datum concept and
 * its return type must conform to the @ref cppgres::convertible_into_nullable_datum or
 * @ref cppgres::datumable_iterator concepts. This requirement is inherited from @ref cppgres::postgres_function.
 *
 * \arg name Name to export it under
 * \arg function C++ function or lambda
 *
 * \note You no longer need to use PG_FUNCTION_INFO_V1 macro.
 *
 */
#define postgres_function(name, function)                                                          \
  extern "C" {                                                                                     \
  PG_FUNCTION_INFO_V1(name);                                                                       \
  Datum name(PG_FUNCTION_ARGS) { return cppgres::postgres_function(function)(fcinfo); }            \
  }
