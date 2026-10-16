# Check the actual standalone reader test executable's imports.
if(NOT DEFINED RELAXER_TEST_EXECUTABLE OR NOT DEFINED RELAXER_OBJDUMP)
  message(FATAL_ERROR "The lyric document test executable and objdump are required")
endif()

execute_process(
  COMMAND "${RELAXER_OBJDUMP}" -p "${RELAXER_TEST_EXECUTABLE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE imports
  ERROR_VARIABLE error)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Could not inspect lyric document test dependencies: ${error}")
endif()
foreach(required Core Test)
  if(NOT imports MATCHES "Qt6${required}d?[.](dll|so)")
    message(FATAL_ERROR "The independent lyric document test does not import Qt6${required}: ${imports}")
  endif()
endforeach()
if(imports MATCHES "Qt6(Gui|Widgets|Network|Sql|Multimedia[^ .\n]*|Svg[^ .\n]*|Quick[^ .\n]*|Qml[^ .\n]*|OpenGL[^ .\n]*)d?[.](dll|so)")
  message(FATAL_ERROR "The independent lyric document test imports a forbidden Qt module: ${imports}")
endif()
message(STATUS "Lyric document link audit passed: Qt Core/Test present; Gui/Widgets/Network/Sql/Multimedia/Svg/Quick/Qml/OpenGL absent")
