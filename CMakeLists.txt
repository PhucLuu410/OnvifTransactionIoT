cmake_minimum_required(VERSION 3.18)
project(Eyerus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)

enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

add_subdirectory(CameraService)