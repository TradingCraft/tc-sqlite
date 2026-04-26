#
# SQLite amalgamation library
#

# ---------------------------------------------------------------------------
# Sanity check that the amalgamation files are actually here
# ---------------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c")
    message(FATAL_ERROR
        "sqlite3.c not found in ${CMAKE_CURRENT_SOURCE_DIR}. "
        "Download the amalgamation from https://www.sqlite.org/download.html "
        "and place sqlite3.c, sqlite3.h (and optionally shell.c) here.")
endif()

# ---------------------------------------------------------------------------
# The library
# ---------------------------------------------------------------------------
add_library(sqlite3 STATIC sqlite3.c)
# Compatibility alias: lets consumers use SQLite::SQLite3 (FindSQLite3 convention)
add_library(SQLite::SQLite3 ALIAS sqlite3)

target_include_directories(sqlite3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(sqlite3 PROPERTIES
    C_STANDARD          11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS        OFF
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME         sqlite3
)

# ---------------------------------------------------------------------------
# Compile-time defines — these MATTER for SQLite. Tune for your workload.
# ---------------------------------------------------------------------------
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1                # serialized mode; use =2 for multi-thread (no shared cache)
    SQLITE_DEFAULT_MEMSTATUS=0         # disable memory stats — small perf win
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1   # NORMAL sync in WAL mode (durable + fast)
    SQLITE_LIKE_DOESNT_MATCH_BLOBS     # minor perf/code-size win
    SQLITE_MAX_EXPR_DEPTH=0            # remove expr-depth check
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_SHARED_CACHE           # shared-cache is discouraged upstream
    SQLITE_USE_URI=1                   # allow file:foo?mode=ro style URIs
    SQLITE_DQS=0                       # reject MySQL-style "double-quoted strings"
)

if(SQLITE_ENABLE_FTS5_OPT)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_FTS5)
endif()
if(SQLITE_ENABLE_JSON1_OPT)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_JSON1)
endif()
if(SQLITE_ENABLE_RTREE_OPT)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_RTREE)
endif()
if(SQLITE_ENABLE_DBSTAT_OPT)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_DBSTAT_VTAB)
endif()
if(SQLITE_ENABLE_SESSION_OPT)
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_ENABLE_SESSION
        SQLITE_ENABLE_PREUPDATE_HOOK
    )
endif()

if(SQLITE_ENABLE_LOAD_EXT_OPT)
    # dlopen lives in libdl on Linux; ${CMAKE_DL_LIBS} resolves to "" on Windows.
    target_link_libraries(sqlite3 PUBLIC ${CMAKE_DL_LIBS})
else()
    target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_LOAD_EXTENSION)
endif()

target_link_libraries(sqlite3 PUBLIC Threads::Threads)

# FTS5 uses log() from libm on Linux/macOS
if(UNIX)
    target_link_libraries(sqlite3 PUBLIC m)
endif()

# ---------------------------------------------------------------------------
# Per-compiler tweaks. SQLite's amalgamation is famously warning-noisy with
# -Wall, and the upstream answer is "yes, those are intentional". Silence the
# specific ones rather than blanket-disabling.
# ---------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sqlite3 PRIVATE
        -Wno-unused-parameter
        -Wno-unused-but-set-variable
        -Wno-implicit-fallthrough
        -Wno-cast-function-type
    )
elseif(MSVC)
    target_compile_options(sqlite3 PRIVATE /W3 /wd4244 /wd4267 /wd4996)
    # Treat sqlite3.c as C, not C++; stop MSVC complaining about secure CRT.
    target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
# CLI (sqlite3 / sqlite3.exe)
# ---------------------------------------------------------------------------
if(SQLITE_BUILD_SHELL AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shell.c")
    add_executable(sqlite3_cli shell.c)
    target_link_libraries(sqlite3_cli PRIVATE sqlite3)
    set_target_properties(sqlite3_cli PROPERTIES OUTPUT_NAME sqlite3)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sqlite3_cli PRIVATE -Wno-unused-parameter)
    endif()

    # The shell references sqlite3_enable_load_extension / sqlite3_load_extension
    # via the same ifdef guard the library uses, so propagate the define.
    if(NOT SQLITE_ENABLE_LOAD_EXT_OPT)
        target_compile_definitions(sqlite3_cli PRIVATE SQLITE_OMIT_LOAD_EXTENSION)
    endif()
endif()

# Install the library
install(TARGETS sqlite3
    EXPORT tc-sqliteTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install the CLI shell if it was built
if(SQLITE_BUILD_SHELL AND TARGET sqlite3_cli)
    install(TARGETS sqlite3_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install public headers directly in the include/ root
install(FILES sqlite3.h sqlite3ext.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})