cmake_minimum_required(VERSION 3.10)
project(mpi_api_mashchenko)

set(CMAKE_CXX_STANDARD 20)

add_executable(prod-cons prod-cons_test.cpp src/myMPI.cpp src/myMPI.h)
add_executable(prod-cons-async prod-cons_async.cpp src/myMPI.cpp src/myMPI.h)

target_link_libraries(prod-cons pthread rt)
target_link_libraries(prod-cons-async pthread rt)