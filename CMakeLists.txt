cmake_minimum_required(VERSION 3.31)
project(linearhash)

set(CMAKE_CXX_STANDARD 20)

add_executable(linearhash main.cpp linearhash.h loadcsv.h)
add_executable(generate_dataset generate_dataset.cpp)
