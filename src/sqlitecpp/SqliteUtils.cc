
#include "SqliteUtils.hh"
// Std
#include <string>
#include <exception>
#include <filesystem>
#include <system_error>
// Prj
#include "Log.hh"

using namespace std;
namespace fs = std::filesystem;

namespace TC {


// Open an SQLite DB from a given file name
int OpenSQLiteDB(const std::string& dbf, SqliteDb& sqlDb, int dbOpenFlags)
{
  int rv = 0;
#if SQLITECPP_EXCEPTIONS
  try {
#endif
    if(!(dbOpenFlags & SQLITE_OPEN_CREATE)) {
      std::error_code ec;
      if(!fs::exists(dbf, ec)) {
        Log(error, "DB file {} doesn't exist...", dbf);
        return 1;
      }
    }
    sqlDb = SqliteDb(dbf, dbOpenFlags);
    // Belt-and-suspenders: if exceptions are disabled, the constructor may
    // fail silently; check the handle directly so rv reflects the real outcome.
    if(!sqlDb.get()) rv = 1;
#if SQLITECPP_EXCEPTIONS
  }
  catch(std::exception& e) {
    Log(e);
    rv = 1;
  }
#endif
  return rv;
}


// Does the given table exist in the database?
bool TableExists(const SqliteDb& db, std::string_view table)
{
#if SQLITECPP_EXCEPTIONS
  try {
#endif
    SqliteStmt stmt;
    // Parameterised query: avoids SQL injection for caller-supplied table names
    if(db.prepare("SELECT name FROM sqlite_schema WHERE type='table' AND name=?", stmt) != SQLITE_OK)
      return false;
    stmt.bind(1, table);
    return stmt.step() == SQLITE_ROW;
#if SQLITECPP_EXCEPTIONS
  }
  catch(const std::exception& e) {
    Log(e);
  }
  return false;
#endif
}


// Get all table names defined in a given database
int GetAllTableNames(const SqliteDb& db, std::set<std::string>& tableSet)
{
#if SQLITECPP_EXCEPTIONS
  try {
#endif
    tableSet.clear();
    SqliteStmt stmt;
    if(db.prepare("SELECT name FROM sqlite_schema WHERE type='table'", stmt) != SQLITE_OK)
      return 1;
    while(stmt++) {
      string tbl;
      stmt.column(0, tbl);
      tableSet.emplace(std::move(tbl));
    }
    return 0;
#if SQLITECPP_EXCEPTIONS
  }
  catch(const std::exception& e) {
    Log(e);
  }
  return 1;
#endif
}


} // end namespace
