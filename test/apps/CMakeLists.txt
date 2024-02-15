set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(one_shot_connect
    one_shot_connect.c)

add_executable(connector
    connector.cpp)

target_link_libraries(connector
    io_service_impl)

# Output to build dir
set_target_properties(one_shot_connect 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_target_properties(connector 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
