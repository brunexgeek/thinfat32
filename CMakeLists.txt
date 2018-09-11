project(thinfat32)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfatal-errors -DDEBUG -DTF_DEBUG")

file(GLOB SOURCES "src/*")
include_directories("src/include")

add_executable(thinfat32 ${SOURCES})