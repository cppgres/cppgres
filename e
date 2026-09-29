In file included from /Users/yrashk/Projects/cppgres/cppgres/tests/tests.cpp:8:
In file included from /Users/yrashk/Projects/cppgres/cppgres/src/cppgres.hpp:66:
/Users/yrashk/Projects/cppgres/cppgres/src/cppgres/aggregate.hpp:38:12: error: no viable conversion from returned value of type 'void *' to function return type 'nullable_datum'
   38 |     return reinterpret_cast<void *>(state0);
      |            ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/Users/yrashk/Projects/cppgres/cppgres/src/cppgres/datum.hpp:56:8: note: candidate constructor (the implicit copy constructor) not viable: cannot convert argument of incomplete type 'void *' to 'const nullable_datum &' for 1st argument
   56 | struct nullable_datum {
      |        ^~~~~~~~~~~~~~
/Users/yrashk/Projects/cppgres/cppgres/src/cppgres/datum.hpp:56:8: note: candidate constructor (the implicit move constructor) not viable: cannot convert argument of incomplete type 'void *' to 'nullable_datum &&' for 1st argument
   56 | struct nullable_datum {
      |        ^~~~~~~~~~~~~~
/Users/yrashk/Projects/cppgres/cppgres/src/cppgres/datum.hpp:81:12: note: explicit constructor is not a candidate
   81 |   explicit nullable_datum(::NullableDatum d) : _ndatum(d) {}
      |            ^
/Users/yrashk/Projects/cppgres/cppgres/src/cppgres/datum.hpp:85:12: note: explicit constructor is not a candidate
   85 |   explicit nullable_datum(::Datum d) : _ndatum({.value = d, .isnull = false}) {}
      |            ^
/Users/yrashk/Projects/cppgres/cppgres/src/cppgres/datum.hpp:86:12: note: explicit constructor is not a candidate
   86 |   explicit nullable_datum(datum d) : _ndatum({.value = d._datum, .isnull = false}) {}
      |            ^
1 error generated.
make[2]: *** [CMakeFiles/cppgres_tests.dir/tests/tests.cpp.o] Error 1
make[1]: *** [CMakeFiles/cppgres_tests.dir/all] Error 2
make: *** [all] Error 2
