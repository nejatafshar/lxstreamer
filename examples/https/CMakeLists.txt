cmake_minimum_required(VERSION 3.14)

project(https_streamer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} main.cpp )

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    )
target_link_libraries(${PROJECT_NAME} PRIVATE lxstreamer)
