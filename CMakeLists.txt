cmake_minimum_required(VERSION 3.5.0)
project(myTorrent VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

option(BUILD_TESTS "Enable building tests" OFF) # OFF by default

if(CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(-Wall -Wextra -pedantic)
endif()

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

find_package(OpenSSL REQUIRED)

set(EXTERNAL_LIBS external/bencode.hpp external/expected.hpp)

set(SOURCE_FILES src/config.cpp src/utils.cpp src/socket.cpp src/tracker_connection.cpp
    src/download.cpp src/peer_connection.cpp src/peer_message.cpp src/download_strategy.cpp
    src/announce_list.cpp src/metainfo_file.cpp src/file_handler.cpp src/piece.cpp
    )

set(HEADER_FILES include/config.hpp include/utils.hpp include/socket.hpp include/tracker_connection.hpp
    include/download.hpp include/peer_connection.hpp include/peer_message.hpp include/download_strategy.hpp
    include/announce_list.hpp include/metainfo_file.hpp include/file_handler.hpp include/piece.hpp
    )

add_executable(myTorrent src/main.cpp ${EXTERNAL_LIBS} ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(myTorrent OpenSSL::SSL)

target_include_directories(myTorrent PRIVATE include/ external/)


if(BUILD_TESTS)
  include(CTest)

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_executable(dlstrategy_test test/dlstrategy.cpp ${EXTERNAL_LIBS} ${SOURCE_FILES} ${HEADER_FILES})
  target_link_libraries(
    dlstrategy_test
    GTest::gtest_main
    OpenSSL::SSL
  )

  include(GoogleTest)
  gtest_discover_tests(dlstrategy_test)

  add_test(NAME DLStrategy COMMAND dlstrategy_test)

  target_include_directories(dlstrategy_test PRIVATE include/ external/)
endif()

