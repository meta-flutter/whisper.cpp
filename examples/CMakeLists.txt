# dependencies

find_package(Threads REQUIRED)

# third-party

if (WHISPER_SUPPORT_SDL2)
    # SDL2
    find_package(SDL2 REQUIRED)

    string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)

    message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")
    message(STATUS "SDL2_LIBRARIES = ${SDL2_LIBRARIES}")
endif()

if (WHISPER_SUPPORT_PIPEWIRE)
    # PipeWire, SPA
    find_package(PkgConfig)
    pkg_search_module(PIPEWIRE libpipewire-0.3 libpipewire-0.2 REQUIRED)
    pkg_search_module(SPA libspa-0.2 libspa-0.1 REQUIRED)

    string(STRIP "${PIPEWIRE_LIBRARIES}" PIPEWIRE_LIBRARIES)

    message(STATUS "PIPEWIRE_INCLUDE_DIRS = ${PIPEWIRE_INCLUDE_DIRS}")
    message(STATUS "PIPEWIRE_LINK_LIBRARIES = ${PIPEWIRE_LINK_LIBRARIES}")
    message(STATUS "SPA_INCLUDE_DIRS = ${SPA_INCLUDE_DIRS}")
endif()

# common

set(TARGET common)

add_library(${TARGET} STATIC
    common.h
    common.cpp
    )

include(DefaultTargetOptions)

set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (WHISPER_SUPPORT_SDL2)
    # common-sdl

    set(TARGET common-sdl)

    add_library(${TARGET} STATIC
        common-sdl.h
        common-sdl.cpp
        )

    include(DefaultTargetOptions)

    target_include_directories(${TARGET} PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(${TARGET} PRIVATE ${SDL2_LIBRARIES})

    set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(COMMON_LIB ${TARGET})
endif()

if (WHISPER_SUPPORT_PIPEWIRE)
    # common-pw

    set(TARGET common-pw)

    add_library(${TARGET} STATIC
            common-pw.h
            common-pw.cpp
            )

    include(DefaultTargetOptions)

    target_include_directories(${TARGET} PUBLIC ${PIPEWIRE_INCLUDE_DIRS} ${SPA_INCLUDE_DIRS})
    target_link_libraries(${TARGET} PRIVATE ${PIPEWIRE_LINK_LIBRARIES})

    set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(COMMON_LIB ${TARGET})
endif()

# examples

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (EMSCRIPTEN)
    add_subdirectory(whisper.wasm)
    add_subdirectory(stream.wasm)
    add_subdirectory(command.wasm)
    add_subdirectory(talk.wasm)
    add_subdirectory(bench.wasm)
elseif(CMAKE_JS_VERSION)
    add_subdirectory(addon.node)
else()
    add_subdirectory(main)
    add_subdirectory(stream)
    add_subdirectory(command)
    add_subdirectory(bench)
    add_subdirectory(talk)
    add_subdirectory(talk-llama)
endif()
