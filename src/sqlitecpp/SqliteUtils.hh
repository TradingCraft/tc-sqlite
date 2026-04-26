#pragma once

/** \file SqliteUtils.hh
 * Declarations SQLite Utilities
 *
 * (c) Copyright  Semih Cemiloglu
 * All rights reserved, see COPYRIGHT file for details.
 *
 */

// Std
#include <set>
#include <string>
#include <string_view>
// Prj
#include "Sqlite.hh"


namespace TC {

  // Open an SQLite DB from a given file name
  int OpenSQLiteDB(const std::string& dbf, SqliteDb& sqlDb, int dbOpenFlags = SQLITE_OPEN_READONLY);

  // Does the given table exist in the database?
  bool TableExists(const SqliteDb& db, std::string_view table);

  // Get all table names defined in a given database
  int GetAllTableNames(const SqliteDb& db, std::set<std::string>& tableSet);

} // namespace TC
