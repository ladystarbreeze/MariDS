cmake_minimum_required(VERSION 3.4)
project(MariDS CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

set(SOURCES
    src/main.cpp
    src/common/file.cpp
    src/core/bus.cpp
    src/core/MariDS.cpp
)

set(HEADERS
    src/common/file.hpp
    src/common/types.hpp
    src/core/bus.hpp
    src/core/MariDS.hpp
)

add_executable(MariDS ${SOURCES} ${HEADERS})
