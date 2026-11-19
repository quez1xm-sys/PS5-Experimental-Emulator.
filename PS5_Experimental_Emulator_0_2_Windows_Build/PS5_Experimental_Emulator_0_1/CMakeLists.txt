cmake_minimum_required(VERSION 3.21)
project(PS5ExperimentalEmulator VERSION 0.1 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This prototype currently targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(PS5ExperimentalEmulator WIN32 main.cpp)
target_compile_definitions(PS5ExperimentalEmulator PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(PS5ExperimentalEmulator PRIVATE d3d12 dxgi dxguid)
