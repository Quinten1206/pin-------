cmake_minimum_required(VERSION 3.16)
project(DeskPinsHook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s -static")

add_definitions(-DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)

add_library(DeskPinsHook SHARED dllmain.cpp)
target_link_libraries(DeskPinsHook PRIVATE gdi32 user32 comctl32)
