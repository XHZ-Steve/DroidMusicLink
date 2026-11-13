cmake_minimum_required(VERSION 3.24)
project(DroidMediaLink VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
 # Keep Windows SDK min/max macros out of C++ standard-library expressions.
 add_compile_definitions(NOMINMAX)
endif()
if(MSVC)
 # C++20 __VA_OPT__ requires the conforming preprocessor, even with /std:c++20.
 add_compile_options(/Zc:preprocessor /utf-8)
endif()
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Concurrent Widgets Quick QuickControls2 QuickDialogs2 Network WebSockets ShaderTools)
qt_standard_project_setup(REQUIRES 6.11)
# Native Quick frame, pinned Apache-2.0 QWindowKit. No replacement UI framework.
find_package(Qt6 REQUIRED COMPONENTS CorePrivate GuiPrivate QuickPrivate)
set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_QUICK ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_INSTALL OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/qwindowkit EXCLUDE_FROM_ALL)
qt_add_executable(DroidMediaLink WIN32 src/main.cpp src/Bridge.cpp src/Bridge.h src/LyricProviders.cpp src/TrayPerformance.cpp src/TrayPerformance.h)
qt_add_qml_module(DroidMediaLink URI DroidMediaLink VERSION 1.0 QML_FILES qml/Main.qml qml/Icons.js qml/GlassMaterial.qml qml/DesignTokens.qml qml/AmbientField.qml qml/CoverArtwork.qml qml/CapsuleProgress.qml qml/SettingsPage.qml qml/LyricView.qml qml/DesktopLyricsWindow.qml qml/NowPlayingCanvas.qml qml/TypographyTokens.qml qml/LyricInk.qml SOURCES src/LyricTimeline.h src/NativeWindowController.h src/NativeWindowController.cpp)
target_include_directories(DroidMediaLink PRIVATE src)
target_link_libraries(DroidMediaLink PRIVATE QWKQuick Qt6::Concurrent Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Network Qt6::WebSockets crypt32 dwmapi psapi)
install(TARGETS DroidMediaLink RUNTIME DESTINATION .)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../artifacts/native/WindowsNative.dll")
 add_custom_command(TARGET DroidMediaLink POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../artifacts/native/WindowsNative.dll" "$<TARGET_FILE_DIR:DroidMediaLink>/WindowsNative.dll")
endif()
enable_testing()
qt_add_executable(core_tests tests/core_tests.cpp src/Bridge.cpp src/Bridge.h src/LyricProviders.cpp)
target_include_directories(core_tests PRIVATE src)
target_link_libraries(core_tests PRIVATE Qt6::Concurrent Qt6::Gui Qt6::Qml Qt6::Network Qt6::WebSockets crypt32)
add_test(NAME core_tests COMMAND core_tests)
qt_add_executable(link_probe tests/link_probe.cpp src/Bridge.cpp src/Bridge.h src/LyricProviders.cpp)
target_include_directories(link_probe PRIVATE src)
target_link_libraries(link_probe PRIVATE Qt6::Concurrent Qt6::Gui Qt6::Qml Qt6::Network Qt6::WebSockets crypt32)

qt_add_resources(DroidMediaLink "lucide_icons" PREFIX "/icons" BASE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/vendor/lucide/font" FILES "${CMAKE_CURRENT_SOURCE_DIR}/../assets/vendor/lucide/font/lucide.ttf")
# Build-tree tests must find Qt and the compiler runtime without a developer PATH.
if(WIN32)
 get_filename_component(DML_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
 set_tests_properties(core_tests PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${DML_COMPILER_BIN}")
endif()

# liquidglass Qt preview, pinned source and attribution in vendor/liquidglass.
function(dml_add_glass_resources target)
 qt_add_resources(${target} "${target}_liquidglass_qml" PREFIX "/qt/qml/DroidMediaLink" FILES
  vendor/liquidglass/LiquidGlass/GlassSurface.qml
  vendor/liquidglass/LiquidGlass/GlassEffect.qml
  vendor/liquidglass/LiquidGlass/material.js
  vendor/liquidglass/LiquidGlass/qmldir)
 qt_add_shaders(${target} "${target}_liquidglass_shaders" PRECOMPILE BATCHABLE
  PREFIX "/qt/qml/DroidMediaLink/vendor/liquidglass/LiquidGlass/shaders"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/liquidglass/LiquidGlass/shaders"
  GLSL "300 es,330" HLSL 50 MSL 12
  FILES vendor/liquidglass/LiquidGlass/shaders/liquidglass.vert
        vendor/liquidglass/LiquidGlass/shaders/liquidglass.frag)
endfunction()
dml_add_glass_resources(DroidMediaLink)
qt_add_executable(glass_smoke tests/glass_smoke.cpp)
target_link_libraries(glass_smoke PRIVATE Qt6::Quick)
dml_add_glass_resources(glass_smoke)
set_source_files_properties(tests/GlassSmoke.qml PROPERTIES QT_RESOURCE_ALIAS "qml/GlassSmoke.qml")
qt_add_resources(glass_smoke "glass_smoke_qml" PREFIX "/qt/qml/DroidMediaLink" FILES
 tests/GlassSmoke.qml qml/GlassMaterial.qml)

qt_add_shaders(DroidMediaLink "ambient_shaders" PRECOMPILE
 PREFIX "/qt/qml/DroidMediaLink/shaders" BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
 GLSL "300 es,330" HLSL 50 MSL 12 FILES shaders/ambient.frag shaders/lyricink.frag shaders/lyricfade.frag shaders/lyricfocus.frag)

qt_add_executable(lyrics_tests tests/lyrics_tests.cpp)
target_include_directories(lyrics_tests PRIVATE src)
target_link_libraries(lyrics_tests PRIVATE Qt6::Core)
add_test(NAME lyrics_tests COMMAND lyrics_tests)
qt_add_executable(lyrics_smoke tests/lyrics_smoke.cpp)
target_include_directories(lyrics_smoke PRIVATE src)
target_link_libraries(lyrics_smoke PRIVATE Qt6::Quick Qt6::QuickControls2)
if(WIN32)
 set_tests_properties(lyrics_tests PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${DML_COMPILER_BIN}")
endif()

qt_add_executable(lyrics_engine_tests tests/lyrics_engine_tests.cpp src/Bridge.cpp src/Bridge.h src/LyricProviders.cpp)
target_include_directories(lyrics_engine_tests PRIVATE src)
target_link_libraries(lyrics_engine_tests PRIVATE Qt6::Concurrent Qt6::Gui Qt6::Qml Qt6::Network Qt6::WebSockets crypt32)
add_test(NAME lyrics_engine_tests COMMAND lyrics_engine_tests)
if(WIN32)
 set_tests_properties(lyrics_engine_tests PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${DML_COMPILER_BIN}")
endif()

qt_add_shaders(lyrics_smoke "lyrics_smoke_ink" PRECOMPILE
 PREFIX "/qt/qml/DroidMediaLink/shaders" BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
 GLSL "300 es,330" HLSL 50 MSL 12 FILES shaders/lyricink.frag shaders/lyricfade.frag shaders/lyricfocus.frag)

qt_add_executable(lyrics_provider_tests tests/lyrics_provider_tests.cpp src/LyricProviders.cpp)
target_include_directories(lyrics_provider_tests PRIVATE src)
target_link_libraries(lyrics_provider_tests PRIVATE Qt6::Concurrent Qt6::Gui Qt6::Network)
add_test(NAME lyrics_provider_tests COMMAND lyrics_provider_tests)
if(WIN32)
 set_tests_properties(lyrics_provider_tests PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${DML_COMPILER_BIN}")
endif()
qt_add_executable(typography_smoke tests/typography_smoke.cpp)
target_link_libraries(typography_smoke PRIVATE Qt6::Quick Qt6::QuickControls2)

dml_add_glass_resources(lyrics_smoke)

get_target_property(DML_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(DML_QT_BIN "${DML_QMAKE_EXECUTABLE}" DIRECTORY)
find_program(DML_QMLTESTRUNNER qmltestrunner HINTS "${DML_QT_BIN}")
if(DML_QMLTESTRUNNER)
 add_test(NAME qml_interaction_tests COMMAND "${DML_QMLTESTRUNNER}" -platform offscreen -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml" -import "${CMAKE_CURRENT_SOURCE_DIR}/qml")
 set_tests_properties(qml_interaction_tests PROPERTIES TIMEOUT 45 ENVIRONMENT "QT_QUICK_CONTROLS_STYLE=Basic;QT_QUICK_BACKEND=software;QT_FORCE_STDERR_LOGGING=1")
 if(WIN32)
  set_property(TEST qml_interaction_tests APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
  set_tests_properties(qml_interaction_tests PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${DML_COMPILER_BIN}")
 endif()
endif()

qt_add_resources(DroidMediaLink "window_icons" PREFIX "/icons/window" BASE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/vendor/lucide/icons" FILES
 "${CMAKE_CURRENT_SOURCE_DIR}/../assets/vendor/lucide/icons/minus.svg"
 "${CMAKE_CURRENT_SOURCE_DIR}/../assets/vendor/lucide/icons/square.svg"
 "${CMAKE_CURRENT_SOURCE_DIR}/../assets/vendor/lucide/icons/copy.svg"
 "${CMAKE_CURRENT_SOURCE_DIR}/../assets/vendor/lucide/icons/x.svg"
)
