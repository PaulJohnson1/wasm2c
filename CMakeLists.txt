cmake_minimum_required(VERSION 3.16)

project(wasm2c)

add_compile_options(-Wall)

find_package(Threads REQUIRED)

set(BINARYEN_DIR ThirdParty/binaryen)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "")
add_subdirectory(${BINARYEN_DIR} ${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/binaryen)

include_directories(${BINARYEN_DIR}/src ThirdParty/popl/include)

add_executable(wasm2c main.cc)
target_link_libraries(wasm2c binaryen Threads::Threads)
