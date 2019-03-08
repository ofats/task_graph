enable_testing()

project(task_runner_ut)

set(SOURCE_FILES
    task_graph_ut.cpp
    task_runner_ut.cpp
    task_system_ut.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} catch task_runner)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
