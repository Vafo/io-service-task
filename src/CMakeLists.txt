set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(common)
add_subdirectory(async)

add_library(io_service_impl
    io_service.cpp)

target_include_directories(io_service_impl PUBLIC .)

target_link_libraries(io_service_impl
    io_common_impl
    io_service_async_impl

    module_concurrency 
    module_shared_ptr

    io_service_compiler_flags)
