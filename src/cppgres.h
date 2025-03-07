/**
 * \file cppgres.h
 *
 * \mainpage Cppgres: Postgres extensions in C++
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
 * ```c++
 * #include <cppgres.h>
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

#include "cppgres/datum.h"
#include "cppgres/error.h"
#include "cppgres/exception_impl.h"
#include "cppgres/executor.h"
#include "cppgres/function.h"
#include "cppgres/guard.h"
#include "cppgres/imports.h"
#include "cppgres/memory.h"
#include "cppgres/set.h"
#include "cppgres/types.h"
#include "cppgres/xact.h"

/**
 * Export a C++ function as a Postgres function.
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
