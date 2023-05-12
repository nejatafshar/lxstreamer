cmake_minimum_required(VERSION 3.14)

project(lxstreamer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC
  streamer.cpp
  server/mongoose.c
  server/http_server.cpp
  source/source_data.cpp
  source/source.cpp
  source/demuxer.cpp
  source/codec/decoder.cpp
  source/codec/encoder.cpp
  source/codec/scaler.cpp
  source/codec/transcoder.cpp
  source/codec/resampler.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${FFMPEG_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

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