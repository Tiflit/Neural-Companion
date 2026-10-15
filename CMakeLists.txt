cmake_minimum_required(VERSION 3.20)
project(NeuralCompanion VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Pinned Vulkan SDK path fallback
if(NOT DEFINED VULKAN_SDK AND DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK "$ENV{VULKAN_SDK}")
elseif(NOT DEFINED VULKAN_SDK)
    set(VULKAN_SDK "D:/dev/VulkanSDK/1.4.363.0")
endif()

message(STATUS "Vulkan SDK Root: ${VULKAN_SDK}")

set(VULKAN_INCLUDE_DIR "${VULKAN_SDK}/Include")
if(NOT EXISTS "${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
    message(FATAL_ERROR "Vulkan headers not found at ${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
endif()

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
