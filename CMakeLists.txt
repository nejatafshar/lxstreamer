cmake_minimum_required(VERSION 3.14)

project(lxstreamer
    VERSION 1.0.0
    LANGUAGES C CXX
    )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(DIR_3RDPARTIES "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty" CACHE PATH "path to 3rdparty libs")
option(HTTPS "compile with https support" ON)

list(PREPEND CMAKE_INCLUDE_PATH "${DIR_3RDPARTIES}/ffmpeg/include")
list(PREPEND CMAKE_LIBRARY_PATH "${DIR_3RDPARTIES}/ffmpeg/lib"    )
list(PREPEND CMAKE_PREFIX_PATH  "${DIR_3RDPARTIES}"        )
list(REMOVE_DUPLICATES CMAKE_INCLUDE_PATH)
list(REMOVE_DUPLICATES CMAKE_LIBRARY_PATH)
list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)

include_directories(${CMAKE_INCLUDE_PATH})
link_directories(${CMAKE_LIBRARY_PATH})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(FFMPEG REQUIRED
    COMPONENTS avfilter avformat avcodec avdevice swresample swscale avutil
    )

if(HTTPS)
    list(PREPEND CMAKE_PREFIX_PATH  "${DIR_3RDPARTIES}/openssl"        )
    find_package(OpenSSL)
    add_compile_definitions(MG_ENABLE_SSL=1)
endif()

add_subdirectory(src)
