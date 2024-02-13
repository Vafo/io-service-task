set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(io_service_async_test_suite OBJECT
    async_task_test.cpp
    uring_test.cpp)

target_link_libraries(io_service_async_test_suite
    io_service_async_impl

    Catch2::Catch2

    io_service_compiler_flags)

