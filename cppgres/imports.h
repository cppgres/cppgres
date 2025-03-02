#pragma once

extern "C" {
// clang-format off
#include <postgres.h>
#include <fmgr.h>
// clang-format on
#include <catalog/pg_type.h>
#include <miscadmin.h>
#include <utils/builtins.h>
#include <utils/tuplestore.h>
}

namespace cppgres::sys {
using Datum = ::Datum;
}
