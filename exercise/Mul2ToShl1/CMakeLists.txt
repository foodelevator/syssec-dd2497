cmake_minimum_required(VERSION 3.13)
project(Mul2ToShl1 LANGUAGES C CXX)  

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake at: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(Mul2ToShl1 SHARED Mul2ToShl1.cpp)

target_link_libraries(Mul2ToShl1
  PRIVATE
    LLVMCore
    LLVMSupport
    LLVMPasses
)

if(UNIX AND NOT APPLE)
  target_link_options(Mul2ToShl1 PRIVATE -Wl,--exclude-libs,ALL)
endif()

set_target_properties(Mul2ToShl1 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "Mul2ToShl1"
)