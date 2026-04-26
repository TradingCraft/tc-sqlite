
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
  try {
    if(!(dbOpenFlags & SQLITE_OPEN_CREATE)) {
      std::error_code ec;
      if(!fs::exists(dbf, ec)) {
        Log(error, "DB file {} doesn't exist...", dbf);
        return 1;
      }
    }
    sqlDb = SqliteDb(dbf, dbOpenFlags);
  }
  catch(std::exception& e) {
    Log(e);
    rv = 1;
  }
  return rv;
}


// Does the given table exist in the database?
bool TableExists(const SqliteDb& db, std::string_view table)
{
  try {
    SqliteStmt stmt;
    // Parameterised query: avoids SQL injection for caller-supplied table names
    if(db.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name=?", stmt) != SQLITE_OK)
      return false;
    stmt.bind(1, table);
    return stmt.step() == SQLITE_ROW;
  }
  catch(const std::exception& e) {
    Log(e);
  }
  return false;
}


// Get all table names defined in a given database
int GetAllTableNames(const SqliteDb& db, std::set<std::string>& tableSet)
{
  try {
    tableSet.clear();
    SqliteStmt stmt;
    if(db.prepare("SELECT name FROM sqlite_master WHERE type='table'", stmt) != SQLITE_OK)
      return 1;
    while(stmt++) {
      string tbl;
      stmt.column(0, tbl);
      tableSet.emplace(std::move(tbl));
    }
    return 0;
  }
  catch(const std::exception& e) {
    Log(e);
  }
  return 1;
}


} // end namespace
