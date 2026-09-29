# CMake generated Testfile for 
# Source directory: /Users/yrashk/Projects/cppgres/cppgres
# Build directory: /Users/yrashk/Projects/cppgres/cppgres/build-pg19
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[cppgres_tests]=] "env" "PG_CONFIG=/Users/yrashk/Projects/cppgres/cppgres/.pg/Darwin-Debug/19beta2/build/bin/pg_config" "TEST_MODULE_PATH=/Users/yrashk/Projects/cppgres/cppgres/build-pg19/libcppgres_tests.so" "/Users/yrashk/Projects/cppgres/cppgres/test.sh")
set_tests_properties([=[cppgres_tests]=] PROPERTIES  WORKING_DIRECTORY "/Users/yrashk/Projects/cppgres/cppgres" _BACKTRACE_TRIPLES "/Users/yrashk/Projects/cppgres/cppgres/CMakeLists.txt;81;add_test;/Users/yrashk/Projects/cppgres/cppgres/CMakeLists.txt;0;")
