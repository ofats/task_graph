enable_testing()

project(future)

set(HEADER_FILES
    future.h)

set(SOURCE_FILES
    future.cpp)

add_library(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${PROJECT_NAME} pthread)

add_subdirectory(ut)
