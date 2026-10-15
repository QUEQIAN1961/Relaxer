if(NOT DEFINED RELAXER_TEST_EXECUTABLE OR NOT DEFINED RELAXER_OBJDUMP)
  message(FATAL_ERROR "The service test executable and objdump are required")
endif()

execute_process(
  COMMAND "${RELAXER_OBJDUMP}" -p "${RELAXER_TEST_EXECUTABLE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE imports
  ERROR_VARIABLE error)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Could not inspect service test imports: ${error}")
endif()
if(NOT imports MATCHES "DLL Name: Qt6Network[.]dll")
  message(FATAL_ERROR "The isolated service executable does not import Qt6Network.dll")
endif()
if(imports MATCHES "DLL Name: Qt6Widgets[.]dll")
  message(FATAL_ERROR "The isolated service executable unexpectedly imports Qt6Widgets.dll")
endif()
message(STATUS "Service link audit passed: Qt6Network.dll is present and Qt6Widgets.dll is absent")
