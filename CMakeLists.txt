cmake_minimum_required(VERSION 3.5)

project(mine-tank-cpp)

file(GLOB_RECURSE SOURCES "src/*")

add_executable(${PROJECT_NAME} ${SOURCES})