set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Concurrency tests inclusion
set(INCLUDE_CONCURRENCY_TESTS OFF)

# List test cpp's
add_library(io_service_test_suite OBJECT io_service_test.cpp)

target_link_libraries(io_service_test_suite Catch2::Catch2)
target_link_libraries(io_service_test_suite io_service_impl io_service_compiler_flags)

if(NOT INCLUDE_CONCURRENCY_TESTS)
    add_executable(io_service_test $<TARGET_OBJECTS:io_service_test_suite>)
else()
    add_executable(io_service_test 
        $<TARGET_OBJECTS:io_service_test_suite>
        $<TARGET_OBJECTS:concurrency_test_suite>
    )

    target_link_libraries(io_service_test concurrency_test_suite)
endif()


# Catch2
target_link_libraries(io_service_test io_service_test_suite Catch2::Catch2WithMain)

# Output to build dir
set_target_properties(io_service_test 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)