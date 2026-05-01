# tc-sqlite

This document collects the build, install, and package-consumer notes for `tc-sqlite`.

## Build

Configure and build with one of the presets in `CMakePresets.json`:

```sh
cmake --preset gcc
cmake --build bld_gcc
```

Available presets:

| Preset   | Platform | Compiler | Type    |
|----------|----------|----------|---------|
| `gcc`    | Linux    | GCC      | Debug   |
| `gccrel` | Linux    | GCC      | Release |
| `cl`     | Linux    | Clang    | Debug   |
| `clrel`  | Linux    | Clang    | Release |
| `vc`     | Windows  | MSVC     | Debug   |
| `vcb`    | Windows  | MSVC     | Debug (MSBuild) |
| `mw`     | Windows  | MinGW    | Debug   |
| `mwrel`  | Windows  | MinGW    | Release |

Each preset places build artifacts in `bld_<preset>/` and install output in `inst_<preset>/`.

To configure only the library targets and skip tests:

```sh
cmake -GNinja -Bbld_gcc -S. -DBUILD_TESTING=OFF
# or using a preset:
cmake --preset gcc -DBUILD_TESTING=OFF
```

## Options

The following CMake options control SQLite features and wrapper behaviour:

- `SQLITE_BUILD_SHELL`: Build the sqlite3 command-line shell (default ON)
- `SQLITE_ENABLE_FTS5_OPT`: Enable FTS5 full-text search (default ON)
- `SQLITE_ENABLE_JSON1_OPT`: Enable JSON1 extension (default ON)
- `SQLITE_ENABLE_RTREE_OPT`: Enable R-Tree spatial index (default ON)
- `SQLITE_ENABLE_DBSTAT_OPT`: Enable dbstat virtual table (default OFF)
- `SQLITE_ENABLE_LOAD_EXT_OPT`: Enable runtime loadable extensions (default OFF)
- `SQLITE_ENABLE_SESSION_OPT`: Enable session/changeset extension (default OFF)
- `SQLITE_EXCEPTIONS`: Enable C++ exceptions in sqlitecpp (default ON). Set to OFF to compile away all throw sites, e.g. for `-fno-exceptions` builds.
- `BUILD_TESTING`: Configure GoogleTest targets (default ON)

## Test

Run all tests after building:

```sh
ctest --test-dir bld_gcc --output-on-failure
```

To run a specific test suite:

```sh
ctest --test-dir bld_gcc -R sqlitecpp --output-on-failure
```

## Install

Install to the preset-defined prefix:

```sh
cmake --preset gcc
cmake --build bld_gcc
cmake --install bld_gcc
```

The `gcc` preset sets `CMAKE_INSTALL_PREFIX` to `./inst_gcc` by default.

To install somewhere else:

```sh
cmake --preset gcc -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build bld_gcc
cmake --install bld_gcc
```

Installed files include:

- `<prefix>/lib/libsqlite3.a`
- `<prefix>/lib/libsqlitecpp.a`
- `<prefix>/include/sqlite3.h`
- `<prefix>/include/sqlite3ext.h`
- `<prefix>/include/Sqlite.hh`
- `<prefix>/include/SqliteUtils.hh`
- `<prefix>/lib/cmake/tc-sqlite/*.cmake`

## Transactions

`SqliteTransaction` is a scoped RAII guard that ensures a transaction is never
left open on exception paths.  It calls `ROLLBACK` in its destructor if
`commit()` or `rollback()` was never called.

```cpp
TC::SqliteTransaction tx(db);                    // BEGIN DEFERRED
TC::SqliteTransaction tx(db, TC::SqliteTransactionMode::Immediate);  // BEGIN IMMEDIATE
TC::SqliteTransaction tx(db, TC::SqliteTransactionMode::Exclusive);  // BEGIN EXCLUSIVE

tx.commit();    // COMMIT  — destructor does nothing afterwards
tx.rollback();  // ROLLBACK — destructor does nothing afterwards
// scope exit without commit/rollback → automatic ROLLBACK
```

`SqliteDb` also exposes the raw transaction calls directly for scripts or
non-RAII patterns:

```cpp
db.begin();     // or db.begin(TC::SqliteTransactionMode::Immediate)
db.commit();
db.rollback();
```

Post-mutation helpers:

```cpp
int     n   = db.changes();          // rows affected by last INSERT/UPDATE/DELETE
int64_t rid = db.lastInsertRowid();  // rowid of last INSERT
```

## Consumer Usage

In another CMake project:

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_app LANGUAGES CXX)

find_package(tc-sqlite REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE tc-sqlite::sqlite3 tc-sqlite::sqlitecpp)
```

Example source:

```cpp
#include <Sqlite.hh>
#include <iostream>

int main() {
    // SQLITE_OPEN_CREATE creates the file if it does not exist.
    TC::SqliteDb db("test.db", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

    db.exec("CREATE TABLE IF NOT EXISTS t (id INTEGER PRIMARY KEY, val TEXT)");

    {
        TC::SqliteTransaction tx(db);
        TC::SqliteStmt ins = db.stmt("INSERT INTO t (val) VALUES (?)");
        ins << std::string("hello");
        ins.step();
        tx.commit();
    }

    TC::SqliteStmt sel = db.stmt("SELECT id, val FROM t");
    while(sel++) {
        int32_t id{};
        std::string val;
        sel >> id >> val;
        std::cout << id << ": " << val << "\n";
    }
    return 0;
}
```