set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(io_service_async_impl
    uring.cpp
    uring_async.cpp
    acceptor.cpp
    resolver.cpp
    socket.cpp)

target_include_directories(io_service_async_impl PUBLIC .)

target_link_libraries(io_service_async_impl PUBLIC

    module_concurrency 
    uring

    io_service_impl
    io_service_compiler_flags)
