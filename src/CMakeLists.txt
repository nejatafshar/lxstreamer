cmake_minimum_required(VERSION 3.14)

project(lxstreamer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC
  mongoose.c
  http_server.cpp
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
target_compile_definitions(${PROJECT_NAME} PUBLIC
    MG_ENABLE_MQTT=0
    MG_ENABLE_HTTP_WEBSOCKET=0
    MG_ENABLE_BROADCAST=0
    MG_ENABLE_GETADDRINFO
    MG_HIDE_SERVER_INFO
    MG_ENABLE_SSL=1
    )

IF (WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
ENDIF()
