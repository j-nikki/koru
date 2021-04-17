cmake_minimum_required(VERSION 3.19)

project(koru LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(STANDALONE ON)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
else()
  set(STANDALONE OFF)
endif()

option(KORU_TESTS "Discover Koru tests" ${STANDALONE})

add_subdirectory(koru)
if(KORU_TESTS)
  add_subdirectory(test)
endif()
