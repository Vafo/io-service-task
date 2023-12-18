set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Order is important
add_subdirectory(concurrency EXCLUDE_FROM_ALL)
add_subdirectory(shared_ptr EXCLUDE_FROM_ALL)

# include(ExternalProject)

# ExternalProject_Add(
#     Concurrency
#     GIT_REPOSITORY https://github.com/Vafo/multithread-task.git
#     SOURCE_DIR concurrency
# )

add_library(module_concurrency ALIAS concurrency_impl)
add_library(module_shared_ptr ALIAS shared_ptr_impl)


