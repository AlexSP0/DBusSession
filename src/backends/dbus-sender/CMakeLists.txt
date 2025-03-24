cmake_minimum_required(VERSION 3.16)

project(dbus-sender LANGUAGES C)
set(CMAKE_C_STANDART 17)
set(CMAKE_C_STANDART REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_check_modules(Glib20 REQUIRED IMPORTED_TARGET glib-2.0)

set(HEADERS

)
set(SOURCES
    main.c
)

add_executable( dbus-sender ${HEADERS} ${SOURCES} )

target_include_directories(dbus-sender PRIVATE ${GLIB_INCLUDE_LIBRARIES})

target_link_libraries(dbus-sender PRIVATE PkgConfig::Glib20 ${GLIB_LIBRARIES})

install(TARGETS dbus-sender
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
