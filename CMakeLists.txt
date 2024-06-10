cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 14)

project(ModbusTCP)

file(GLOB SOURCES "*.cpp" "*.h" "./Socket/*.cxx" "./Socket/*.hxx")

add_executable(${PROJECT_NAME} ${SOURCES})