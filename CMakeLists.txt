cmake_minimum_required(VERSION 4.0)
project(OptiMap LANGUAGES CXX)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include FetchContent for dependency management
include(FetchContent)

# Fetch Google Benchmark
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.2
)
# Disable tests and examples for Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# Add the source files
add_executable(OptiMap
    optimap/main.cpp
)

# Create benchmarks exe
add_executable(OptiMapBenchmarks
    benchmarks/array_sum_benchmark.cpp
)

# Link Google Benchmark to benchmarks
target_link_libraries(OptiMapBenchmarks 
    benchmark::benchmark 
    benchmark::benchmark_main
)

target_include_directories(OptiMap PRIVATE include)
target_include_directories(OptiMapBenchmarks PRIVATE include)

# Clang-format
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES 
        optimap/*.cpp 
        include/*.hpp 
        benchmarks/*.cpp
    )
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_CXX_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on source files"
    )
endif()

# Clang-tidy
set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")

# Doc generation
find_package(Doxygen REQUIRED)

if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.out)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
