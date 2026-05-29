set(EXE_SOURCES
    main.cpp
    traymanager.cpp
    proclist.cpp
    dllinjector.cpp
    pincontroller.cpp
)

set(EXE_HEADERS
    version.h
    resource.h
    traymanager.h
    proclist.h
    dllinjector.h
    pincontroller.h
)

add_executable(DeskPins ${EXE_SOURCES} ${EXE_HEADERS} resource.rc)

# Embed the DLL binary as a post-build dependency
add_dependencies(DeskPins DeskPinsHook)

# Copy DLL next to EXE for resource compilation to find it
add_custom_command(TARGET DeskPins POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DeskPinsHook>
        $<TARGET_FILE_DIR:DeskPins>/DeskPinsHook.dll
)

target_include_directories(DeskPins PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DeskPins PRIVATE comctl32)
