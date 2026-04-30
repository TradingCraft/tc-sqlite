#pragma once

/** \file Sqlite.hh
 * Declarations SQLite
 *
 * (c) Copyright  Semih Cemiloglu
 * All rights reserved, see COPYRIGHT file for details.
 *
 */

// Std
#include <any>
#include <vector>
#include <memory>
#include <string>
#include <string_view>
#include <cstring>
#include <tuple>
#include <type_traits>
// Prj
#include <sqlite3.h>
#include "Assert.hh"

namespace TC {

  // Sqlite C interface entry point:
  // https://www.sqlite.org/cintro.html

  using Blob_t = std::vector<uint8_t>;

  constexpr bool SqliteExceptionsEnabled = true;

  // ================================= SqliteStmt class ==========================================

  void Sqlite3StmtDeleter(sqlite3_stmt* stmt);

  // Sqlite3 Statement handle/holder
  // https://www.sqlite.org/c3ref/stmt.html
  class SqliteStmt
  {
  private:
    std::shared_ptr<sqlite3_stmt> m_stmt;
    int m_bindPos;     // 1-based bind ordinal
    int m_colPos;      // 0-based column ordinal
    mutable int m_rc;
    mutable bool m_ex; // per-instance exceptions flag

  public:
    // CREATORS
    SqliteStmt(sqlite3_stmt* stmt = nullptr);
    ~SqliteStmt();
    // Copy is disabled: two SqliteStmt instances sharing the same sqlite3_stmt*
    // (via shared_ptr) would also share m_bindPos/m_colPos/m_rc, leading to
    // silent corruption when both sides advance the cursor independently.
    // Move is enabled to support factory-return patterns (e.g. SqliteDb::stmt()).
    SqliteStmt(const SqliteStmt&)            = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;
    SqliteStmt(SqliteStmt&&)                 = default;
    SqliteStmt& operator=(SqliteStmt&&)      = default;

    // ACCESSORS
    sqlite3_stmt* get()         { return m_stmt.get(); }
    sqlite3_stmt* operator->()  { return m_stmt.get(); }
    operator sqlite3_stmt*()    { return m_stmt.get(); }

    int  rc() const             { return m_rc; }
    bool ex() const             { return m_ex; }
    void ex(bool val) const     { m_ex = val; }
    int  ce() const             { return checkError(); }

    // MODIFIERS

    // Primary templates are deleted; only the explicit specialisations below are valid.
    template <typename T> int bind(int pos, const T t) = delete;
    template <typename T> int bindref(int pos, const T& t) = delete;

    // Block rvalue temporaries from silently compiling with bindref.
    // SQLITE_STATIC requires the buffer to stay valid until the parameter is
    // rebound or the statement is finalized — reset() alone does NOT clear
    // bindings. A temporary is destroyed before step() is ever called.
    int bindref(int pos, std::string&&) = delete;
    int bindref(int pos, Blob_t&&)      = delete;

    // Bind tuple by value (stored as a blob).
    // All element types must be trivially copyable AND standard-layout — the
    // tuple is memcpy'd raw into the blob.
    // WARNING: std::tuple layout is compiler/ABI-specific. Do NOT persist these
    // blobs across builds or use them as a cross-process serialization format.
    // Same-binary, same-session use only.
    template <typename... Args>
    int bind(int pos, const std::tuple<Args...> args)
    {
      static_assert((std::is_trivially_copyable_v<Args> && ...),
          "SqliteStmt tuple bind: all element types must be trivially copyable");
      static_assert((std::is_standard_layout_v<Args> && ...),
          "SqliteStmt tuple bind: all element types must be standard-layout (tuple blob layout is ABI-specific)");
      m_rc = sqlite3_bind_blob(m_stmt.get(), pos, &args, static_cast<int>(sizeof(args)), SQLITE_TRANSIENT);
      checkError();
      return m_rc;
    }

    // Bind tuple by reference (stored as a blob).
    // All element types must be trivially copyable AND standard-layout — same
    // constraint and ABI caveat as bind(). Same-binary, same-session use only.
    template <typename... Args>
    int bindref(int pos, const std::tuple<Args...>& args)
    {
      static_assert((std::is_trivially_copyable_v<Args> && ...),
          "SqliteStmt tuple bindref: all element types must be trivially copyable");
      static_assert((std::is_standard_layout_v<Args> && ...),
          "SqliteStmt tuple bindref: all element types must be standard-layout (tuple blob layout is ABI-specific)");
      m_rc = sqlite3_bind_blob(m_stmt.get(), pos, &args, static_cast<int>(sizeof(args)), SQLITE_TRANSIENT);
      checkError();
      return m_rc;
    }

    int  step();
    // Postfix ++ advances one row and returns true while rows remain, false on
    // SQLITE_DONE or error.  The bool return is intentional — this is NOT a
    // standard iterator; it does not return a copy of the prior state.
    // Typical use:  while(stmt++) { stmt >> col1 >> col2; }
    bool operator++(int);

    int         columnCount()         { return sqlite3_column_count(m_stmt.get()); }
    const char* columnName(int col)   { return sqlite3_column_name(m_stmt.get(), col); }
    int         columnType(int col)   { return sqlite3_column_type(m_stmt.get(), col); }
    const char* columnTypeStr(int col);
    const char* columnDeclType(int col) { return sqlite3_column_decltype(m_stmt.get(), col); }

    // Column access: out-param form (0-based ordinals)
    template <typename T> void column(int col, T& t);

    template <typename... Args>
    void column(int col, std::tuple<Args...>& args)
    {
      static_assert((std::is_trivially_copyable_v<Args> && ...),
          "SqliteStmt tuple column: all element types must be trivially copyable");
      static_assert((std::is_standard_layout_v<Args> && ...),
          "SqliteStmt tuple column: all element types must be standard-layout (tuple blob layout is ABI-specific)");
      const void* bptr = sqlite3_column_blob(m_stmt.get(), col);
      if(!bptr) return; // SQL NULL — leave args unchanged
      int size = sqlite3_column_bytes(m_stmt.get(), col);
      TC::Ensures(size == static_cast<int>(sizeof(args)));
      args = *static_cast<const std::tuple<Args...>*>(bptr);
    }

    // Column access: returns std::any (type depends on SQLite column type)
    std::any column(int col);

    // at() is a synonym for column()
    template <typename T> void at(int col, T& t) { column(col, t); }

    int reset();    // reset to initial state for re-execution
    int finalize(); // release the statement handle early

    int checkError() const;

    // << chains bind calls:  stmt << val1 << val2;
    template <typename T>
    friend SqliteStmt& operator<<(SqliteStmt& stmt, const T& t)
    { stmt.m_rc = stmt.bind(stmt.m_bindPos++, t); stmt.checkError(); return stmt; }

    // >> chains column reads: stmt >> v1 >> v2;
    template <typename T>
    friend SqliteStmt& operator>>(SqliteStmt& stmt, T& t)
    { stmt.column(stmt.m_colPos++, t); return stmt; }

  }; // class SqliteStmt


  //
  // bind() explicit specialisations — 1-based ordinals
  //
  template <> inline int SqliteStmt::bind(int i, const int32_t val)
  { return m_rc = sqlite3_bind_int(m_stmt.get(), i, val); }

  template <> inline int SqliteStmt::bind(int i, const int64_t val)
  { return m_rc = sqlite3_bind_int64(m_stmt.get(), i, val); }

  template <> inline int SqliteStmt::bind(int i, const double val)
  { return m_rc = sqlite3_bind_double(m_stmt.get(), i, val); }

  template <> inline int SqliteStmt::bind(int i, const std::string_view val)
  { return m_rc = sqlite3_bind_text(m_stmt.get(), i, val.data(), static_cast<int>(val.length()), SQLITE_TRANSIENT); }

  template <> inline int SqliteStmt::bind(int i, const char* val)
  {
    if(!val) return m_rc = sqlite3_bind_null(m_stmt.get(), i);
    return m_rc = sqlite3_bind_text(m_stmt.get(), i, val, static_cast<int>(std::strlen(val)), SQLITE_TRANSIENT);
  }

  template <> inline int SqliteStmt::bind(int i, const std::nullptr_t)
  { return m_rc = sqlite3_bind_null(m_stmt.get(), i); }

  // SQLITE_STATIC: caller promises val remains valid until the parameter is rebound
  // or the statement is finalized. reset() does NOT clear bindings.
  template <> inline int SqliteStmt::bindref(int i, const std::string& val)
  { return m_rc = sqlite3_bind_text(m_stmt.get(), i, val.data(), static_cast<int>(val.length()), SQLITE_STATIC); }

  // SQLITE_STATIC: caller promises the blob buffer remains valid until the parameter
  // is rebound or the statement is finalized. reset() does NOT clear bindings.
  // For temporary blobs, copy into a Blob_t and bind by value (SQLITE_TRANSIENT).
  template <> inline int SqliteStmt::bindref(int i, const Blob_t& v)
  { return m_rc = sqlite3_bind_blob(m_stmt.get(), i, v.data(), static_cast<int>(v.size()), SQLITE_STATIC); }


  //
  // column() explicit specialisations — 0-based ordinals
  //
  template <> inline void SqliteStmt::column(int i, int32_t& v)
  { v = sqlite3_column_int(m_stmt.get(), i); }

  template <> inline void SqliteStmt::column(int i, int64_t& v)
  { v = sqlite3_column_int64(m_stmt.get(), i); }

  template <> inline void SqliteStmt::column(int i, double& v)
  { v = sqlite3_column_double(m_stmt.get(), i); }

  template <> inline void SqliteStmt::column(int i, std::string& v)
  {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt.get(), i));
    v = text ? text : "";
  }

  template <> inline void SqliteStmt::column(int i, Blob_t& v)
  {
    const void* ptr = sqlite3_column_blob(m_stmt.get(), i);
    int size = sqlite3_column_bytes(m_stmt.get(), i);
    v.resize(static_cast<size_t>(size));
    if(size > 0 && ptr) std::memcpy(v.data(), ptr, static_cast<size_t>(size));
  }


  // ================================= SqliteDb class ============================================

  void Sqlite3Deleter(sqlite3* dbh);

  // Sqlite3 Db handle/session holder
  class SqliteDb
  {
  private:
    std::shared_ptr<sqlite3> m_dbh;
    std::string m_filename;
    int m_flags;
    mutable int m_rc;
    mutable bool m_ex;

  public:
    // CREATORS
    SqliteDb();
    SqliteDb(std::string_view filename, int flags = SQLITE_OPEN_READWRITE);
    ~SqliteDb();

    // ACCESSORS
    sqlite3* get() const        { return m_dbh.get(); }
    sqlite3* operator->() const { return m_dbh.get(); }
    operator sqlite3*() const   { return m_dbh.get(); }

    std::string_view getFileName() const { return m_filename; }
    int  getFlags() const { return m_flags; }
    int  rc()       const { return m_rc; }
    bool ex()       const { return m_ex; }
    void ex(bool val) const { m_ex = val; }
    int  ce()       const { return checkError(); }
    int  ce2()      const { return SqliteDb::CheckError(m_rc, m_ex); }

    // PRECONDITION: stmt.data() must be NUL-terminated. sqlite3_exec() does not
    // accept a length; it reads past the view boundary if no NUL is present.
    // Callers must ensure the view points to a NUL-terminated buffer
    // (std::string, string literal, or explicitly NUL-terminated char array).
    int exec(std::string_view stmt) const;
    int checkError() const;

    // MODIFIERS
    int prepare(std::string_view sqlStr, SqliteStmt& stmt) const;
    SqliteStmt stmt(std::string_view sqlStr) const;

    // STATIC MEMBERS
    static int CheckError(int rc, bool throwOnError = SqliteExceptionsEnabled);

  }; // class SqliteDb


} // namespace TC
