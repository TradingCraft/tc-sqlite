# tc-sqlite

C++20 RAII wrapper around the SQLite3 amalgamation.  Bundles the vendored C
library (`src/sqlite/`) and exposes `TC::SqliteDb`, `TC::SqliteStmt`, and
utility functions in `TC::SqliteUtils`.  Depends on `tc-base` (provides
`Log.hh`, `Assert.hh`, `CLI.hh`).

## Build

```sh
cmake --preset gcc          # configure (Debug, GCC)
cmake --build bld_gcc       # compile
```

Other presets: `gccrel`, `cl`, `clrel` (Linux); `vc`, `vcb`, `mw`, `mwrel` (Windows).
Each preset writes into `bld_<preset>/` and installs into `inst_<preset>/`.

The `tc-base` install must be reachable via `CMAKE_PREFIX_PATH`.  The Linux
presets default to `../tc-base/inst_gcc` and `/opt/local`.

## Test

```sh
ctest --test-dir bld_gcc --output-on-failure
```

Filter to a single suite:

```sh
ctest --test-dir bld_gcc -R sqlitecpp --output-on-failure
```

## Install

```sh
cmake --install bld_gcc
```

Installs to `inst_gcc/` by default.  Override with
`-DCMAKE_INSTALL_PREFIX=/your/path` at configure time.

## Key source layout

```
src/
  sqlite/       SQLite3 amalgamation (sqlite3.c / sqlite3.h / shell.c)
  sqlitecpp/    C++ wrapper library  (Sqlite.hh/cc, SqliteUtils.hh/cc)
tst/
  sqlitecpp/    GoogleTest unit tests (Sqlite_t.cc)
cmk/            CMake helpers (Config.cmake.in)
doc/            README and LICENSE
```

## CMake options

| Option                      | Default | Effect                              |
|-----------------------------|---------|-------------------------------------|
| `SQLITE_BUILD_SHELL`        | ON      | Build the sqlite3 CLI               |
| `SQLITE_ENABLE_FTS5_OPT`    | ON      | Full-text search (FTS5)             |
| `SQLITE_ENABLE_JSON1_OPT`   | ON      | JSON1 extension                     |
| `SQLITE_ENABLE_RTREE_OPT`   | ON      | R-Tree spatial index                |
| `SQLITE_ENABLE_DBSTAT_OPT`  | OFF     | dbstat virtual table                |
| `SQLITE_ENABLE_LOAD_EXT_OPT`| OFF     | Runtime loadable extensions (dlopen)|
| `SQLITE_ENABLE_SESSION_OPT` | OFF     | Session/changeset extension         |
| `SQLITE_EXCEPTIONS`         | ON      | C++ exceptions in sqlitecpp         |
| `BUILD_TESTING`             | ON      | Configure GoogleTest targets        |

## Design notes

- `SqliteStmt` is move-only (copy is deleted).  Two instances sharing the same
  `sqlite3_stmt*` would also share cursor position, causing silent corruption.
- `bind<T>` (including `string_view`, `string`, `char*`, `Blob_t`) uses
  `SQLITE_TRANSIENT` — SQLite copies the value immediately; callers need not
  manage lifetime beyond the bind call.  `bindref<string>` / `bindref<Blob_t>`
  use `SQLITE_STATIC` — zero-copy, but the buffer must remain valid until the
  parameter is rebound or the statement is finalized.  Rvalue overloads of
  `bindref` are deleted to prevent dangling pointers at compile time.
- Tuple bind/column overloads store the raw struct bytes as a blob — only
  suitable for tuples of trivially-copyable POD types (enforced via
  `static_assert`).
- `SqliteTransaction` is a non-copyable, non-movable RAII guard: constructor
  calls `BEGIN`, destructor calls `ROLLBACK` unless `commit()` or `rollback()`
  was already called (`m_done` flag).  `SqliteDb` also exposes raw `begin()`,
  `commit()`, and `rollback()` methods for non-RAII use.
- `SqliteExceptionsEnabled` (driven by the `SQLITE_EXCEPTIONS` CMake option, default ON)
  and the per-instance `m_ex` flag together control whether errors throw
  `std::runtime_error`.  With `SQLITE_EXCEPTIONS=OFF` all throw sites are compiled
  away and `-fno-exceptions` builds are supported; the `try/catch` blocks in
  `SqliteUtils.cc` are likewise guarded by `#if SQLITECPP_EXCEPTIONS`.
