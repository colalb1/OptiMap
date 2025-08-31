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