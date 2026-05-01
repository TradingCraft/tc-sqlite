#include "Sqlite.hh"
#include "SqliteUtils.hh"
#include <gtest/gtest.h>
#include <any>
#include <cstdint>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace TC;
namespace fs = std::filesystem;

// Compile-time rejection of bindref rvalue temporaries cannot be expressed as a
// static_assert: GCC treats calling a deleted function as a hard error even
// inside a requires-expression, rather than evaluating to false. The deletions
// in Sqlite.hh (bindref(int, string&&) = delete etc.) are verified by the fact
// that this translation unit compiles — any accidental rvalue bindref call here
// would break the build.

static constexpr const char* kMemDb  = ":memory:";
static constexpr int         kRwCreate = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

// ─── SqliteDb ─────────────────────────────────────────────────────────────────

TEST(SqliteDb, DefaultConstructed)
{
    SqliteDb db;
    EXPECT_EQ(db.get(), nullptr);
    EXPECT_EQ(db.rc(), SQLITE_OK);
}

TEST(SqliteDb, OpenInMemory)
{
    SqliteDb db(kMemDb, kRwCreate);
    EXPECT_NE(db.get(), nullptr);
    EXPECT_EQ(db.rc(), SQLITE_OK);
}

TEST(SqliteDb, ExecCreateTable)
{
    SqliteDb db(kMemDb, kRwCreate);
    int rc = db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT)");
    EXPECT_EQ(rc, SQLITE_OK);
}

TEST(SqliteDb, PrepareAndStep)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE items (id INTEGER, val TEXT)");
    db.exec("INSERT INTO items VALUES (1, 'hello')");
    db.exec("INSERT INTO items VALUES (2, 'world')");

    SqliteStmt s;
    int rc = db.prepare("SELECT id, val FROM items ORDER BY id", s);
    ASSERT_EQ(rc, SQLITE_OK);

    rc = s.step();
    EXPECT_EQ(rc, SQLITE_ROW);
    int32_t id{};
    std::string val;
    s.column(0, id);
    s.column(1, val);
    EXPECT_EQ(id, 1);
    EXPECT_EQ(val, "hello");

    rc = s.step();
    EXPECT_EQ(rc, SQLITE_ROW);
    s.column(0, id);
    s.column(1, val);
    EXPECT_EQ(id, 2);
    EXPECT_EQ(val, "world");

    rc = s.step();
    EXPECT_EQ(rc, SQLITE_DONE);
}

TEST(SqliteDb, StmtFactoryMethod)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE nums (n INTEGER)");
    db.exec("INSERT INTO nums VALUES (42)");

    SqliteStmt s = db.stmt("SELECT n FROM nums");
    EXPECT_EQ(s.step(), SQLITE_ROW);
    int32_t n{};
    s.column(0, n);
    EXPECT_EQ(n, 42);
}

TEST(SqliteDb, BindAndQuery)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE kv (k TEXT, v INTEGER)");
    db.exec("INSERT INTO kv VALUES ('alpha', 10)");
    db.exec("INSERT INTO kv VALUES ('beta',  20)");

    SqliteStmt s;
    db.prepare("SELECT v FROM kv WHERE k = ?", s);
    s.bind(1, "alpha");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int32_t v{};
    s.column(0, v);
    EXPECT_EQ(v, 10);
}

// ─── SqliteStmt ───────────────────────────────────────────────────────────────

TEST(SqliteStmt, ResetAllowsReuse)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (n INTEGER)");
    db.exec("INSERT INTO t VALUES (7)");

    SqliteStmt s;
    db.prepare("SELECT n FROM t", s);

    s.step();
    s.reset();
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int32_t n{};
    s.column(0, n);
    EXPECT_EQ(n, 7);
}

TEST(SqliteStmt, ResetAndRebind)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE kv (k TEXT, v INTEGER)");
    db.exec("INSERT INTO kv VALUES ('a', 1)");
    db.exec("INSERT INTO kv VALUES ('b', 2)");

    SqliteStmt s;
    db.prepare("SELECT v FROM kv WHERE k = ?", s);

    s.bind(1, "a");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int32_t v{};
    s.column(0, v);
    EXPECT_EQ(v, 1);

    s.reset();
    s.bind(1, "b");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    s.column(0, v);
    EXPECT_EQ(v, 2);
}

TEST(SqliteStmt, Int64Column)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE big (n INTEGER)");
    db.exec("INSERT INTO big VALUES (9223372036854775807)"); // INT64_MAX

    SqliteStmt s = db.stmt("SELECT n FROM big");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int64_t n{};
    s.column(0, n);
    EXPECT_EQ(n, INT64_MAX);
}

TEST(SqliteStmt, DoubleColumn)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE fp (v REAL)");
    SqliteStmt ins;
    db.prepare("INSERT INTO fp VALUES (?)", ins);
    ins.bind(1, 3.14159);
    ins.step();

    SqliteStmt s = db.stmt("SELECT v FROM fp");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    double v{};
    s.column(0, v);
    EXPECT_DOUBLE_EQ(v, 3.14159);
}

TEST(SqliteStmt, BlobColumn)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE blobs (data BLOB)");
    Blob_t payload = {0x01, 0x02, 0x03, 0xFF};
    SqliteStmt ins;
    db.prepare("INSERT INTO blobs VALUES (?)", ins);
    ins.bindref(1, payload);
    ins.step();

    SqliteStmt s = db.stmt("SELECT data FROM blobs");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    Blob_t out;
    s.column(0, out);
    EXPECT_EQ(out, payload);
}

TEST(SqliteStmt, NullBinding)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE nv (v TEXT)");
    SqliteStmt ins;
    db.prepare("INSERT INTO nv VALUES (?)", ins);
    ins.bind(1, nullptr);
    ins.step();

    SqliteStmt s = db.stmt("SELECT v FROM nv");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    EXPECT_EQ(s.columnType(0), SQLITE_NULL);
}

TEST(SqliteStmt, StreamOperators)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE r (a INTEGER, b TEXT, c REAL)");
    SqliteStmt ins;
    db.prepare("INSERT INTO r VALUES (?, ?, ?)", ins);
    ins << int32_t(7) << std::string_view("hello") << 2.5;
    ins.step();

    SqliteStmt sel = db.stmt("SELECT a, b, c FROM r");
    ASSERT_EQ(sel.step(), SQLITE_ROW);
    int32_t a{};
    std::string b;
    double c{};
    sel >> a >> b >> c;
    EXPECT_EQ(a, 7);
    EXPECT_EQ(b, "hello");
    EXPECT_DOUBLE_EQ(c, 2.5);
}

TEST(SqliteStmt, ColumnMetadata)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE meta (id INTEGER, name TEXT, score REAL)");
    db.exec("INSERT INTO meta VALUES (1, 'x', 0.5)");

    SqliteStmt s = db.stmt("SELECT id, name, score FROM meta");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    EXPECT_EQ(s.columnCount(), 3);
    EXPECT_STREQ(s.columnName(0), "id");
    EXPECT_STREQ(s.columnName(1), "name");
    EXPECT_STREQ(s.columnName(2), "score");
    EXPECT_EQ(s.columnType(0), SQLITE_INTEGER);
    EXPECT_EQ(s.columnType(1), SQLITE_TEXT);
    EXPECT_EQ(s.columnType(2), SQLITE_FLOAT);
}

TEST(SqliteStmt, AnyColumn)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE anyt (i INTEGER, f REAL, t TEXT)");
    db.exec("INSERT INTO anyt VALUES (42, 1.5, 'world')");

    SqliteStmt s = db.stmt("SELECT i, f, t FROM anyt");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    EXPECT_EQ(std::any_cast<int64_t>(s.column(0)), 42);
    EXPECT_DOUBLE_EQ(std::any_cast<double>(s.column(1)), 1.5);
    EXPECT_EQ(std::any_cast<std::string>(s.column(2)), "world");
}

TEST(SqliteStmt, AnyColumnNull)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE nul (v TEXT)");
    db.exec("INSERT INTO nul VALUES (NULL)");

    SqliteStmt s = db.stmt("SELECT v FROM nul");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    EXPECT_FALSE(s.column(0).has_value());
}

TEST(SqliteStmt, FinalizeReleasesHandle)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (n INTEGER)");

    SqliteStmt s = db.stmt("SELECT n FROM t");
    EXPECT_NE(s.get(), nullptr);
    s.finalize();
    EXPECT_EQ(s.get(), nullptr);
}

TEST(SqliteStmt, OperatorPlusPlusIteratesAll)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE seq (n INTEGER)");
    SqliteStmt ins;
    db.prepare("INSERT INTO seq VALUES (?)", ins);
    for(int32_t i = 1; i <= 5; ++i) {
        ins.reset();
        ins.bind(1, i);
        ins.step();
    }

    SqliteStmt s = db.stmt("SELECT n FROM seq ORDER BY n");
    int32_t expected = 1;
    while(s++) {
        int32_t n{};
        s >> n;
        EXPECT_EQ(n, expected++);
    }
    EXPECT_EQ(expected, 6);
}

// ─── SqliteUtils ──────────────────────────────────────────────────────────────

TEST(SqliteUtils, TableExistsTrue)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE mytable (id INTEGER)");
    EXPECT_TRUE(TableExists(db, "mytable"));
}

TEST(SqliteUtils, TableExistsFalse)
{
    SqliteDb db(kMemDb, kRwCreate);
    EXPECT_FALSE(TableExists(db, "nonexistent"));
}

TEST(SqliteUtils, GetAllTableNamesEmpty)
{
    SqliteDb db(kMemDb, kRwCreate);
    std::set<std::string> tables;
    int rc = GetAllTableNames(db, tables);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(tables.empty());
}

TEST(SqliteUtils, GetAllTableNames)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE alpha (id INTEGER)");
    db.exec("CREATE TABLE beta  (id INTEGER)");

    std::set<std::string> tables;
    int rc = GetAllTableNames(db, tables);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(tables.size(), 2u);
    EXPECT_EQ(tables.count("alpha"), 1u);
    EXPECT_EQ(tables.count("beta"),  1u);
}

// ─── File-based DB ────────────────────────────────────────────────────────────

class SqliteFileTest : public ::testing::Test {
protected:
    fs::path m_dbPath;

    void SetUp() override
    {
        // Use the test's own suite+case name as the filename so that parallel
        // CTest shards cannot race on the same file and corrupt each other's DB.
        auto* ti = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = std::string(ti->test_suite_name()) + "_" + ti->name() + ".db";
        m_dbPath = fs::temp_directory_path() / name;
        fs::remove(m_dbPath);
    }

    void TearDown() override { fs::remove(m_dbPath); }

    std::string path() const { return m_dbPath.string(); }
};

TEST_F(SqliteFileTest, CreateFile)
{
    SqliteDb db(path(), kRwCreate);
    ASSERT_NE(db.get(), nullptr);
    EXPECT_EQ(db.rc(), SQLITE_OK);
    EXPECT_TRUE(fs::exists(m_dbPath));
}

TEST_F(SqliteFileTest, DataPersistsAcrossConnections)
{
    {
        SqliteDb db(path(), kRwCreate);
        db.exec("CREATE TABLE saved (id INTEGER, val TEXT)");
        SqliteStmt ins;
        db.prepare("INSERT INTO saved VALUES (?, ?)", ins);
        ins << int32_t(1) << std::string_view("persistent");
        ins.step();
    } // connection closed; file flushed

    SqliteDb db(path(), SQLITE_OPEN_READONLY);
    ASSERT_NE(db.get(), nullptr);
    SqliteStmt s = db.stmt("SELECT val FROM saved WHERE id = 1");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    std::string val;
    s.column(0, val);
    EXPECT_EQ(val, "persistent");
}

TEST_F(SqliteFileTest, OpenSQLiteDBUtility)
{
    {
        SqliteDb db(path(), kRwCreate);
        db.exec("CREATE TABLE t (n INTEGER)");
        db.exec("INSERT INTO t VALUES (99)");
    }

    SqliteDb db;
    int rv = OpenSQLiteDB(path(), db, SQLITE_OPEN_READONLY);
    ASSERT_EQ(rv, 0);
    ASSERT_NE(db.get(), nullptr);

    SqliteStmt s = db.stmt("SELECT n FROM t");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int32_t n{};
    s.column(0, n);
    EXPECT_EQ(n, 99);
}

TEST_F(SqliteFileTest, OpenSQLiteDBMissingFileReturnsError)
{
    SqliteDb db;
    int rv = OpenSQLiteDB(path(), db, SQLITE_OPEN_READONLY); // file doesn't exist, no CREATE
    EXPECT_NE(rv, 0);
    EXPECT_EQ(db.get(), nullptr);
}

TEST_F(SqliteFileTest, MultipleTablesAndSchema)
{
    {
        SqliteDb db(path(), kRwCreate);
        db.exec("CREATE TABLE a (x INTEGER)");
        db.exec("CREATE TABLE b (y TEXT)");
        db.exec("CREATE TABLE c (z REAL)");
    }

    SqliteDb db;
    OpenSQLiteDB(path(), db, SQLITE_OPEN_READONLY);
    std::set<std::string> tables;
    GetAllTableNames(db, tables);
    EXPECT_EQ(tables.size(), 3u);
    EXPECT_EQ(tables.count("a"), 1u);
    EXPECT_EQ(tables.count("b"), 1u);
    EXPECT_EQ(tables.count("c"), 1u);
}

// ─── Error paths ──────────────────────────────────────────────────────────────

#if SQLITECPP_EXCEPTIONS
TEST(SqliteDbError, ExecBadSqlThrows)
{
    SqliteDb db(kMemDb, kRwCreate);
    EXPECT_THROW(db.exec("NOT VALID SQL"), std::runtime_error);
}

TEST(SqliteDbError, PrepareBadSqlThrows)
{
    SqliteDb db(kMemDb, kRwCreate);
    SqliteStmt s;
    EXPECT_THROW(db.prepare("SELECT * FROM nonexistent_table_xyz", s), std::runtime_error);
}
#endif // SQLITECPP_EXCEPTIONS

TEST(SqliteDbError, ExecOnNullHandleReturnsMisuse)
{
    SqliteDb db;   // default-constructed, no open
    db.ex(false);  // disable throw so we can inspect the return code
    EXPECT_EQ(db.exec("CREATE TABLE t (n INTEGER)"), SQLITE_MISUSE);
}

TEST(SqliteDbError, PrepareOnNullHandleReturnsMisuse)
{
    SqliteDb db;
    db.ex(false);
    SqliteStmt s;
    EXPECT_EQ(db.prepare("SELECT 1", s), SQLITE_MISUSE);
}

TEST(SqliteDbError, ExceptionsDisabledReturnsErrorCode)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.ex(false);
    int rc = db.exec("THIS IS NOT SQL");
    EXPECT_NE(rc, SQLITE_OK);
    // reaching here means no exception was thrown
}

// ─── SqliteTransaction ────────────────────────────────────────────────────────

TEST(SqliteTransaction, CommitPersistsData)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (n INTEGER)");
    {
        SqliteTransaction tx(db);
        db.exec("INSERT INTO t VALUES (1)");
        tx.commit();
    }
    SqliteStmt s = db.stmt("SELECT count(*) FROM t");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int32_t count{};
    s.column(0, count);
    EXPECT_EQ(count, 1);
}

#if SQLITECPP_EXCEPTIONS
TEST(SqliteTransaction, DestructorRollsBackOnException)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (n INTEGER)");
    try {
        SqliteTransaction tx(db);
        db.exec("INSERT INTO t VALUES (1)");
        throw std::runtime_error("simulated failure");
        tx.commit(); // never reached
    } catch(...) {}

    SqliteStmt s = db.stmt("SELECT count(*) FROM t");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int32_t count{};
    s.column(0, count);
    EXPECT_EQ(count, 0);
}
#endif // SQLITECPP_EXCEPTIONS

TEST(SqliteTransaction, ExplicitRollbackRevertsData)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (n INTEGER)");
    {
        SqliteTransaction tx(db);
        db.exec("INSERT INTO t VALUES (1)");
        tx.rollback();
    } // destructor does nothing — m_done is already true
    SqliteStmt s = db.stmt("SELECT count(*) FROM t");
    ASSERT_EQ(s.step(), SQLITE_ROW);
    int32_t count{};
    s.column(0, count);
    EXPECT_EQ(count, 0);
}

// ─── SqliteDb::changes() and lastInsertRowid() ────────────────────────────────

TEST(SqliteDbMutationInfo, ChangesAfterInsert)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (n INTEGER)");
    db.exec("INSERT INTO t VALUES (1)");
    EXPECT_EQ(db.changes(), 1);
}

TEST(SqliteDbMutationInfo, ChangesAfterMultiRowUpdate)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (n INTEGER)");
    db.exec("INSERT INTO t VALUES (1)");
    db.exec("INSERT INTO t VALUES (2)");
    db.exec("INSERT INTO t VALUES (3)");
    db.exec("UPDATE t SET n = n + 10");
    EXPECT_EQ(db.changes(), 3);
}

TEST(SqliteDbMutationInfo, LastInsertRowid)
{
    SqliteDb db(kMemDb, kRwCreate);
    db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT)");
    db.exec("INSERT INTO t VALUES (42, 'hello')");
    EXPECT_EQ(db.lastInsertRowid(), 42);
}

TEST(SqliteDbMutationInfo, ChangesOnNullHandleReturnsZero)
{
    SqliteDb db;
    EXPECT_EQ(db.changes(), 0);
}

TEST(SqliteDbMutationInfo, LastInsertRowidOnNullHandleReturnsZero)
{
    SqliteDb db;
    EXPECT_EQ(db.lastInsertRowid(), 0);
}
