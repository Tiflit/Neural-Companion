cmake_minimum_required(VERSION 3.20)
project(NeuralGroupDispatchCpuTests LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The CPU fixture builds the Windows layer.")
endif()
if(NOT EXISTS "${NC_VULKAN_HEADERS}/include/vulkan/vk_layer.h")
    message(FATAL_ERROR "Provide the reviewed Vulkan-Headers checkout through NC_VULKAN_HEADERS.")
endif()

set(layer_source "${CMAKE_CURRENT_SOURCE_DIR}/../../src/layer")
add_library(cpu_layer SHARED "${layer_source}/layer.cpp"
    "${layer_source}/VkLayer_neural_companion_passthrough.def")
set_target_properties(cpu_layer PROPERTIES OUTPUT_NAME VkLayer_neural_companion_passthrough)
target_include_directories(cpu_layer PRIVATE "${NC_VULKAN_HEADERS}/include" "${layer_source}")
target_compile_features(cpu_layer PRIVATE cxx_std_17)

add_executable(test_group_dispatch test_group_dispatch.cpp)
target_include_directories(test_group_dispatch PRIVATE "${NC_VULKAN_HEADERS}/include")
target_compile_features(test_group_dispatch PRIVATE cxx_std_17)
add_dependencies(test_group_dispatch cpu_layer)
if(MSVC)
    target_compile_options(cpu_layer PRIVATE /W4 /WX- /permissive- /EHsc)
    target_compile_options(test_group_dispatch PRIVATE /W4 /WX /permissive- /EHsc)
endif()

enable_testing()
foreach(case_name IN ITEMS CoreOnly KhrOnly BothCore BothKhr Neither CoreCount KhrCount
        CoreIncomplete KhrIncomplete CoreError KhrError InstanceIsolation Cleanup NullQueries)
    add_test(NAME GroupDispatch.${case_name}
        COMMAND test_group_dispatch "$<TARGET_FILE:cpu_layer>" ${case_name})
    set_tests_properties(GroupDispatch.${case_name} PROPERTIES TIMEOUT 10)
endforeach()
