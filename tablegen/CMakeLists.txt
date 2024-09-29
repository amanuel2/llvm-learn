cmake_minimum_required (VERSION 3.20.0)

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

# Add LLVM and Clang to the module path
list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
list(APPEND CMAKE_MODULE_PATH ${Clang_DIR})

# Find LLVM and Clang packages
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Include LLVM and Clang headers
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

# Include the generated headers
include_directories(BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add LLVM definitions and link directories
add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})
link_directories(${CLANG_LIBRARY_DIRS})

# Add LLVM CMake modules
include(AddLLVM)
include(HandleLLVMOptions)

add_executable(amanlang-tblgen
TableGenEmitter.cc    
Main.cc
)

target_link_libraries(amanlang-tblgen PRIVATE LLVMSupport LLVMTableGen)