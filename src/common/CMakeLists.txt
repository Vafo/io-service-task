set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(io_common_impl
    logger.cpp)

target_include_directories(io_common_impl PUBLIC .)

target_link_libraries(io_common_impl PUBLIC
    module_concurrency
    io_service_compiler_flags)
