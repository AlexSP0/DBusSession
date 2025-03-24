cmake_minimum_required(VERSION 3.16)

project(DBusSession LANGUAGES C)
if(UNIX)
    include(GNUInstallDirs)
else()
    message(FATAL_ERROR "Not unix not implemented yet.")
endif()

add_subdirectory(src)
