#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wregister"
extern "C" {
// clang-format off
#include <postgres.h>
#include <fmgr.h>
// clang-format on
#include <catalog/pg_type.h>
#include <executor/spi.h>
#include <miscadmin.h>
#include <utils/builtins.h>
#include <utils/tuplestore.h>
}
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

namespace cppgres::sys {
using Datum = ::Datum;
}
