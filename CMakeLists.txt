cmake_minimum_required(VERSION 3.16)
project(cpp_concurrency_box)

set(CMAKE_CXX_STANDARD 23)

find_package(Threads REQUIRED)

option(BUILD_Q_TESTS "Build project tests" TRUE)
option(SANITIZE_Q_APPS "Build apps with sanitizer flags" FALSE)

# aggregate compile and link options
add_library(apricot INTERFACE)
target_include_directories(apricot INTERFACE include)
target_link_libraries(apricot INTERFACE Threads::Threads)

set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

target_compile_options(apricot
    INTERFACE
    "$<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wall;-Wextra;-Wshadow;-Wformat=2;-Wunused>>"
    "$<${msvc_cxx}:$<BUILD_INTERFACE:-W3>>"
)

add_subdirectory(apps)

if (${BUILD_Q_TESTS})
  enable_testing()
  include(FetchContent)
  add_subdirectory(test)
endif ()
