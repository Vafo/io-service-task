set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(util)

add_library(io_service_impl io_service.cpp)

target_include_directories(io_service_impl PUBLIC .)

# target_link_libraries(io_service_impl INTERFACE ...)

target_link_libraries(io_service_impl concurrency_impl io_service_util_impl)
target_link_libraries(io_service_impl io_service_compiler_flags)