project(thinfat32)
cmake_minimum_required(VERSION 2.8)
# -DDEBUG -DTF_DEBUG
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfatal-errors  -pedantic -std=gnu99 -Wl,--no-undefined -fPIC -Wall -Wextra -Wconversion -Werror=return-type -Werror=implicit-function-declaration")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=incompatible-pointer-types")

include_directories("src/include")

add_executable(thinfat32 "./src/fat32.c" "./src/device.c" "./src/tests.c" "./src/fat32_ui.c")

add_executable(fusequark "./src/quark.c" "./src/device.c" "./src/fusequark.c")
target_link_libraries(fusequark "fuse")

add_executable(fusefat32 "./src/fat32.c" "./src/device.c" "./src/fuse.c" "./src/fat32_ui.c")
target_link_libraries(fusefat32 "fuse")