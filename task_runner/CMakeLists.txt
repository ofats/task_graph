enable_testing()

project(task_runner)

set(HEADER_FILES
    event.h
    task_graph.h
    task_runner.h
    task_system.h)

set(SOURCE_FILES
    event.cpp
    task_graph.cpp
    task_runner.cpp
    task_system.cpp)

add_library(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${PROJECT_NAME} pthread meta)

add_subdirectory(ut)
