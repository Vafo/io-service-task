set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# add_subdirectory(...)

add_library(io_service_impl INTERFACE)

# target_link_libraries(io_service_impl INTERFACE ...)

target_link_libraries(io_service_impl INTERFACE compiler_flags)