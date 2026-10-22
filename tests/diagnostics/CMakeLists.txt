cmake_minimum_required(VERSION 3.20)
project(NeuralDiagnosticPathTests LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "These tests exercise the Windows environment API.")
endif()

add_executable(test_diagnostic_path test_diagnostic_path.cpp)
target_compile_features(test_diagnostic_path PRIVATE cxx_std_17)
target_include_directories(test_diagnostic_path PRIVATE ../../src/layer)
if(MSVC)
    target_compile_options(test_diagnostic_path PRIVATE /W4 /WX /permissive- /EHsc)
endif()

enable_testing()
foreach(case_name IN ITEMS Unset Empty Short MaximumFit ExactCapacity OneOverCapacity LargeValue StaleAfterLong)
    add_test(NAME DiagnosticPath.${case_name} COMMAND test_diagnostic_path ${case_name})
    set_tests_properties(DiagnosticPath.${case_name} PROPERTIES TIMEOUT 5)
endforeach()
