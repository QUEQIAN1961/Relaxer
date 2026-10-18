cmake_minimum_required(VERSION 3.22)
project(Relaxer VERSION 0.2.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
include(scripts/source-fingerprint.cmake)
relaxer_track_source_content("${CMAKE_CURRENT_SOURCE_DIR}")
option(RELAXER_PLAYBACK_FACADE_ONLY "Build playback facade without Widgets" OFF)
option(RELAXER_LYRICS_SEARCH_ONLY "Build lyrics search service without Widgets" OFF)
option(RELAXER_TRANSLATION_ONLY "Build translation business service without Widgets" OFF)
option(RELAXER_IMPORT_ONLY "Build ImportCoordinator and real storage tests without Widgets" OFF)
option(RELAXER_QUEUE_ONLY "Build only the independent Queue and its tests" OFF)
option(RELAXER_LYRIC_MATCH_ONLY "Build only the independent lyric matching tests" OFF)
option(RELAXER_STORAGE_ONLY "Build storage/appearance tests without Widgets" OFF)
option(RELAXER_LYRICS_DOCUMENT_ONLY "Build only independent lyric document tests" OFF)
if((RELAXER_PLAYBACK_FACADE_ONLY AND (RELAXER_LYRICS_SEARCH_ONLY OR RELAXER_TRANSLATION_ONLY OR RELAXER_IMPORT_ONLY OR RELAXER_STORAGE_ONLY OR RELAXER_QUEUE_ONLY OR RELAXER_LYRIC_MATCH_ONLY OR RELAXER_LYRICS_DOCUMENT_ONLY))
   OR (RELAXER_LYRICS_SEARCH_ONLY AND (RELAXER_TRANSLATION_ONLY OR RELAXER_IMPORT_ONLY OR RELAXER_STORAGE_ONLY OR RELAXER_QUEUE_ONLY OR RELAXER_LYRIC_MATCH_ONLY OR RELAXER_LYRICS_DOCUMENT_ONLY))
   OR (RELAXER_TRANSLATION_ONLY AND (RELAXER_IMPORT_ONLY OR RELAXER_STORAGE_ONLY OR RELAXER_QUEUE_ONLY OR RELAXER_LYRIC_MATCH_ONLY OR RELAXER_LYRICS_DOCUMENT_ONLY))
   OR (RELAXER_IMPORT_ONLY AND (RELAXER_STORAGE_ONLY OR RELAXER_QUEUE_ONLY OR RELAXER_LYRIC_MATCH_ONLY OR RELAXER_LYRICS_DOCUMENT_ONLY))
   OR (RELAXER_STORAGE_ONLY AND (RELAXER_QUEUE_ONLY OR RELAXER_LYRIC_MATCH_ONLY OR RELAXER_LYRICS_DOCUMENT_ONLY))
   OR (RELAXER_QUEUE_ONLY AND (RELAXER_LYRIC_MATCH_ONLY OR RELAXER_LYRICS_DOCUMENT_ONLY))
   OR (RELAXER_LYRIC_MATCH_ONLY AND RELAXER_LYRICS_DOCUMENT_ONLY))
  message(FATAL_ERROR "Select only one independent test configuration")
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Test)
add_library(relaxer_domain STATIC src/domain/queue.cpp src/domain/lyric-match.cpp)
# Queue is a value object; it owns no QObject meta-object or resources.
set_target_properties(relaxer_domain PROPERTIES AUTOMOC OFF AUTORCC OFF)
target_include_directories(relaxer_domain PUBLIC src)
target_link_libraries(relaxer_domain PUBLIC Qt6::Core)
if(MSVC)
  target_compile_options(relaxer_domain PRIVATE /utf-8 /W4)
else()
  target_compile_options(relaxer_domain PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
function(relaxer_add_qt_test test_name target)
  add_test(NAME ${test_name} COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:${target}>"
    "-DRELAXER_TEST_LOG=${CMAKE_CURRENT_BINARY_DIR}/Testing/QtResults/${test_name}.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-qt-test.cmake")
  # The first-start suite is deterministic/headless; physical audio playback
  # remains available for a separate explicit hardware-enabled test run.
  set_tests_properties(${test_name} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_MEDIA_BACKEND=ffmpeg;RELAXER_SKIP_AUDIO_DEVICE_TESTS=1" TIMEOUT 120)
endfunction()
if(NOT RELAXER_LYRIC_MATCH_ONLY AND NOT RELAXER_STORAGE_ONLY AND NOT RELAXER_LYRICS_DOCUMENT_ONLY)
add_executable(relaxer_queue_tests tests/queue-tests.cpp)
target_link_libraries(relaxer_queue_tests PRIVATE relaxer_domain Qt6::Test)
relaxer_add_qt_test(relaxer_queue relaxer_queue_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_custom_command(TARGET relaxer_queue_tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_queue_tests>"
      "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-queue-link.cmake"
    VERBATIM)
  add_test(NAME relaxer_queue_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_queue_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-queue-link.cmake")
  set_tests_properties(relaxer_queue_link PROPERTIES TIMEOUT 10)
else()
  message(WARNING "Queue binary import audit requires objdump on Windows or Linux")
endif()
endif()
# This opt-in configuration proves that neither the GUI nor the media/storage
# packages are needed to build and test Queue. The default builds every suite.
if(RELAXER_QUEUE_ONLY)
  return()
endif()
if(NOT RELAXER_STORAGE_ONLY AND NOT RELAXER_LYRICS_DOCUMENT_ONLY)
# The parser is compiled only into this standalone test as an unchanged fixture
# adapter. relaxer_domain itself never calls it and has no core-library edge.
add_executable(relaxer_lyric_match_tests tests/lyric-match-tests.cpp src/lyrics.cpp)
target_link_libraries(relaxer_lyric_match_tests PRIVATE relaxer_domain Qt6::Test)
target_compile_definitions(relaxer_lyric_match_tests PRIVATE RELAXER_LYRIC_MATCH_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/lyric-match-baseline.json")
if(MSVC)
  target_compile_options(relaxer_lyric_match_tests PRIVATE /utf-8)
endif()
relaxer_add_qt_test(relaxer_lyric_match relaxer_lyric_match_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_custom_command(TARGET relaxer_lyric_match_tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_lyric_match_tests>"
      "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-lyric-match-link.cmake"
    VERBATIM)
  add_test(NAME relaxer_lyric_match_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_lyric_match_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-lyric-match-link.cmake")
  set_tests_properties(relaxer_lyric_match_link PROPERTIES TIMEOUT 10)
endif()
if(RELAXER_LYRIC_MATCH_ONLY)
  return()
endif()
endif()
# Freeze B4's local lyric parser and file-read behavior before extraction.
add_executable(relaxer_lyrics_document_baseline_tests
  tests/lyrics-document-baseline-tests.cpp src/lyrics.cpp)
target_include_directories(relaxer_lyrics_document_baseline_tests PRIVATE src)
target_link_libraries(relaxer_lyrics_document_baseline_tests PRIVATE Qt6::Core Qt6::Test)
relaxer_add_qt_test(relaxer_lyrics_document_baseline relaxer_lyrics_document_baseline_tests)

add_executable(relaxer_lyrics_document_tests
  tests/lyrics-document-reader-tests.cpp src/lyrics.cpp src/lyrics-document-reader.cpp)
target_include_directories(relaxer_lyrics_document_tests PRIVATE src)
target_link_libraries(relaxer_lyrics_document_tests PRIVATE Qt6::Core Qt6::Test)
relaxer_add_qt_test(relaxer_lyrics_document relaxer_lyrics_document_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_custom_command(TARGET relaxer_lyrics_document_tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_lyrics_document_tests>"
      "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-lyrics-document-link.cmake"
    VERBATIM)
  add_test(NAME relaxer_lyrics_document_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_lyrics_document_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-lyrics-document-link.cmake")
  set_tests_properties(relaxer_lyrics_document_link PROPERTIES TIMEOUT 10)
endif()
if(RELAXER_LYRICS_DOCUMENT_ONLY)
  return()
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Sql Multimedia Test)
add_library(relaxer_core src/library.cpp src/storage/settings-store.cpp src/storage/appearance-store.cpp src/appearance-image.cpp src/importer.cpp src/import-coordinator.cpp src/lyrics.cpp src/lyrics-document-reader.cpp src/lyric-text.cpp src/playback.cpp src/midi-transport.cpp)
target_include_directories(relaxer_core PUBLIC src)
target_link_libraries(relaxer_core PUBLIC relaxer_domain Qt6::Core Qt6::Gui Qt6::Sql Qt6::Multimedia)
if(WIN32)
  target_link_libraries(relaxer_core PUBLIC winmm)
endif()
if(MSVC)
  target_compile_options(relaxer_core PUBLIC /utf-8 /W4)
else()
  target_compile_options(relaxer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(RELAXER_PLAYBACK_FACADE_ONLY OR NOT (RELAXER_IMPORT_ONLY OR RELAXER_TRANSLATION_ONLY OR RELAXER_LYRICS_SEARCH_ONLY OR RELAXER_STORAGE_ONLY))
add_library(relaxer_playback_facade STATIC src/playback-facade.cpp src/playback-facade.h)
target_include_directories(relaxer_playback_facade PUBLIC src)
target_link_libraries(relaxer_playback_facade PUBLIC relaxer_core)
add_executable(relaxer_playback_facade_tests tests/playback-facade-tests.cpp)
target_link_libraries(relaxer_playback_facade_tests PRIVATE relaxer_playback_facade Qt6::Test)
relaxer_add_qt_test(relaxer_playback_facade relaxer_playback_facade_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_custom_command(TARGET relaxer_playback_facade_tests POST_BUILD COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_playback_facade_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}" -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-playback-facade-link.cmake" VERBATIM)
  add_test(NAME relaxer_playback_facade_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_playback_facade_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}" -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-playback-facade-link.cmake")
endif()
if(RELAXER_PLAYBACK_FACADE_ONLY)
  return()
endif()
endif()
find_package(SQLite3 REQUIRED)
add_executable(relaxer_lyrics_persistence_tests tests/lyrics-persistence-tests.cpp)
target_link_libraries(relaxer_lyrics_persistence_tests PRIVATE relaxer_core Qt6::Test SQLite::SQLite3)
relaxer_add_qt_test(relaxer_lyrics_persistence relaxer_lyrics_persistence_tests)

# Freeze the actual importer, file and Library behavior before B6 extraction.
add_executable(relaxer_importer_contract_tests tests/importer-contract-tests.cpp)
target_link_libraries(relaxer_importer_contract_tests PRIVATE relaxer_core Qt6::Test)
relaxer_add_qt_test(relaxer_importer_contract relaxer_importer_contract_tests)

add_executable(relaxer_import_process_fixture tests/import-process-fixture.cpp)
set_target_properties(relaxer_import_process_fixture PROPERTIES AUTOMOC OFF AUTORCC OFF)
add_executable(relaxer_import_coordinator_tests tests/import-coordinator-tests.cpp)
add_dependencies(relaxer_import_coordinator_tests relaxer_import_process_fixture)
target_link_libraries(relaxer_import_coordinator_tests PRIVATE relaxer_core Qt6::Test SQLite::SQLite3)
relaxer_add_qt_test(relaxer_import_coordinator relaxer_import_coordinator_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_test(NAME relaxer_import_coordinator_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_import_coordinator_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-import-coordinator-link.cmake")
endif()
if(RELAXER_IMPORT_ONLY)
  return()
endif()

add_executable(relaxer_lyrics_document_compat_tests tests/lyrics-document-compat-tests.cpp)
target_include_directories(relaxer_lyrics_document_compat_tests PRIVATE src)
target_link_libraries(relaxer_lyrics_document_compat_tests PRIVATE relaxer_core Qt6::Test)
relaxer_add_qt_test(relaxer_lyrics_document_compat relaxer_lyrics_document_compat_tests)

# Storage lives in the existing core to reuse the one Library implementation.
# Gui is required by QColor/QImage; Multimedia remains core's existing public
# dependency. Neither these sources nor this independent configuration use Widgets.
add_executable(relaxer_settings_appearance_tests tests/settings-appearance-tests.cpp)
target_compile_definitions(relaxer_settings_appearance_tests PRIVATE RELAXER_SETTINGS_FACADE=1)
target_link_libraries(relaxer_settings_appearance_tests PRIVATE relaxer_core Qt6::Test)
relaxer_add_qt_test(relaxer_settings_appearance relaxer_settings_appearance_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_custom_command(TARGET relaxer_settings_appearance_tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_settings_appearance_tests>"
      "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-settings-appearance-link.cmake"
    VERBATIM)
  add_test(NAME relaxer_settings_appearance_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_settings_appearance_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-settings-appearance-link.cmake")
  set_tests_properties(relaxer_settings_appearance_link PROPERTIES TIMEOUT 10)
endif()
if(RELAXER_STORAGE_ONLY)
  return()
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS Network)
add_library(relaxer_ai STATIC
  src/ai/ai-model.cpp
  src/ai/ai-request-coordinator.cpp
  src/ai/ai-runtime.cpp
  src/ai/whole-song-lyrics-translation.cpp
  src/ai/whole-song-lyrics-translation.h
  src/ai/ai-provider.h
  src/ai/credential-store.cpp
  src/ai/deepseek-provider.cpp)
target_include_directories(relaxer_ai PUBLIC src)
target_link_libraries(relaxer_ai PUBLIC relaxer_core Qt6::Core Qt6::Network)
if(WIN32)
  target_link_libraries(relaxer_ai PUBLIC advapi32)
endif()
if(MSVC)
  target_compile_options(relaxer_ai PRIVATE /utf-8 /W4)
else()
  target_compile_options(relaxer_ai PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(relaxer_ai_tests tests/ai-infrastructure-tests.cpp)
target_link_libraries(relaxer_ai_tests PRIVATE relaxer_ai Qt6::Test)
relaxer_add_qt_test(relaxer_ai_infrastructure relaxer_ai_tests)
add_library(relaxer_services STATIC src/browser-translation.cpp src/lyric-remote.cpp)
target_include_directories(relaxer_services PUBLIC src)
target_link_libraries(relaxer_services PUBLIC Qt6::Core Qt6::Network)
if(MSVC)
  target_compile_options(relaxer_services PRIVATE /utf-8 /W4)
else()
  target_compile_options(relaxer_services PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_library(relaxer_translation STATIC src/translation-coordinator.cpp src/translation-browser-backend.cpp)
target_include_directories(relaxer_translation PUBLIC src)
target_link_libraries(relaxer_translation PUBLIC relaxer_ai relaxer_services)
if(MSVC)
  target_compile_options(relaxer_translation PRIVATE /utf-8 /W4)
else()
  target_compile_options(relaxer_translation PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(relaxer_translation_coordinator_tests tests/translation-coordinator-tests.cpp)
target_link_libraries(relaxer_translation_coordinator_tests PRIVATE relaxer_translation Qt6::Test)
relaxer_add_qt_test(relaxer_translation_coordinator relaxer_translation_coordinator_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_test(NAME relaxer_translation_coordinator_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_translation_coordinator_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-translation-link.cmake")
endif()
if(RELAXER_TRANSLATION_ONLY)
  return()
endif()
add_library(relaxer_lyrics_search STATIC src/lyrics-search-coordinator.cpp)
target_include_directories(relaxer_lyrics_search PUBLIC src)
target_link_libraries(relaxer_lyrics_search PUBLIC relaxer_core relaxer_services)
if(MSVC)
  target_compile_options(relaxer_lyrics_search PRIVATE /utf-8 /W4)
else()
  target_compile_options(relaxer_lyrics_search PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(relaxer_lyrics_search_coordinator_tests tests/lyrics-search-coordinator-tests.cpp)
target_link_libraries(relaxer_lyrics_search_coordinator_tests PRIVATE relaxer_lyrics_search Qt6::Test SQLite::SQLite3)
relaxer_add_qt_test(relaxer_lyrics_search_coordinator relaxer_lyrics_search_coordinator_tests)
if(CMAKE_OBJDUMP AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_test(NAME relaxer_lyrics_search_coordinator_link COMMAND "${CMAKE_COMMAND}"
    "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_lyrics_search_coordinator_tests>"
    "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-lyrics-search-link.cmake")
endif()
if(RELAXER_LYRICS_SEARCH_ONLY)
  return()
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Svg Network)
add_executable(relaxer_translation_classification_storage_tests tests/translation-classification-storage-tests.cpp)
target_link_libraries(relaxer_translation_classification_storage_tests PRIVATE relaxer_core Qt6::Test)
relaxer_add_qt_test(relaxer_translation_classification_storage relaxer_translation_classification_storage_tests)
if(WIN32)
  add_executable(relaxer_midi_tests tests/midi-tests.cpp)
  target_link_libraries(relaxer_midi_tests PRIVATE relaxer_core Qt6::Test)
  relaxer_add_qt_test(relaxer_midi relaxer_midi_tests)
  add_executable(relaxer_midi_runtime_probe tests/midi-runtime-probe.cpp)
  target_link_libraries(relaxer_midi_runtime_probe PRIVATE relaxer_playback_facade)
endif()
set(RELAXER_UI src/ai/ai-settings-page.cpp src/ai/ai-settings-page.h src/chrome.cpp src/playlist-ui.cpp src/motion.cpp src/nextqueue.cpp src/v14widgets.cpp src/lyricslookup.cpp src/lyric-tools.cpp src/smooth-scroll.cpp src/window.cpp src/widgets.cpp src/appearance.cpp src/shortcuts.cpp src/changelog.cpp resources/resources.qrc)
# The application and GUI tests share one compiled UI library. Resources are
# explicitly initialized from window.cpp so static linking retains the RCC unit.
add_library(relaxer_ui STATIC ${RELAXER_UI})
target_include_directories(relaxer_ui PUBLIC src)
target_link_libraries(relaxer_ui PUBLIC relaxer_playback_facade relaxer_lyrics_search relaxer_translation relaxer_ai relaxer_core relaxer_services Qt6::Widgets Qt6::Svg Qt6::Network)
add_executable(relaxer_translation_workflow_tests tests/translation-workflow-tests.cpp)
target_link_libraries(relaxer_translation_workflow_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_translation_workflow relaxer_translation_workflow_tests)
add_executable(relaxer_lyrics_workflow_preview tests/lyrics-workflow-preview.cpp)
target_link_libraries(relaxer_lyrics_workflow_preview PRIVATE relaxer_ui Qt6::Test)
# Phase B integration exercises production services together; service-only
# configurations above retain their independent dependency boundaries/counts.
add_executable(relaxer_phase_b_integration_tests tests/phase-b-integration-tests.cpp)
target_link_libraries(relaxer_phase_b_integration_tests PRIVATE relaxer_ui Qt6::Test)
if(WIN32)
  target_link_libraries(relaxer_phase_b_integration_tests PRIVATE psapi)
endif()
relaxer_add_qt_test(relaxer_phase_b_integration relaxer_phase_b_integration_tests)
set_tests_properties(relaxer_phase_b_integration PROPERTIES TIMEOUT 240)
add_executable(relaxer_phase_b_storage_tests tests/phase-b-storage-tests.cpp)
target_link_libraries(relaxer_phase_b_storage_tests PRIVATE relaxer_ui Qt6::Test)
target_compile_definitions(relaxer_phase_b_storage_tests PRIVATE RELAXER_LEGACY_SQL_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/phase-b-legacy-sql.json")
relaxer_add_qt_test(relaxer_phase_b_storage relaxer_phase_b_storage_tests)
add_executable(relaxer_translation_filter_performance_tests tests/translation-filter-performance-tests.cpp)
target_link_libraries(relaxer_translation_filter_performance_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_translation_filter_performance relaxer_translation_filter_performance_tests)
set_tests_properties(relaxer_translation_filter_performance PROPERTIES TIMEOUT 180)
add_executable(relaxer_ai_lyrics_translation_tests tests/ai-lyrics-translation-tests.cpp)
target_link_libraries(relaxer_ai_lyrics_translation_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_ai_lyrics_translation relaxer_ai_lyrics_translation_tests)
if(WIN32)
  target_link_libraries(relaxer_ui PUBLIC user32 dwmapi)
endif()
qt_add_executable(Relaxer WIN32 src/main.cpp)
target_link_libraries(Relaxer PRIVATE relaxer_ui)
# Service-only configurations return above this point, before any Quick lookup.
option(RELAXER_ENABLE_QML "Build the optional Phase C QML shell" ON)
if(RELAXER_ENABLE_QML)
  find_package(Qt6 6.4 REQUIRED COMPONENTS Qml Quick QuickControls2)
  include(scripts/qml-test-environment.cmake)
  relaxer_configure_qml_tests()
  add_library(relaxer_qml STATIC
    src/ui-bridge/app-bridge.cpp src/ui-bridge/app-bridge.h
    src/ui-bridge/playback-bridge.cpp src/ui-bridge/playback-bridge.h
    src/ui-bridge/qml-shell.cpp src/ui-bridge/qml-shell.h
    src/qml/qml.qrc)
  target_include_directories(relaxer_qml PUBLIC src)
  target_link_libraries(relaxer_qml PUBLIC relaxer_playback_facade Qt6::Qml Qt6::Quick Qt6::QuickControls2)
  target_link_libraries(Relaxer PRIVATE relaxer_qml)
  target_compile_definitions(Relaxer PRIVATE RELAXER_ENABLE_QML=1)
  add_executable(relaxer_qml_tests tests/qml-foundation-tests.cpp)
  target_link_libraries(relaxer_qml_tests PRIVATE relaxer_qml Qt6::Test)
  add_dependencies(relaxer_qml_tests Relaxer)
  target_compile_definitions(relaxer_qml_tests PRIVATE RELAXER_APP_PATH="$<TARGET_FILE:Relaxer>")
  relaxer_add_qt_test(relaxer_qml_foundation relaxer_qml_tests)
  relaxer_qml_test_environment(relaxer_qml_foundation)
  add_executable(relaxer_ui_navigation_probe EXCLUDE_FROM_ALL tests/ui-navigation-probe.cpp)
  target_link_libraries(relaxer_ui_navigation_probe PRIVATE relaxer_qml relaxer_ui)
endif()
if(WIN32)
  target_sources(Relaxer PRIVATE resources/phase-c-relaxer.rc)
  target_include_directories(Relaxer PRIVATE resources)
  add_executable(RelaxerStart WIN32 src/start-windows.cpp resources/phase-c-start.rc)
  set_target_properties(RelaxerStart PROPERTIES AUTOMOC OFF)
  target_include_directories(RelaxerStart PRIVATE resources)
  target_link_libraries(RelaxerStart PRIVATE user32 shell32 ole32 comctl32 gdi32 dwmapi uxtheme advapi32)
  add_executable(RelaxerLockCheck src/pacman-lock-windows.cpp)
  set_target_properties(RelaxerLockCheck PROPERTIES AUTOMOC OFF AUTORCC OFF)
  if(MINGW)
    target_link_options(RelaxerStart PRIVATE -municode -static)
    target_link_options(RelaxerLockCheck PRIVATE -municode -static)
  elseif(MSVC)
    target_compile_options(RelaxerStart PRIVATE /utf-8)
    target_compile_options(RelaxerLockCheck PRIVATE /utf-8)
  endif()
endif()
add_executable(relaxer_core_update_tests tests/core-update-tests.cpp)
set_target_properties(relaxer_core_update_tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
target_include_directories(relaxer_core_update_tests PRIVATE src)
if(MINGW)
  target_link_options(relaxer_core_update_tests PRIVATE -static)
endif()
add_test(NAME relaxer_core_update COMMAND relaxer_core_update_tests)
set_tests_properties(relaxer_core_update PROPERTIES TIMEOUT 10)
add_executable(relaxer_readonly_lock_tests tests/readonly-lock-recovery-tests.cpp)
set_target_properties(relaxer_readonly_lock_tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
target_include_directories(relaxer_readonly_lock_tests PRIVATE src)
if(MINGW)
  target_link_options(relaxer_readonly_lock_tests PRIVATE -static)
endif()
add_test(NAME relaxer_readonly_lock COMMAND relaxer_readonly_lock_tests)
set_tests_properties(relaxer_readonly_lock PROPERTIES TIMEOUT 10)
if(WIN32)
  add_executable(relaxer_client_frame_tests tests/client-frame-windows-tests.cpp)
  set_target_properties(relaxer_client_frame_tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
  target_include_directories(relaxer_client_frame_tests PRIVATE src)
  target_link_libraries(relaxer_client_frame_tests PRIVATE user32 dwmapi)
  if(MINGW)
    target_link_options(relaxer_client_frame_tests PRIVATE -municode -static)
  elseif(MSVC)
    target_compile_options(relaxer_client_frame_tests PRIVATE /utf-8)
  endif()
  add_test(NAME relaxer_client_frame COMMAND relaxer_client_frame_tests)
  set_tests_properties(relaxer_client_frame PROPERTIES TIMEOUT 20)
  add_executable(relaxer_launcher_env_tests tests/launcher-environment-windows-tests.cpp)
  set_target_properties(relaxer_launcher_env_tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
  target_link_libraries(relaxer_launcher_env_tests PRIVATE user32 shell32 ole32 comctl32 gdi32 dwmapi uxtheme advapi32)
  if(MINGW)
    target_link_options(relaxer_launcher_env_tests PRIVATE -municode -static)
  elseif(MSVC)
    target_compile_options(relaxer_launcher_env_tests PRIVATE /utf-8)
  endif()
  add_test(NAME relaxer_launcher_env COMMAND relaxer_launcher_env_tests)
  set_tests_properties(relaxer_launcher_env PROPERTIES TIMEOUT 30)
  add_executable(relaxer_prepare_tests tests/prepare-progress-windows-tests.cpp)
  set_target_properties(relaxer_prepare_tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
  target_include_directories(relaxer_prepare_tests PRIVATE src)
  target_link_libraries(relaxer_prepare_tests PRIVATE user32 shell32 ole32 comctl32 gdi32 dwmapi uxtheme advapi32 gdiplus)
  if(MINGW)
    target_link_options(relaxer_prepare_tests PRIVATE -static)
  endif()
  add_test(NAME relaxer_prepare COMMAND relaxer_prepare_tests)
  set_tests_properties(relaxer_prepare PROPERTIES TIMEOUT 20)
  add_executable(relaxer_pacman_lock_tests tests/pacman-lock-windows-tests.cpp)
  set_target_properties(relaxer_pacman_lock_tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
  target_include_directories(relaxer_pacman_lock_tests PRIVATE src)
  if(MINGW)
    target_link_options(relaxer_pacman_lock_tests PRIVATE -municode -static)
  elseif(MSVC)
    target_compile_options(relaxer_pacman_lock_tests PRIVATE /utf-8)
  endif()
  add_test(NAME relaxer_pacman_lock COMMAND relaxer_pacman_lock_tests)
  set_tests_properties(relaxer_pacman_lock PROPERTIES TIMEOUT 90)
endif()
add_executable(relaxer_tests tests/tests.cpp)
target_link_libraries(relaxer_tests PRIVATE relaxer_core Qt6::Test)
relaxer_add_qt_test(relaxer_core relaxer_tests)
add_executable(relaxer_queue_playback_tests tests/queue-playback-tests.cpp)
target_link_libraries(relaxer_queue_playback_tests PRIVATE relaxer_core Qt6::Test)
relaxer_add_qt_test(relaxer_queue_playback relaxer_queue_playback_tests)
add_executable(relaxer_services_tests tests/services-tests.cpp)
target_link_libraries(relaxer_services_tests PRIVATE relaxer_services Qt6::Test)
relaxer_add_qt_test(relaxer_services relaxer_services_tests)
if(WIN32 AND CMAKE_OBJDUMP)
  add_custom_command(TARGET relaxer_services_tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DRELAXER_TEST_EXECUTABLE=$<TARGET_FILE:relaxer_services_tests>"
      "-DRELAXER_OBJDUMP=${CMAKE_OBJDUMP}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-services-link.cmake"
    VERBATIM)
endif()
add_executable(relaxer_import_widgets_tests tests/import-widgets-tests.cpp)
target_link_libraries(relaxer_import_widgets_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_import_widgets relaxer_import_widgets_tests)
add_executable(relaxer_gui_tests tests/gui-tests.cpp)
target_link_libraries(relaxer_gui_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_gui relaxer_gui_tests)
add_executable(relaxer_ai_widgets_tests tests/ai-settings-widgets-tests.cpp)
target_link_libraries(relaxer_ai_widgets_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_ai_widgets relaxer_ai_widgets_tests)
add_executable(relaxer_v11_tests tests/v11-tests.cpp)
target_link_libraries(relaxer_v11_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_v11 relaxer_v11_tests)

add_executable(relaxer_v12_tests tests/v12-tests.cpp)
target_link_libraries(relaxer_v12_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_v12 relaxer_v12_tests)

add_executable(relaxer_v13_tests tests/v13-tests.cpp)
target_link_libraries(relaxer_v13_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_v13 relaxer_v13_tests)

add_executable(relaxer_popup_tests tests/popup-tests.cpp)
target_link_libraries(relaxer_popup_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_popup relaxer_popup_tests)

add_executable(relaxer_motion_tests tests/motion-tests.cpp)
target_link_libraries(relaxer_motion_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_motion relaxer_motion_tests)

add_executable(relaxer_v14_tests tests/v14-tests.cpp)
target_link_libraries(relaxer_v14_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_v14 relaxer_v14_tests)

add_executable(relaxer_v15_tests tests/v15-tests.cpp)
target_link_libraries(relaxer_v15_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_v15 relaxer_v15_tests)
add_executable(relaxer_v16_tests tests/v16-tests.cpp)
target_link_libraries(relaxer_v16_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_v16 relaxer_v16_tests)

add_executable(relaxer_v19_tests tests/v19-tests.cpp)
target_link_libraries(relaxer_v19_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_v19 relaxer_v19_tests)
add_executable(relaxer_v110_tests tests/v110-tests.cpp)
target_link_libraries(relaxer_v110_tests PRIVATE relaxer_core Qt6::Test)
relaxer_add_qt_test(relaxer_v110 relaxer_v110_tests)
add_executable(relaxer_phase03_tests tests/phase03-tests.cpp)
target_link_libraries(relaxer_phase03_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_phase03 relaxer_phase03_tests)
add_executable(relaxer_phase031_tests tests/phase031-tests.cpp)
target_link_libraries(relaxer_phase031_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_phase031 relaxer_phase031_tests)
add_executable(relaxer_lyrics_search_report tests/lyrics-search-report.cpp)
target_link_libraries(relaxer_lyrics_search_report PRIVATE relaxer_ui Qt6::Test)

# Optional live website check; never run during the offline first-start suite.
add_executable(relaxer_browser_smoke EXCLUDE_FROM_ALL tests/browser-translation-smoke.cpp)
target_link_libraries(relaxer_browser_smoke PRIVATE relaxer_ui)

add_executable(relaxer_media_smoke_tests tests/media-smoke-tests.cpp)
target_include_directories(relaxer_media_smoke_tests PRIVATE src)
target_link_libraries(relaxer_media_smoke_tests PRIVATE Qt6::Core Qt6::Multimedia Qt6::Test)
add_dependencies(relaxer_media_smoke_tests Relaxer)
relaxer_add_qt_test(relaxer_media_smoke relaxer_media_smoke_tests)

add_executable(relaxer_lyric_match_compat_tests tests/lyric-match-compat-tests.cpp)
target_link_libraries(relaxer_lyric_match_compat_tests PRIVATE relaxer_ui Qt6::Test)
target_compile_definitions(relaxer_lyric_match_compat_tests PRIVATE RELAXER_LYRIC_MATCH_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/lyric-match-baseline.json")
relaxer_add_qt_test(relaxer_lyric_match_compat relaxer_lyric_match_compat_tests)

# Legacy include/API compatibility uses the same frozen assertions as the new
# no-Widgets facade test; product implementations have one core compilation owner.
add_executable(relaxer_settings_appearance_compat_tests tests/settings-appearance-tests.cpp)
target_link_libraries(relaxer_settings_appearance_compat_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_settings_appearance_compat relaxer_settings_appearance_compat_tests)
add_executable(relaxer_settings_appearance_widgets_tests tests/settings-appearance-widgets-tests.cpp)
target_link_libraries(relaxer_settings_appearance_widgets_tests PRIVATE relaxer_ui Qt6::Test)
relaxer_add_qt_test(relaxer_settings_appearance_widgets relaxer_settings_appearance_widgets_tests)
