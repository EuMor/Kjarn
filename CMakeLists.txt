
cmake_minimum_required(VERSION 3.20)

project(Kjarn
    VERSION 0.1.0
    DESCRIPTION "Kjarn Game Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR
        "The current Kjarn platform layer supports Windows only."
    )
endif()

add_library(KjarnCore STATIC
    Core/src/Window.cpp
    Core/src/Input.cpp
    Core/src/Renderer.cpp
    Core/src/Time.cpp
    Core/src/Camera.cpp
)

add_library(Kjarn::Core ALIAS KjarnCore)

target_include_directories(KjarnCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/include
)

target_compile_definitions(KjarnCore
    PUBLIC
        UNICODE
        _UNICODE
)

target_link_libraries(KjarnCore
    PUBLIC
        user32
        gdi32
)

add_executable(Kjarn
    App/main.cpp
)

target_link_libraries(Kjarn
    PRIVATE
        Kjarn::Core
)
if(MSVC)
    set(KJARN_COMPILE_OPTIONS
        /W4
        /permissive-
    )
else()
    set(KJARN_COMPILE_OPTIONS
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

target_compile_options(KjarnCore PRIVATE ${KJARN_COMPILE_OPTIONS})
target_compile_options(Kjarn PRIVATE ${KJARN_COMPILE_OPTIONS})