cmake_minimum_required(VERSION 3.10)

project(lock_free_priority_queue VERSION 1.0 LANGUAGES CXX)

add_subdirectory(3rdparty/MPMCQueue EXCLUDE_FROM_ALL)

add_library(${PROJECT_NAME} INTERFACE)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)
target_link_libraries(${PROJECT_NAME} INTERFACE MPMCQueue::MPMCQueue)
target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

add_executable(example
        example.cpp)

target_link_libraries(example PRIVATE ${PROJECT_NAME})
