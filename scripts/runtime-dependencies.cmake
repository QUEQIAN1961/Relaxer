cmake_minimum_required(VERSION 3.22)
if(NOT CMAKE_HOST_WIN32)
  message(FATAL_ERROR "Runtime collection requires native Windows CMake")
endif()
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM windows+pe)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL objdump)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${OBJDUMP}")
function(collect output)
  file(GET_RUNTIME_DEPENDENCIES ${ARGN}
    RESOLVED_DEPENDENCIES_VAR resolved UNRESOLVED_DEPENDENCIES_VAR unresolved
    DIRECTORIES "${output}" "${UCRT_BIN}"
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    # Windows discovery can return mixed separators such as
    # C:\\WINDOWS\\system32/secur32.dll. Match both kinds of separators.
    POST_EXCLUDE_REGEXES "[/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\]" "[/\\\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\\\]")
  if(unresolved)
    message(FATAL_ERROR "Unresolved runtime dependencies: ${unresolved}")
  endif()
  foreach(dll IN LISTS resolved)
    file(TO_CMAKE_PATH "${dll}" source_dll)
    file(COPY "${source_dll}" DESTINATION "${output}")
  endforeach()
endfunction()
file(GLOB_RECURSE plugins "${DEST}/*.dll")
# Helpers have their own DLL closure; do not mix equal DLL names from bin/ into the application scan.
list(FILTER plugins EXCLUDE REGEX "/bin/")
collect("${DEST}" EXECUTABLES "${DEST}/Relaxer.exe" LIBRARIES ${plugins})
# Helpers load from bin/. Do not use Relaxer's root DLLs to satisfy their closure.
collect("${DEST}/bin" EXECUTABLES "${DEST}/bin/ffmpeg.exe" "${DEST}/bin/ffprobe.exe")
