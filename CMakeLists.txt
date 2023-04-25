cmake_minimum_required(VERSION 3.4)
project(MariDS CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

set(SOURCES
    src/main.cpp
    src/common/file.cpp
    src/core/bus.cpp
    src/core/cartridge.cpp
    src/core/dma.cpp
    src/core/firmware.cpp
    src/core/intc.cpp
    src/core/ipc.cpp
    src/core/MariDS.cpp
    src/core/math.cpp
    src/core/ppu.cpp
    src/core/scheduler.cpp
    src/core/spi.cpp
    src/core/timer.cpp
    src/core/cpu/cpu.cpp
    src/core/cpu/cpuint.cpp
    src/core/cpu/cp15.cpp
)

set(HEADERS
    src/common/file.hpp
    src/common/types.hpp
    src/core/bus.hpp
    src/core/cartridge.hpp
    src/core/dma.hpp
    src/core/firmware.hpp
    src/core/intc.hpp
    src/core/ipc.hpp
    src/core/MariDS.hpp
    src/core/math.hpp
    src/core/ppu.hpp
    src/core/scheduler.hpp
    src/core/spi.hpp
    src/core/timer.hpp
    src/core/cpu/cpu.hpp
    src/core/cpu/cpuint.hpp
    src/core/cpu/cp15.hpp
)

find_package(SDL2 REQUIRED)
include_directories(MariDS ${SDL2_INCLUDE_DIRS})

add_executable(MariDS ${SOURCES} ${HEADERS})
target_link_libraries(MariDS ${SDL2_LIBRARIES})
