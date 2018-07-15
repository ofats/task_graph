enable_testing()

project(future_ut)

set(SOURCE_FILES
    future_ut.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} catch future)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
