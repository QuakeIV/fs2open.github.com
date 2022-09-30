
cmake_minimum_required(VERSION 3.23)

project(freespace2)

set(VERSION "3.6.12_QUAKE")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# note: these are not major issues for C (at all, even remotely), which this codebase originally was, however consider trying to eliminate some of these later
add_compile_options(
    -Wno-write-strings
    -Wno-conversion-null
    -Wno-format-truncation
    -Wno-format-security
    -Wno-format-overflow
    -Wno-pointer-arith
)

add_subdirectory(code)

add_executable(fs2_open_${VERSION}
    code/freespace2/freespace.cpp
    code/freespace2/freespace.h
    code/freespace2/levelpaging.cpp
    code/freespace2/levelpaging.h
)
target_link_libraries(fs2_open_${VERSION} code)
