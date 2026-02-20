cmake_minimum_required(VERSION 3.25)
project(OptiMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set release build flags (Linux/macOS only)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

include(FetchContent)

FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.2
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Debug: Print GoogleTest include directories
message(STATUS "GoogleTest Source Dir: ${googletest_SOURCE_DIR}")
message(STATUS "GoogleTest Binary Dir: ${googletest_BINARY_DIR}")

# Add optional abseil-cpp subdirectory when available
set(ABSEIL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/abseil-cpp")
if(EXISTS "${ABSEIL_DIR}/CMakeLists.txt")
    # Benchmarks currently depend on Abseil headers in source files.
    add_executable(OptiMapBenchmarks
        benchmarks/hashmap_benchmark.cpp
        benchmarks/accidentally_quadratic_benchmark.cpp
    )
    add_subdirectory("${ABSEIL_DIR}")
    target_link_libraries(OptiMapBenchmarks
        benchmark::benchmark
        benchmark::benchmark_main
        absl::flat_hash_map
        absl::raw_hash_set
    )
    target_include_directories(OptiMapBenchmarks PRIVATE include "${ABSEIL_DIR}")
else()
    message(WARNING "Optional dependency not found: Source/abseil-cpp. Skipping OptiMapBenchmarks target.")
endif()

enable_testing()

add_executable(OptiMapTests
    tests/sample_test.cpp
    tests/test_hashmap.cpp
    tests/test_at.cpp
    tests/test_contains.cpp
)

target_link_libraries(OptiMapTests
    gtest
    gtest_main
)

target_include_directories(OptiMapTests PRIVATE
    include
)

# gxhash uses AES intrinsics on x86; GCC/Clang need explicit target flags.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|i[3-6]86)$")
    target_compile_options(OptiMapTests PRIVATE -maes -msse4.1)
endif()

add_test(NAME OptiMapTests COMMAND OptiMapTests)

# Coverage configuration
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target OptiMapTests)
        target_compile_options(${target} PRIVATE -fprofile-arcs -ftest-coverage)
        target_link_options(${target} PRIVATE -fprofile-arcs -ftest-coverage)
    endforeach()
endif()

target_include_directories(OptiMapTests PRIVATE include)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
        demo.cpp
        include/*.hpp
        benchmarks/*.cpp
        tests/*.cpp
    )
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_CXX_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on source files"
    )
endif()
