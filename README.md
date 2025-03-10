# Cppgres: Postgres extensions in C++
 
Cppgres allows you to build Postgres extensions using C++: a high-performance, feature-rich
language already supported by the same compiler toolchains used to develop for Postgres,
like GCC and Clang.

## Features

* Header-only library
* Compile and runtime safety checks
* Automatic type mapping
* Ergonomic executor API
* Modern C+++20 interface & implementation
* Direct integration with C

## Building

You don't really need to build this library as it is headers-only. You can,
however, use `cppgres` target from the included CMake file.

### Amalgamation

If copying the entire `src` directory is not convenient, it is quite easy to
get an amalgamated single-file header.

For example, using [cpp-amalgamate](https://github.com/Felerius/cpp-amalgamate):

```shell
cpp-amalgamate src/cppgres.hpp > cppgres.hpp
```

This file can be used just as before:

```c++
#include <cppgres.h>
```

## Running tests

```shell
# prepare
cmake -S . -B build 
# run tests
CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target all test
```

If you want to run tests on a specific version (or major version) of Postgres, specify the `PGVER` variable:

```shell
cmake -S . -B build -DPGVER=16
```

By default, our build process will set up its own copy of Postgres. If you'd like to use your own build,
specify the `PG_CONFIG` variable:

```shell
cmake -S . -B build -DPG_CONFIG=/path/to/pg_config
```
