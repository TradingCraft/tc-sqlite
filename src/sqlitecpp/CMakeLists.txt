#
# SQLite C++ wrapper library
#

file(GLOB libSrcs CONFIGURE_DEPENDS *.cc)
file(GLOB libHdrs CONFIGURE_DEPENDS *.h *.hh *.hpp)

# Target Library name
set(tgt sqlitecpp)

add_library(${tgt} STATIC)

target_sources(${tgt}
    PRIVATE ${libSrcs}
  )

target_include_directories(${tgt}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${tgt}
  PUBLIC
    sqlite3
    tc-base::base
)

# Install the library
install(TARGETS ${tgt}
    EXPORT tc-sqliteTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install public headers directly in the include/ root
install(FILES ${libHdrs} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})