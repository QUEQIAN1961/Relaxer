# ZIP timestamps may precede retained object files. A content-dependent compile
# definition changes Ninja's command hash even when source mtimes go backwards.
# The first-start script explicitly configures before each build.
function(relaxer_track_source_content source_root)
  file(GLOB_RECURSE source_files LIST_DIRECTORIES FALSE RELATIVE "${source_root}"
    "${source_root}/src/*.cpp" "${source_root}/src/*.h"
    "${source_root}/tests/*.cpp" "${source_root}/resources/*")
  list(APPEND source_files "CMakeLists.txt")
  list(SORT source_files)
  file(SHA256 "${CMAKE_CURRENT_FUNCTION_LIST_FILE}" policy_hash)
  set(source_identity "fingerprint-policy:${policy_hash}\n")
  foreach(relative_path IN LISTS source_files)
    file(SHA256 "${source_root}/${relative_path}" file_hash)
    string(APPEND source_identity "${relative_path}:${file_hash}\n")
  endforeach()
  string(SHA256 fingerprint "${source_identity}")
  # Applying this to every compiled target also refreshes MOC/RCC consumers and
  # the test executables; a failed build remains dirty for the next retry.
  add_compile_definitions(RELAXER_BUILD_SOURCE_SHA256="${fingerprint}")
  file(WRITE "${CMAKE_BINARY_DIR}/relaxer-source-fingerprint.txt" "${fingerprint}\n")
  message(STATUS "Relaxer source fingerprint: ${fingerprint}")
  set(RELAXER_SOURCE_SHA256 "${fingerprint}" PARENT_SCOPE)
endfunction()
