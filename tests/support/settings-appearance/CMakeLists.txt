cmake_minimum_required(VERSION 3.22)
project(RelaxerB3SettingsAppearanceBaseline LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network Sql Multimedia Svg Test)
if(NOT RELAXER_BASELINE_SOURCE OR NOT RELAXER_BASELINE_BUILD)
  message(FATAL_ERROR "Explicit accepted B3 source and build are required")
endif()
foreach(kind IN ITEMS settings-appearance settings-appearance-widgets)
  add_executable(relaxer_baseline_${kind} ../../${kind}-tests.cpp)
  target_include_directories(relaxer_baseline_${kind} PRIVATE "${RELAXER_BASELINE_SOURCE}/src")
  target_link_libraries(relaxer_baseline_${kind} PRIVATE
    "${RELAXER_BASELINE_BUILD}/librelaxer_ui.a"
    "${RELAXER_BASELINE_BUILD}/librelaxer_core.a"
    "${RELAXER_BASELINE_BUILD}/librelaxer_services.a"
    "${RELAXER_BASELINE_BUILD}/librelaxer_domain.a"
    Qt6::Widgets Qt6::Svg Qt6::Multimedia Qt6::Sql Qt6::Network Qt6::Gui Qt6::Core Qt6::Test
    winmm user32 dwmapi)
endforeach()
