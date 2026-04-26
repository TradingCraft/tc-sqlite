#
# SQLite C++ wrapper library unit tests
#

file(GLOB test_srcs CONFIGURE_DEPENDS *_t.cc)

add_executable(sqlitecpp_t ${test_srcs})

target_link_libraries(sqlitecpp_t
  PRIVATE
    sqlitecpp
    GTest::gtest
)

gtest_discover_tests(sqlitecpp_t)