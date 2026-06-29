cmake_minimum_required(VERSION 3.20)
project(dx12_metal_poc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)          # metal-cpp requires C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# metal-cpp is header-only; drop the header bundle into third_party/metal-cpp
# (download: https://developer.apple.com/metal/cpp/  — see NOTES.md M0)
set(METAL_CPP_DIR "${CMAKE_SOURCE_DIR}/third_party/metal-cpp"
    CACHE PATH "Path to metal-cpp headers")
if(NOT EXISTS "${METAL_CPP_DIR}/Metal/Metal.hpp")
  message(FATAL_ERROR
    "metal-cpp not found at ${METAL_CPP_DIR}.\n"
    "Download from https://developer.apple.com/metal/cpp/ and unpack there (NOTES.md M0).")
endif()

find_library(METAL Metal REQUIRED)
find_library(FOUNDATION Foundation REQUIRED)
find_library(QUARTZCORE QuartzCore REQUIRED)

# M0 — headless device probe (validates toolchain).
add_executable(device_probe src/probe/main.cpp)
target_include_directories(device_probe PRIVATE "${METAL_CPP_DIR}")
target_compile_options(device_probe PRIVATE -fno-objc-arc)   # metal-cpp manages refcounts itself
target_link_libraries(device_probe PRIVATE ${METAL} ${FOUNDATION} ${QUARTZCORE})

# Offscreen samples (M1 clear, M3 triangle) — core metal-cpp only, no windowing
# (DESIGN.md D-6: render-to-texture + PPM is verifiable and needs no AppKit).
foreach(sample m1_clear m3_triangle)
  add_executable(${sample} src/samples/${sample}/main.cpp)
  target_include_directories(${sample} PRIVATE "${METAL_CPP_DIR}" src)
  target_compile_options(${sample} PRIVATE -fno-objc-arc)
  target_link_libraries(${sample} PRIVATE ${METAL} ${FOUNDATION} ${QUARTZCORE})
endforeach()
