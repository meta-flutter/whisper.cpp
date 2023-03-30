if (WHISPER_SUPPORT_PIPEWIRE OR WHISPER_SUPPORT_SDL2)

    # stream
    set(TARGET stream)
    add_executable(${TARGET} stream.cpp)

    include(DefaultTargetOptions)

    target_link_libraries(${TARGET} PRIVATE common ${COMMON_LIB} whisper ${CMAKE_THREAD_LIBS_INIT})
endif ()
