cmake_minimum_required(VERSION 3.20)
project(MyWindowLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)

add_library(Core STATIC
    Core/src/Window.cpp
)

target_include_directories(Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/include
)

if(WIN32)
    target_link_libraries(Core PUBLIC user32 gdi32)
endif()

add_executable(App
    App/main.cpp
)

target_link_libraries(App PRIVATE Core)