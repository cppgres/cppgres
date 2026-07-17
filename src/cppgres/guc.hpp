/**
 * \file
 */
#pragma once

#include "guard.hpp"
#include "imports.h"

extern "C" {
#include <utils/guc.h>
}

namespace cppgres {

/**
 * @brief Optional settings for a boolean GUC definition
 *
 * All members default to the values Postgres extensions most commonly pass, so
 * call sites only need to designated-initialize the ones they care about.
 */
struct guc_bool_options {
  const char *long_desc = nullptr;
  ::GucContext context = PGC_USERSET;
  int flags = 0;
  ::GucBoolCheckHook check_hook = nullptr;
  ::GucBoolAssignHook assign_hook = nullptr;
  ::GucShowHook show_hook = nullptr;
};

/**
 * @brief Optional settings for a string GUC definition
 *
 * All members default to the values Postgres extensions most commonly pass, so
 * call sites only need to designated-initialize the ones they care about.
 */
struct guc_string_options {
  const char *long_desc = nullptr;
  ::GucContext context = PGC_USERSET;
  int flags = 0;
  ::GucStringCheckHook check_hook = nullptr;
  ::GucStringAssignHook assign_hook = nullptr;
  ::GucShowHook show_hook = nullptr;
};

/**
 * @brief Optional settings for an enum GUC definition
 *
 * All members default to the values Postgres extensions most commonly pass, so
 * call sites only need to designated-initialize the ones they care about.
 */
struct guc_enum_options {
  const char *long_desc = nullptr;
  ::GucContext context = PGC_USERSET;
  int flags = 0;
  ::GucEnumCheckHook check_hook = nullptr;
  ::GucEnumAssignHook assign_hook = nullptr;
  ::GucShowHook show_hook = nullptr;
};

/**
 * @brief Define a custom boolean GUC (`DefineCustomBoolVariable`)
 *
 * Runs under @ref cppgres::ffi_guard, so a Postgres error surfaces as
 * @ref cppgres::pg_exception.
 */
inline void define_guc(const char *name, const char *short_desc, bool *var, bool default_value,
                       const guc_bool_options &opts = {}) {
  ffi_guard{::DefineCustomBoolVariable}(name, short_desc, opts.long_desc, var, default_value,
                                        opts.context, opts.flags, opts.check_hook,
                                        opts.assign_hook, opts.show_hook);
}

/**
 * @brief Define a custom string GUC (`DefineCustomStringVariable`)
 *
 * Runs under @ref cppgres::ffi_guard, so a Postgres error surfaces as
 * @ref cppgres::pg_exception.
 */
inline void define_guc(const char *name, const char *short_desc, char **var,
                       const char *default_value, const guc_string_options &opts = {}) {
  ffi_guard{::DefineCustomStringVariable}(name, short_desc, opts.long_desc, var, default_value,
                                          opts.context, opts.flags, opts.check_hook,
                                          opts.assign_hook, opts.show_hook);
}

/**
 * @brief Define a custom enum GUC (`DefineCustomEnumVariable`)
 *
 * Runs under @ref cppgres::ffi_guard, so a Postgres error surfaces as
 * @ref cppgres::pg_exception.
 */
inline void define_guc(const char *name, const char *short_desc, int *var, int default_value,
                       const ::config_enum_entry *entries, const guc_enum_options &opts = {}) {
  ffi_guard{::DefineCustomEnumVariable}(name, short_desc, opts.long_desc, var, default_value,
                                        entries, opts.context, opts.flags, opts.check_hook,
                                        opts.assign_hook, opts.show_hook);
}

/**
 * @brief Reserve a GUC prefix for this extension (`MarkGUCPrefixReserved`)
 *
 * Reserve the prefix only after every GUC under it has been defined:
 * reserving earlier discards the placeholders for GUCs defined later, so
 * values SET before the module was loaded would be thrown away.
 *
 * Runs under @ref cppgres::ffi_guard, so a Postgres error surfaces as
 * @ref cppgres::pg_exception.
 */
inline void reserve_guc_prefix(const char *prefix) {
#if PG_MAJORVERSION_NUM >= 15
  ffi_guard{::MarkGUCPrefixReserved}(prefix);
#else
  // Postgres 14 and older have no reservation; warning on mistyped
  // placeholders is the closest equivalent.
  ffi_guard{::EmitWarningsOnPlaceholders}(prefix);
#endif
}

} // namespace cppgres
