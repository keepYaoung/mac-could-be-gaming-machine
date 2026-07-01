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

# Offscreen samples — core metal-cpp only, no windowing
# (DESIGN.md D-6: render-to-texture + PPM is verifiable and needs no AppKit).
#   m1_clear      : M1 clear-color offscreen
#   m3_triangle   : M3-prime triangle with hand-written MSL (harness proof)
#   m2_translated : M2 same triangle rendered from a HLSL->DXIL->metallib
#                   pipeline (proves shader translation). Requires the
#                   metallibs built by tools/build_shaders.sh, loaded at run
#                   time from build/shaders/.
#   m4_texture    : M4 textured fullscreen quad via MSC argument buffers
#                   (TLAB + resource/sampler descriptor heaps). Uses
#                   /usr/local/include/metal_irconverter_runtime for the
#                   inline IRDescriptorTableSet* helpers (no lib link).
foreach(sample m1_clear m3_triangle m2_translated m4_texture)
  add_executable(${sample} src/samples/${sample}/main.cpp)
  target_include_directories(${sample} PRIVATE "${METAL_CPP_DIR}" src)
  target_compile_options(${sample} PRIVATE -fno-objc-arc)
  target_link_libraries(${sample} PRIVATE ${METAL} ${FOUNDATION} ${QUARTZCORE})
endforeach()
