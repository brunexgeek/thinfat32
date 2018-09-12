project(thinfat32)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfatal-errors -DDEBUG -DTF_DEBUG -pedantic -std=gnu99 -Wl,--no-undefined -fPIC -Wall -Wextra -Wconversion -Werror=return-type -Werror=implicit-function-declaration")

include_directories("src/include")

add_executable(thinfat32 "./src/fat32.c" "./src/device.c" "./src/tests.c" "./src/fat32_ui.c")