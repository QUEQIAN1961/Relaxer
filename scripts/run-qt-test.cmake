# File logging also works when a Windows GUI process has no usable stdout.
# Read the report back through CMake so CTest and the native launcher receive it.
if(NOT DEFINED RELAXER_TEST_EXECUTABLE OR NOT DEFINED RELAXER_TEST_LOG)
  message(FATAL_ERROR "Qt test executable and report path are required")
endif()
get_filename_component(report_directory "${RELAXER_TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${report_directory}")
# Evidence roots are a caller/test-runner contract. Some fixtures intentionally
# require an existing directory and treat write errors as hard failures. Prepare
# supported roots before launching any test, including a new build's first run.
# MAKE_DIRECTORY remains fatal on errors; do not hide an unwritable/file path.
foreach(evidence_variable IN ITEMS
    RELAXER_C6_EVIDENCE RELAXER_P2_EVIDENCE RELAXER_INTEGRATION_EVIDENCE
    RELAXER_APPEARANCE_EVIDENCE RELAXER_EXIT_GUARD_EVIDENCE
    RELAXER_D203_EVIDENCE RELAXER_D204_EVIDENCE RELAXER_UX13_EVIDENCE
    RELAXER_D2_ESCAPE_EVIDENCE RELAXER_D2_EVIDENCE)
  if(DEFINED ENV{${evidence_variable}} AND NOT "$ENV{${evidence_variable}}" STREQUAL "")
    get_filename_component(evidence_directory "$ENV{${evidence_variable}}" ABSOLUTE
      BASE_DIR "${report_directory}")
    file(MAKE_DIRECTORY "${evidence_directory}")
    if(NOT IS_DIRECTORY "${evidence_directory}")
      message(FATAL_ERROR "Required evidence root is not a directory: ${evidence_variable}=${evidence_directory}")
    endif()
    set(ENV{${evidence_variable}} "${evidence_directory}")
    message("QtTest evidence root: ${evidence_variable}=${evidence_directory}")
  endif()
endforeach()
file(REMOVE "${RELAXER_TEST_LOG}")
get_filename_component(report_name "${RELAXER_TEST_LOG}" NAME)
message("QtTest starting: executable=${RELAXER_TEST_EXECUTABLE}; platform=$ENV{QT_QPA_PLATFORM}; pluginPath=$ENV{QT_PLUGIN_PATH}; qpaPluginPath=$ENV{QT_QPA_PLATFORM_PLUGIN_PATH}")
execute_process(
  COMMAND "${RELAXER_TEST_EXECUTABLE}" -v1 -o "${report_name},txt" -o "-,txt"
  WORKING_DIRECTORY "${report_directory}"
  RESULT_VARIABLE test_result)
set(test_report "")
if(EXISTS "${RELAXER_TEST_LOG}")
  file(READ "${RELAXER_TEST_LOG}" test_report)
  message("QtTest report: ${RELAXER_TEST_LOG}\n${test_report}")
else()
  message("QtTest did not create its report: ${RELAXER_TEST_LOG}")
endif()
message("QtTest process result: ${test_result}")
if(NOT test_result STREQUAL "0")
  message(FATAL_ERROR "Qt test failed; original process result=${test_result}")
endif()
if(test_report STREQUAL "")
  message(FATAL_ERROR "Qt test returned success without the required report")
endif()
