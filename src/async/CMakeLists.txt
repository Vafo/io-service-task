set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(io_service_async_impl INTERFACE)

target_include_directories(io_service_async_impl INTERFACE .)

target_link_libraries(io_service_async_impl INTERFACE

    module_concurrency 
    uring

    Catch2::Catch2
    io_service_compiler_flags)
