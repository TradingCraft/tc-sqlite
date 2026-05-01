# Top level CMake project
cmake_minimum_required(VERSION 3.15)
project(tc-sqlite LANGUAGES C CXX VERSION 3.50.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Enable compile command export
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Enforce out-of-source build, fail otherwise
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)

# Make the default build type as Debug in single-configuration generators
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Platform defaults and settings
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  message(STATUS "Linux detected.")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  message(STATUS "Windows detected.")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  message(FATAL_ERROR "MacOS is not supported yet")
else()
  message(WARNING "Unknown platform")
endif() # Platform settings


# Compiler settings and defaults:
message(STATUS "CMAKE_C_COMPILER_ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "CMAKE_C_COMPILER_VERSION: ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")



# Identify compiler:
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using Gcc
  # lots of warnings and all warnings as errors
  #add_compile_options(-Wall -Wextra -pedantic -Werror)
  #
  # Add following for lttng tracing:
  #add_compile_options(-finstrument-functions)#
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # using Visual Studio C++
  # warning level 4 and all warnings as errors
  #add_compile_options(/W4 /WX)
  # Concurrent compile support (maybe /FS also?)
  add_compile_options(/MP)

  set(pdb_out ${CMAKE_CURRENT_BINARY_DIR}/pdb)
  set(CMAKE_PDB_OUTPUT_DIRECTORY ${pdb_out})
  set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${pdb_out})
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
 # using Clang/LLVM
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
 # using Intel C++
else()
  message(WARNING "Unknown compiler!")
endif()


# Adjust CMake Module Path list
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmk")

include(GNUInstallDirs)


# Options
option(SQLITE_BUILD_SHELL          "Build the sqlite3 command-line shell"            ON)
option(SQLITE_ENABLE_FTS5_OPT      "Enable FTS5 full-text search"                    ON)
option(SQLITE_ENABLE_JSON1_OPT     "Enable JSON1 extension"                          ON)
option(SQLITE_ENABLE_RTREE_OPT     "Enable R-Tree spatial index"                     ON)
option(SQLITE_ENABLE_DBSTAT_OPT    "Enable dbstat virtual table"                     OFF)
option(SQLITE_ENABLE_LOAD_EXT_OPT  "Enable runtime loadable extensions"              OFF)
option(SQLITE_ENABLE_SESSION_OPT   "Enable session/changeset extension"              OFF)
option(SQLITE_EXCEPTIONS           "Enable C++ exceptions in sqlitecpp"              ON)


# Needed packages
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(tc-base REQUIRED)


# Define sources and targets
add_subdirectory(src)


#
# Testing support:
# - `include(CTest)` defines the standard `BUILD_TESTING` CMake option.
# - Default is ON, so tests are configured when GTest is available.
# - To configure just the library/app targets without tests:
#     cmake -GNinja -Bbld_gcc -S. -DBUILD_TESTING=OFF
#
include(CTest)

if(BUILD_TESTING)
  # Define test targets
  add_subdirectory(tst)
endif()

# Build and package usage notes live in README.md and doc/README.md.

include(CMakePackageConfigHelpers)

set(TARGETS_EXPORT_NAME ${PROJECT_NAME}Targets)
set(PROJECT_CONFIG_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(PROJECT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake")

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmk/Config.cmake.in"
    ${PROJECT_CONFIG_FILE}
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
    ${PROJECT_CONFIG_VERSION_FILE}
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${PROJECT_CONFIG_FILE}
    ${PROJECT_CONFIG_VERSION_FILE}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Install exported targets
install(EXPORT ${TARGETS_EXPORT_NAME}
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
