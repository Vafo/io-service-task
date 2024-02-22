set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(io_service_async_detail_impl
    scoped_uring.cpp)

target_include_directories(io_service_async_detail_impl PUBLIC .)

target_link_libraries(io_service_async_detail_impl PUBLIC
    module_concurrency
    uring

    io_service_async_impl
    io_service_compiler_flags)
