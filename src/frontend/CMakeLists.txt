cmake_minimum_required(VERSION 3.16)

project(dbus-interactive-chat LANGUAGES C)
set(CMAKE_C_STANDART 17)
set(CMAKE_C_STANDART REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_check_modules(Glib20 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(DBUS gio-2.0 dbus-1 dbus-glib-1)

include_directories(${DBUS_INCLUDE_DIRS})

link_directories(${DBUS_LIBRARY_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})

set(HEADERS

)
set(SOURCES
    main.c
)

add_executable( dbus-interactive-chat ${HEADERS} ${SOURCES} )

target_include_directories(dbus-interactive-chat PRIVATE ${GLIB_INCLUDE_LIBRARIES})

target_link_libraries(dbus-interactive-chat PRIVATE PkgConfig::Glib20 ${GLIB_LIBRARIES} ${DBUS_LIBRARIES})

install(TARGETS dbus-interactive-chat
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
