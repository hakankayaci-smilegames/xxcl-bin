cmake_minimum_required(VERSION 3.10)
project(xbe-inspect VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable common warnings and compile as 32-bit
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -m32")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")


file(GLOB SOURCES "src/main.cpp" "src/xbe.cpp" "src/loader.cpp" "src/kernel.cpp" "src/kernel/*.cpp" "src/video/*.cpp")
add_executable(xbe-inspect ${SOURCES})
target_link_libraries(xbe-inspect PRIVATE SDL2)
