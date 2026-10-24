cmake_minimum_required(VERSION 3.20)
project(AbdFilterProbe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(ABDSHARED_CODE ${REPO}/../ABDShared/Code)

add_executable(abd_filter_harness
    abd_filter_harness.cpp
    ${REPO}/Source/DSP/Filters/MultiModeFilter.cpp
    ${REPO}/Source/DSP/Filters/FilterResonanceComp.cpp
)

target_include_directories(abd_filter_harness PRIVATE
    ${REPO}/Source
    ${REPO}/Source/Core
    ${REPO}/Source/DSP
    ${REPO}/Source/DSP/Common
    ${REPO}/Source/DSP/Filters
    ${ABDSHARED_CODE}
    ${ABDSHARED_CODE}/SynthCore
)
