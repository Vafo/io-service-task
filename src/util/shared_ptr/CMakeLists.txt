set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# add_subdirectory(...)

add_library(io_service_shared_ptr_impl INTERFACE)

target_include_directories(io_service_shared_ptr_impl INTERFACE .)

target_link_libraries(io_service_shared_ptr_impl INTERFACE io_service_checked_delete_impl io_service_compiler_flags)