cmake_minimum_required(VERSION 3.14)

project(lxstreamer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC
  streamer.cpp
  source_data.cpp
  source.cpp
  demuxer.cpp
  codec/decoder.cpp
  codec/encoder.cpp
  codec/scaler.cpp
  codec/transcoder.cpp
  codec/resampler.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${FFMPEG_LIBRARIES})

target_compile_definitions(${PROJECT_NAME} PRIVATE LXSTREAMER_LIBRARY)
