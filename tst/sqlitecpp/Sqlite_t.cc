#include "Sqlite.hh"
#include "SqliteUtils.hh"
#include <gtest/gtest.h>
#include <set>
#include <string>

using namespace TC;

static constexpr const char* kMemDb = ":memory:";
static constexpr int kRwCreate = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

// ─── SqliteDb ─────────────────────────────────────────────────────────────────

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
