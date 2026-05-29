add_library(DeskPinsHook SHARED
    dllmain.cpp
    titlebarpainter.cpp
)

target_include_directories(DeskPinsHook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DeskPinsHook PRIVATE comctl32 gdi32 user32)

set_target_properties(DeskPinsHook PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
