cmake_minimum_required(VERSION 3.15)
project(thunder_recomp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(core/include)

add_library(thunder_core STATIC
    core/src/thunder_core.c
)

# Optional SDL2 frontend for Desktop / Mobile / Modern platforms
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    message(STATUS "Found SDL2: building desktop launcher binary")
    add_executable(thunder_sdl platforms/sdl2/main.c)
    target_link_libraries(thunder_sdl PRIVATE thunder_core SDL2::SDL2)
else()
    message(STATUS "SDL2 not found: only building static thunder_core library")
endif()
