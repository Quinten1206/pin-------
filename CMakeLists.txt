cmake_minimum_required(VERSION 3.20)
project(DeskPins VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static")

add_definitions(-DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)

add_subdirectory(dll)
add_subdirectory(src)
