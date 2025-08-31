cmake_minimum_required(VERSION 4.0)
project(OptiMap LANGUAGES CXX)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the source files
add_executable(OptiMap
    optimap/main.cpp
)

# Include headers if you have them in /include
target_include_directories(OptiMap PRIVATE include)

# Clang-format
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES optimap/*.cpp include/*.hpp)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_CXX_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on source files"
    )
endif()

# Clang-tidy
set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")