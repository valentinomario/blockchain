cmake_minimum_required(VERSION 3.30)
project(bibochain)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

add_executable(bibochain src/main.cpp
                         src/block.cpp
                         src/chain.cpp)

find_package(OpenSSL REQUIRED)

target_include_directories(bibochain PRIVATE include)

target_link_libraries(bibochain PRIVATE OpenSSL::Crypto OpenSSL::SSL)