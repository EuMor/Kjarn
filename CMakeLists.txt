
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

# --------------------------------------------------
# Core
# --------------------------------------------------

add_library(KjarnCore STATIC
    Core/src/Application.cpp
    Core/src/Time.cpp
    Core/src/Camera.cpp
)

add_library(Kjarn::Core ALIAS KjarnCore)

target_include_directories(KjarnCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(KjarnCore
    PRIVATE ${KJARN_COMPILE_OPTIONS}
)

# --------------------------------------------------
# Win32 Platform
# --------------------------------------------------

add_library(KjarnPlatformWin32 STATIC
    Platform/Win32/Window.cpp
    Platform/Win32/Input.cpp
)

add_library(Kjarn::PlatformWin32 ALIAS KjarnPlatformWin32)

target_include_directories(KjarnPlatformWin32
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/include
)

target_compile_definitions(KjarnPlatformWin32
    PUBLIC
        UNICODE
        _UNICODE
)

target_link_libraries(KjarnPlatformWin32
    PUBLIC
        Kjarn::Core
        user32
)

target_compile_options(KjarnPlatformWin32
    PRIVATE ${KJARN_COMPILE_OPTIONS}
)

# --------------------------------------------------
# GDI Renderer
# --------------------------------------------------

add_library(KjarnRendererGDI STATIC
    Renderer/Win32/GdiRenderer.cpp
)

add_library(Kjarn::RendererGDI ALIAS KjarnRendererGDI)

target_include_directories(KjarnRendererGDI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/include
)

target_compile_definitions(KjarnRendererGDI
    PUBLIC
        UNICODE
        _UNICODE
)

target_link_libraries(KjarnRendererGDI
    PUBLIC
        Kjarn::Core
        gdi32
        user32
)

target_compile_options(KjarnRendererGDI
    PRIVATE ${KJARN_COMPILE_OPTIONS}
)

# --------------------------------------------------
# Sandbox
# --------------------------------------------------

add_executable(Kjarn
    App/main.cpp
)

target_link_libraries(Kjarn
    PRIVATE
        Kjarn::Core
        Kjarn::PlatformWin32
        Kjarn::RendererGDI
)

target_compile_options(Kjarn
    PRIVATE ${KJARN_COMPILE_OPTIONS}
)
