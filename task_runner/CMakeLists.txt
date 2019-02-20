enable_testing()

project(task_runner)

set(HEADER_FILES
    event.h
    task_system.h
    task_runner.h)

set(SOURCE_FILES
    event.cpp
    task_system.cpp
    task_runner.cpp)

add_library(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${PROJECT_NAME} pthread)

add_subdirectory(ut)
