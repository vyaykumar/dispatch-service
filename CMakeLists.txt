cmake_minimum_required(VERSION 3.20)
project(dispatch_service_smoketest CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(smoke_test src/main.cc
        src/worker.cpp
        src/by_my_hand.cpp)

if(WIN32)
    target_link_libraries(smoke_test PRIVATE ws2_32)
endif()
