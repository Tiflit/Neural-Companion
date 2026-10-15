cmake_minimum_required(VERSION 3.20)
project(NeuralCompanion VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Vulkan SDK Resolution:
# Prefer explicit -DVULKAN_SDK=..., then $ENV{VULKAN_SDK}, then CMake find_package(Vulkan)
if(NOT DEFINED VULKAN_SDK AND DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK "$ENV{VULKAN_SDK}")
endif()

if(DEFINED VULKAN_SDK)
    set(VULKAN_INCLUDE_DIR "${VULKAN_SDK}/Include")
    set(VULKAN_LIB_DIR "${VULKAN_SDK}/Lib")
else()
    find_package(Vulkan REQUIRED)
    set(VULKAN_INCLUDE_DIR "${Vulkan_INCLUDE_DIRS}")
    set(VULKAN_LIB_DIR "")
endif()

if(NOT EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
    message(FATAL_ERROR "Vulkan headers not found. Set VULKAN_SDK environment variable or pass -DVULKAN_SDK=<path>.")
endif()

message(STATUS "Vulkan Include Dir: ${VULKAN_INCLUDE_DIR}")

# Global output directories for generated build products
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Layer shared library: compiled against SDK headers; dispatch resolved via downstream function pointers
add_library(VkLayer_neural_companion_passthrough SHARED
    src/layer/layer.h
    src/layer/layer.cpp
    src/layer/VkLayer_neural_companion_passthrough.def
)

target_include_directories(VkLayer_neural_companion_passthrough PRIVATE
    ${VULKAN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer
)

# Compiler & Linker options
if(MSVC)
    target_compile_options(VkLayer_neural_companion_passthrough PRIVATE
        /W4 /WX- /permissive- /EHsc
    )
    # Link with module definition file to ensure unmangled exported symbols
    target_sources(VkLayer_neural_companion_passthrough PRIVATE src/layer/VkLayer_neural_companion_passthrough.def)
endif()

# Deploy layer manifest into the build output directory alongside the DLL
add_custom_command(TARGET VkLayer_neural_companion_passthrough POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/layer/VkLayer_neural_companion_passthrough.json.in"
        "$<TARGET_FILE_DIR:VkLayer_neural_companion_passthrough>/VkLayer_neural_companion_passthrough.json"
    COMMENT "Deploying VkLayer_neural_companion_passthrough.json to target output directory"
)

# Test executable for layer dispatch, query semantics, and unhandled forwarding
add_executable(test_layer_dispatch
    tests/test_layer_dispatch.cpp
)

target_include_directories(test_layer_dispatch PRIVATE
    ${VULKAN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer
)

if(DEFINED VULKAN_LIB_DIR AND EXISTS "${VULKAN_LIB_DIR}/vulkan-1.lib")
    target_link_libraries(test_layer_dispatch PRIVATE "${VULKAN_LIB_DIR}/vulkan-1.lib")
elseif(TARGET Vulkan::Vulkan)
    target_link_libraries(test_layer_dispatch PRIVATE Vulkan::Vulkan)
else()
    target_link_libraries(test_layer_dispatch PRIVATE vulkan-1)
endif()

if(MSVC)
    target_compile_options(test_layer_dispatch PRIVATE
        /W4 /WX- /permissive- /EHsc
    )
endif()
