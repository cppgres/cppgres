#ifdef __cplusplus
#pragma once
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wregister"
#endif
#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#include <postgres.h>
#include <fmgr.h>
// clang-format on
#include <catalog/pg_class.h>
#include <executor/spi.h>
#include <miscadmin.h>
#include <nodes/execnodes.h>
#include <utils/builtins.h>
#include <utils/expandeddatum.h>
#include <utils/memutils.h>
#include <utils/tuplestore.h>
#ifdef __cplusplus
}
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
