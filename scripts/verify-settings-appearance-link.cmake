# Inspect the real executable, not just the includes in storage/appearance-store.cpp. GNU objdump
# supports both the MinGW PE import table and Linux ELF NEEDED entries.
if(NOT DEFINED RELAXER_TEST_EXECUTABLE OR NOT DEFINED RELAXER_OBJDUMP)
  message(FATAL_ERROR "The Settings/appearance test executable and objdump are required")
endif()

execute_process(
  COMMAND "${RELAXER_OBJDUMP}" -p "${RELAXER_TEST_EXECUTABLE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE imports
  ERROR_VARIABLE error)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Could not inspect Settings/appearance test dependencies: ${error}")
endif()
foreach(required Core Gui Sql Test)
  if(NOT imports MATCHES "Qt6${required}d?[.](dll|so)")
    message(FATAL_ERROR "The isolated Settings/appearance test does not import Qt6${required}: ${imports}")
  endif()
endforeach()
if(imports MATCHES "Qt6(Widgets|Svg[^ .\n]*|Quick[^ .\n]*|Qml[^ .\n]*)d?[.](dll|so)")
  message(FATAL_ERROR "The isolated Settings/appearance test imports a forbidden Qt module: ${imports}")
endif()
message(STATUS "Settings/appearance link audit passed: Qt Core/Gui/Sql/Test present; Widgets/Svg/Quick/Qml absent; inherited core Multimedia/Network allowed")
