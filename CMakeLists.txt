cmake_minimum_required(VERSION 3.15)

project(io-service)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# add_library(compiler_flags INTERFACE)
# target_compile_features(compiler_flags INTERFACE cxx_std_11)

# Include src folders
add_subdirectory(src)
# add_subdirectory(test)
add_subdirectory(modules)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.3.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(io_service.app main.cpp)

target_link_libraries(io_service.app io_service_impl module_concurrency compiler_flags)

