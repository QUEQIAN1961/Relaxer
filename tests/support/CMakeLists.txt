cmake_minimum_required(VERSION 3.22)
project(RelaxerB2MatchOracle LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network Sql Multimedia Svg)
if(NOT RELAXER_BASELINE_SOURCE OR NOT RELAXER_BASELINE_BUILD)
  message(FATAL_ERROR "Explicit verified B2 source and build directories are required")
endif()
add_executable(relaxer_b2_match_oracle lyric-match-export.cpp)
target_include_directories(relaxer_b2_match_oracle PRIVATE "${RELAXER_BASELINE_SOURCE}/src")
target_link_libraries(relaxer_b2_match_oracle PRIVATE
  "${RELAXER_BASELINE_BUILD}/librelaxer_ui.a"
  "${RELAXER_BASELINE_BUILD}/librelaxer_core.a"
  "${RELAXER_BASELINE_BUILD}/librelaxer_services.a"
  "${RELAXER_BASELINE_BUILD}/librelaxer_domain.a"
  Qt6::Widgets Qt6::Svg Qt6::Multimedia Qt6::Sql Qt6::Network Qt6::Gui Qt6::Core
  winmm user32 dwmapi)
