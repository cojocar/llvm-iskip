cmake_minimum_required(VERSION 3.4.3)
project(iskip)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)


set(LLVM_BUILD_DIR
  $ENV{LLVM_BUILD_DIR})

add_definitions(${LLVM_DEFINITIONS})
include_directories(include ${LLVM_INCLUDE_DIRS}
  "${LLVM_BUILD_DIR}/lib/Target/ARM")

##get_cmake_property(_variableNames VARIABLES)
##foreach (_variableName ${_variableNames})
##      message(STATUS "${_variableName}=${${_variableName}}")
##    endforeach()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
add_subdirectory(lib)
