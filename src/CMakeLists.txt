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

add_dependencies(DeskPins DeskPinsHook)

# Pre-build: copy DLL to src/ so windres can embed it as RCDATA
add_custom_command(TARGET DeskPins PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DeskPinsHook>
        ${CMAKE_CURRENT_SOURCE_DIR}/DeskPinsHook.dll
    COMMENT "Copying DeskPinsHook.dll for resource embedding"
)

# Post-build: copy DLL alongside EXE for runtime extraction fallback
add_custom_command(TARGET DeskPins POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DeskPinsHook>
        $<TARGET_FILE_DIR:DeskPins>/DeskPinsHook.dll
)

target_include_directories(DeskPins PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DeskPins PRIVATE comctl32)
