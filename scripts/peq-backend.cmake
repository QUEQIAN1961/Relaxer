# The output-processing hook must match the exact installed private Qt ABI.
if(WIN32)
  list(APPEND CMAKE_MODULE_PATH "${Qt6_DIR}")
  find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)
  find_package(Qt6 6.11.2 EXACT REQUIRED COMPONENTS CorePrivate MultimediaPrivate FFmpegMediaPluginImplPrivate)
  add_library(relaxer_peq_media_plugin MODULE
    src/audio/qt/qffmpegplugin.cpp src/audio/qt/qffmpegaudiorenderer.cpp src/build-identity.cpp)
  get_filename_component(peq_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/peq-runtime.json" peq_abi)
  string(JSON peq_expected_archive GET "${peq_abi}" installedArchive sha256)
  string(JSON peq_expected_header GET "${peq_abi}" installedRendererHeader sha256)
  file(SHA256 "${peq_qt_prefix}/lib/libQt6FFmpegMediaPluginImpl.a" peq_actual_archive)
  file(SHA256 "${peq_qt_prefix}/include/qt6/QtFFmpegMediaPluginImpl/6.11.2/QtFFmpegMediaPluginImpl/private/qffmpegaudiorenderer_p.h" peq_actual_header)
  if(NOT peq_actual_archive STREQUAL peq_expected_archive OR NOT peq_actual_header STREQUAL peq_expected_header)
    message(FATAL_ERROR "The PEQ audio hook requires the recorded Qt 6.11.2 private ABI; review toolchain changes before rebuilding")
  endif()
  target_include_directories(relaxer_peq_media_plugin PRIVATE src "${peq_qt_prefix}/include/qt6/QtFFmpegMediaPluginImpl/6.11.2/QtFFmpegMediaPluginImpl/private")
  target_link_libraries(relaxer_peq_media_plugin PRIVATE Qt6::FFmpegMediaPluginImplPrivate relaxer_audio_dsp)
  set_target_properties(relaxer_peq_media_plugin PROPERTIES PREFIX "" OUTPUT_NAME "ffmpegmediaplugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/multimedia" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/multimedia")
  add_custom_command(TARGET relaxer_peq_media_plugin POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DPEQ_PLUGIN=$<TARGET_FILE:relaxer_peq_media_plugin>" "-DPEQ_DSP=$<TARGET_FILE:relaxer_audio_dsp>" "-DPEQ_OUTPUT=${CMAKE_BINARY_DIR}/runtime-peq.json" "-DPEQ_SOURCE=${RELAXER_SOURCE_SHA256}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/record-peq-runtime.cmake" VERBATIM)
  add_dependencies(Relaxer relaxer_peq_media_plugin)
endif()
