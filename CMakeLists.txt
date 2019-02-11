cmake_minimum_required(VERSION 3.7)
enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "-O1 -fno-omit-frame-pointer -g -fsanitize=address -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra -Werror -g")

project(advanced_future)

include_directories(./)

add_subdirectory(future)
add_subdirectory(task_runner)
add_subdirectory(catch)
