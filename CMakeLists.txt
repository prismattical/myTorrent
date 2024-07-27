cmake_minimum_required(VERSION 3.5.0)
project(myTorrent VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

if(CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(-Wall -Wextra -pedantic)
endif()

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

find_package(OpenSSL REQUIRED)

set(EXTERNAL_LIBS external/bencode.hpp)

set(SOURCE_FILES src/main.cpp src/config.cpp src/utils.cpp src/socket.cpp src/tracker_connection.cpp
    src/client.cpp src/download.cpp src/peer_connection.cpp src/peer_message.cpp src/platform.cpp
    src/piece.cpp)

set(HEADER_FILES include/config.hpp include/utils.hpp include/socket.hpp include/tracker_connection.hpp
    include/client.hpp include/download.hpp include/peer_connection.hpp include/peer_message.hpp include/platform.hpp
    include/piece.hpp)

add_executable(myTorrent ${EXTERNAL_LIBS} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(myTorrent OpenSSL::SSL)

target_include_directories(myTorrent PRIVATE include/ external/)

