cmake_minimum_required(VERSION 3.16)
project("aman-jit")

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Enable compilation database export
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the path to your custom LLVM installation
set(LLVM_HOME /Users/aman/Documents/llvm-project/install)
set(LLVM_DIR ${LLVM_HOME}/lib/cmake/llvm)
set(Clang_DIR ${LLVM_HOME}/lib/cmake/clang)


# Find LLVM and Clang packages
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Include LLVM and Clang headers
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(Lexer.h Parser.h Sema.h CodeGen.h)

# Add LLVM definitions and link directories
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})

# Set source files
set(SOURCE_FILES main.cc 
                 JIT.cc
                 )

# Add executable
add_executable(aman-jit ${SOURCE_FILES})

# Set compile flags
set_target_properties(aman-jit PROPERTIES COMPILE_FLAGS "-fno-rtti")

# Link against LLVM and Clang libraries
target_link_libraries(aman-jit
  PRIVATE
  clangTooling
  clangBasic
  clangFrontend
  clangASTMatchers
  clangAST
  clangLex
  LLVMSupport
  LLVMOrcJIT
  LLVMExecutionEngine
)

add_custom_command(
  TARGET aman-jit POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_BINARY_DIR}/compile_commands.json
          ${CMAKE_SOURCE_DIR}/../compile_commands.json
)

