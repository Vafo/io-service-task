cmake_minimum_required(VERSION 3.15)

project(io_service)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(io_service_compiler_flags INTERFACE)
target_compile_features(io_service_compiler_flags INTERFACE cxx_std_20)

# Include src folders
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(modules)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.3.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(io_service_app main.cpp)

target_link_libraries(io_service_app io_service_impl module_concurrency io_service_compiler_flags)

