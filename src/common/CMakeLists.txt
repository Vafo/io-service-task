set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(io_common_impl INTERFACE)

target_include_directories(io_common_impl INTERFACE .)

target_link_libraries(io_common_impl INTERFACE io_service_compiler_flags)
