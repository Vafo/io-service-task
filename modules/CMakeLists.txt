set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(concurrency EXCLUDE_FROM_ALL)

# include(ExternalProject)

# ExternalProject_Add(
#     Concurrency
#     GIT_REPOSITORY https://github.com/Vafo/multithread-task.git
#     SOURCE_DIR concurrency
# )

add_library(module_concurrency ALIAS concurrency_impl)


