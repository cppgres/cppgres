#!/usr/bin/env bash

set -e

if [ -z "${PG_CONFIG}" ]; then
  echo "PG_CONFIG must be configured"
  exit 1
fi

if [ -z "${BUILD_DIR}" ]; then
  echo "BUILD_DIR must be configured"
  exit 1
fi

_pg_bindir=$(${PG_CONFIG} --bindir)

rm -rf .testdb
${_pg_bindir}/initdb -D .testdb --no-clean --no-sync --locale=C --encoding=UTF8
${_pg_bindir}/pg_ctl -D .testdb start -o "-c listen_addresses='' -c unix_socket_directories=\"$(realpath .testdb)\""

cleanup() {
  ${_pg_bindir}/pg_ctl -D .testdb stop
}

trap cleanup ERR

${_pg_bindir}/psql -v ON_ERROR_STOP=1 -h $(realpath .testdb) -d postgres -c "load '${BUILD_DIR}/libcppgres_tests.so';"
${_pg_bindir}/psql -v ON_ERROR_STOP=1 -h $(realpath .testdb) -d postgres -c "call cppgres_tests();"
${_pg_bindir}/pg_ctl -D .testdb stop
rm -rf .testdb
