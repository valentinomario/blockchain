cmake_minimum_required(VERSION 3.30)
project(bibochain)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(OpenSSL REQUIRED)

add_library(bibochain_core
            src/block.cpp
            src/chain.cpp
            src/storage/local_storage.cpp
            src/storage/storage_format.cpp)
target_include_directories(bibochain_core PUBLIC include)
target_link_libraries(bibochain_core PUBLIC OpenSSL::Crypto)

add_executable(bibochain src/main.cpp)
target_link_libraries(bibochain PRIVATE bibochain_core)

add_executable(bibochain_storage_inspector tools/storage_inspector.cpp)
target_link_libraries(bibochain_storage_inspector PRIVATE bibochain_core)

include(CTest)

if(BUILD_TESTING)
    add_executable(bibochain_integration_test tests/integration_test.cpp)
    target_link_libraries(bibochain_integration_test PRIVATE bibochain_core)
    add_test(NAME bibochain_integration COMMAND bibochain_integration_test)
endif()
