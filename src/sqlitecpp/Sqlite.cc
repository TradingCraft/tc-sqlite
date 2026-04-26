
#include "Sqlite.hh"
// Std
#include <string>
// Prj
#include "Log.hh"

using namespace std;

namespace TC {

// ─── SqliteDb ─────────────────────────────────────────────────────────────────

void Sqlite3Deleter(sqlite3* dbh)
{
  if(dbh) {
    Log(debug2, "Closing Sqlite3 Dbh={}", (void*)dbh);
    int rv = sqlite3_close_v2(dbh);
    if(rv != SQLITE_OK)
      Log(warn, "{} {}", rv, sqlite3_errstr(rv));
  }
}

SqliteDb::SqliteDb() :
  m_dbh{}, m_filename{}, m_flags{0}, m_rc{0}, m_ex{SqliteExceptionsEnabled}
{}

SqliteDb::SqliteDb(std::string_view filename, int flags) :
  m_dbh{}, m_filename{filename}, m_flags{flags}, m_rc{0}, m_ex{SqliteExceptionsEnabled}
{
  sqlite3* dbh = nullptr;
  int rv = m_rc = sqlite3_open_v2(filename.data(), &dbh, flags, nullptr);
  if(rv == SQLITE_OK) {
    m_dbh.reset(dbh, Sqlite3Deleter);
    Log(debug2, "Constructed Sqlite3 Dbh={}", (void*)m_dbh.get());
    sqlite3_extended_result_codes(dbh, 1);
  }
  else {
    Log(error, "Sqlite3 err={}", sqlite3_errmsg(dbh));
  }
}

SqliteDb::~SqliteDb() {}

// https://www.sqlite.org/c3ref/errcode.html
int SqliteDb::checkError() const
{
  int eec = sqlite3_extended_errcode(m_dbh.get());
  if(eec != SQLITE_OK) {
    const char* emsg = sqlite3_errmsg(m_dbh.get());
    const char* fmt = "Sqlite eec={} {}";
    if(eec == SQLITE_ROW || eec == SQLITE_DONE)
      Log(info, fmt, eec, emsg);
    else {
      Log(error, fmt, eec, emsg);
      if(SqliteExceptionsEnabled && m_ex) throw std::runtime_error(emsg);
    }
  }
  return eec;
}

// https://www.sqlite.org/c3ref/errcode.html
// https://www.sqlite.org/rescode.html
int SqliteDb::CheckError(int rc, bool throwOnError)
{
  if(rc != SQLITE_OK) {
    const char* emsg = sqlite3_errstr(rc);
    const char* fmt = "Sqlite rc={} {}";
    if(rc == SQLITE_ROW || rc == SQLITE_DONE)
      Log(info, fmt, rc, emsg);
    else {
      Log(error, fmt, rc, emsg);
      if(SqliteExceptionsEnabled && throwOnError) throw std::runtime_error(emsg);
    }
  }
  return rc;
}

// https://www.sqlite.org/c3ref/exec.html
int SqliteDb::exec(std::string_view stmt) const
{
  char* errmsg = nullptr;
  int rc = m_rc = sqlite3_exec(m_dbh.get(), stmt.data(), nullptr, nullptr, &errmsg);
  if(errmsg) {
    Log(error, "{}", errmsg); // log before freeing
    sqlite3_free(errmsg);
  }
  else {
    CheckError(rc, m_ex);
  }
  return rc;
}

// https://www.sqlite.org/c3ref/prepare.html
int SqliteDb::prepare(std::string_view sqlStr, SqliteStmt& stmt) const
{
  sqlite3_stmt* ppStmt = nullptr;
  const char* pzTail = nullptr;
  int rc = m_rc = sqlite3_prepare_v3(
      m_dbh.get(), sqlStr.data(), static_cast<int>(sqlStr.length()),
      0, &ppStmt, &pzTail);
  if(rc == SQLITE_OK)
    stmt = SqliteStmt(ppStmt);
  else
    CheckError(rc, m_ex);
  return rc;
}

SqliteStmt SqliteDb::stmt(std::string_view sqlStr) const
{
  SqliteStmt s;
  prepare(sqlStr, s);
  return s;
}


// ─── SqliteStmt ───────────────────────────────────────────────────────────────

void Sqlite3StmtDeleter(sqlite3_stmt* stmt)
{
  if(stmt) {
    Log(debug2, "Finalizing Sqlite3 Stmt={}", (void*)stmt);
    int rv = sqlite3_finalize(stmt);
    if(rv != SQLITE_OK)
      Log(error, "{} {}", rv, sqlite3_errstr(rv));
  }
}

SqliteStmt::SqliteStmt(sqlite3_stmt* stmt) :
  m_bindPos{1}, m_colPos{0}, m_rc{0}, m_ex{SqliteExceptionsEnabled}
{
  m_stmt.reset(stmt, Sqlite3StmtDeleter);
  if(stmt)
    Log(debug2, "Constructed Sqlite3 Stmt={}", (void*)m_stmt.get());
}

SqliteStmt::~SqliteStmt() {}

// https://www.sqlite.org/c3ref/step.html
int SqliteStmt::step()
{
  m_colPos = 0;
  m_rc = sqlite3_step(m_stmt.get());
  return checkError();
}

bool SqliteStmt::operator++(int)
{
  m_colPos = 0;
  int rc = m_rc = sqlite3_step(m_stmt.get());
  if(rc == SQLITE_ROW) return true;
  if(rc == SQLITE_DONE) {
    Log(debug1, "Stepping done for Stmt={}", (void*)m_stmt.get());
    return false;
  }
  checkError();
  return false;
}

// https://www.sqlite.org/c3ref/reset.html
int SqliteStmt::reset()
{
  m_bindPos = 1;
  m_colPos = 0;
  m_rc = sqlite3_reset(m_stmt.get());
  return checkError();
}

// https://www.sqlite.org/c3ref/finalize.html
int SqliteStmt::finalize()
{
  m_bindPos = 1;
  m_colPos = 0;
  m_stmt.reset(); // calls sqlite3_finalize() via Sqlite3StmtDeleter
  return 0;
}

int SqliteStmt::checkError() const
{
  if(m_rc != SQLITE_OK) {
    const char* emsg = sqlite3_errstr(m_rc);
    const char* fmt = "Sqlite rc={} {}";
    if(m_rc == SQLITE_ROW || m_rc == SQLITE_DONE)
      Log(debug2, fmt, m_rc, emsg);
    else {
      Log(error, fmt, m_rc, emsg);
      if(SqliteExceptionsEnabled && m_ex) throw std::runtime_error(emsg);
    }
  }
  return m_rc;
}

any SqliteStmt::column(int col)
{
  any val;
  switch(sqlite3_column_type(m_stmt.get(), col)) {
    case SQLITE_INTEGER:
      val = sqlite3_column_int64(m_stmt.get(), col); // SQLite integers are 64-bit
      break;
    case SQLITE_FLOAT:
      val = sqlite3_column_double(m_stmt.get(), col);
      break;
    case SQLITE_TEXT: {
      const char* text = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt.get(), col));
      val = std::string(text ? text : "");
      break;
    }
    case SQLITE_BLOB: {
      const void* ptr = sqlite3_column_blob(m_stmt.get(), col);
      int size = sqlite3_column_bytes(m_stmt.get(), col);
      Blob_t blob(static_cast<size_t>(size));
      if(size > 0 && ptr) std::memcpy(blob.data(), ptr, static_cast<size_t>(size));
      val = blob;
      break;
    }
    case SQLITE_NULL:
    default:
      break; // val remains empty any — caller checks with val.has_value()
  }
  return val;
}

const char* SqliteStmt::columnTypeStr(int col)
{
  switch(sqlite3_column_type(m_stmt.get(), col)) {
    case SQLITE_INTEGER: return "SQLITE_INTEGER";
    case SQLITE_FLOAT:   return "SQLITE_FLOAT";
    case SQLITE_TEXT:    return "SQLITE_TEXT";
    case SQLITE_BLOB:    return "SQLITE_BLOB";
    case SQLITE_NULL:    return "SQLITE_NULL";
    default:             return "UnknownType";
  }
}


} // end namespace
