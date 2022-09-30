
cmake_minimum_required(VERSION 3.23)

project(freespace2)

set(VERSION "3.6.12")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(code)

add_executable(fs2_open_${VERSION}
    code/freespace2/freespace.cpp
    code/freespace2/freespace.h
    code/freespace2/levelpaging.cpp
    code/freespace2/levelpaging.h
)
target_link_libraries(fs2_open_${VERSION} code)
