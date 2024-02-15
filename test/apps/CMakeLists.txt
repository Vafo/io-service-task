set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(one_shot_connect
    one_shot_connect.c)

# Output to build dir
set_target_properties(one_shot_connect 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
