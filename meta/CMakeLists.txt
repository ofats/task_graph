enable_testing()

project(meta)

set(HEADER_FILES
    type_pack.h)

set(SOURCE_FILES
    type_pack.cpp)

add_library(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${PROJECT_NAME})
