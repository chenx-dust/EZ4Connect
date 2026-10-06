cmake_minimum_required(VERSION 3.21)
project(EZ4ConnectCocoaTrayFix LANGUAGES CXX OBJCXX)

if(NOT APPLE)
    message(FATAL_ERROR "The Cocoa tray backport can only be built on macOS")
endif()
if(NOT CMAKE_OSX_ARCHITECTURES MATCHES "^(arm64|x86_64)$")
    message(FATAL_ERROR "Build a separate Cocoa backport for arm64 or x86_64")
endif()
if(NOT COCOA_FIX_SOURCE_DIR OR NOT COCOA_FIX_OUTPUT_DIR OR NOT COCOA_FIX_QT_PREFIX)
    message(FATAL_ERROR "Use scripts/build-macos-cocoa-tray-fix.sh to prepare verified source")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(QT_NO_CREATE_VERSIONLESS_TARGETS OFF)
find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Core Gui CorePrivate GuiPrivate)

# This backport uses private Qt ABI. Do not silently select another installation
# or apply the patch to a future Qt release, even when its public ABI is compatible.
file(REAL_PATH "${COCOA_FIX_QT_PREFIX}" qt_prefix)
foreach(module Core Gui CorePrivate GuiPrivate)
    file(REAL_PATH "${Qt6${module}_DIR}/../../.." module_prefix)
    if(NOT module_prefix STREQUAL qt_prefix)
        message(FATAL_ERROR "Qt ${module} was selected outside the requested SDK")
    endif()
    if(module MATCHES "^(Core|Gui)$" AND NOT EXISTS "${qt_prefix}/lib/Qt${module}.framework/Versions/A/Qt${module}")
        message(FATAL_ERROR "The backport requires the Qt 6.10.3 macOS framework SDK")
    endif()
endforeach()
if(NOT Qt6Core_VERSION VERSION_EQUAL "6.10.3" OR NOT Qt6Gui_VERSION VERSION_EQUAL "6.10.3")
    message(FATAL_ERROR "Only the verified Qt 6.10.3 private ABI is supported")
endif()
get_target_property(sdk_deployment_target Qt6::Core QT_DARWIN_MIN_DEPLOYMENT_TARGET)
if(NOT sdk_deployment_target OR sdk_deployment_target STREQUAL "sdk_deployment_target-NOTFOUND")
    message(FATAL_ERROR "The selected Qt SDK does not export its minimum macOS version")
endif()
if(CMAKE_OSX_DEPLOYMENT_TARGET AND NOT CMAKE_OSX_DEPLOYMENT_TARGET VERSION_EQUAL sdk_deployment_target)
    message(FATAL_ERROR "The plugin deployment target must match the SDK; do not raise it to the build host version")
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "${sdk_deployment_target}")

set(cocoa_source "${COCOA_FIX_SOURCE_DIR}/src/plugins/platforms/cocoa")
file(SHA256 "${cocoa_source}/CMakeLists.txt" cocoa_build_hash)
file(SHA256 "${cocoa_source}/qcocoasystemtrayicon.mm" cocoa_tray_hash)
if(NOT cocoa_build_hash STREQUAL "9f06730163264411d0737d8083c4ff011322af8f133c73ae098c66c3c5b2ee74"
   OR NOT cocoa_tray_hash STREQUAL "eb97cac41ee732a4ef82088a37eb40a03f4c0a4717fab5433f3fb4bf57a6be1c")
    message(FATAL_ERROR "The official source manifest or verified tray patch has changed")
endif()

# Use the SDK's exported feature configuration. An absent feature is an error,
# rather than an assumed default that could produce an ABI-incompatible plugin.
get_target_property(gui_enabled Qt6::Gui QT_ENABLED_PUBLIC_FEATURES)
get_target_property(gui_disabled Qt6::Gui QT_DISABLED_PUBLIC_FEATURES)
foreach(feature opengl vulkan accessibility sessionmanager systemtrayicon)
    if(feature IN_LIST gui_enabled AND NOT feature IN_LIST gui_disabled)
        set(QT_FEATURE_${feature} ON)
    elseif(feature IN_LIST gui_disabled AND NOT feature IN_LIST gui_enabled)
        set(QT_FEATURE_${feature} OFF)
    else()
        message(FATAL_ERROR "The SDK does not unambiguously export ${feature}")
    endif()
endforeach()
if(NOT QT_FEATURE_systemtrayicon)
    message(FATAL_ERROR "The selected SDK does not support system tray icons")
endif()

foreach(framework Foundation AppKit Carbon CoreServices CoreVideo IOKit IOSurface Metal QuartzCore UniformTypeIdentifiers CoreGraphics)
    find_library(FW${framework} ${framework} REQUIRED)
endforeach()

# Run Qt's original Cocoa CMakeLists, including its exact source list, included
# qnsview fragments, resources and feature conditions. These local adapters map
# only its verified build commands to installed-SDK CMake APIs; they never build
# or install Qt modules, and cannot write into the SDK.
function(qt_internal_add_plugin target)
    cmake_parse_arguments(PARSE_ARGV 1 plugin "" "OUTPUT_NAME;PLUGIN_TYPE"
        "DEFAULT_IF;SOURCES;DEFINES;PRECOMPILED_HEADER;LIBRARIES")
    if(plugin_UNPARSED_ARGUMENTS OR NOT target STREQUAL "QCocoaIntegrationPlugin"
       OR NOT plugin_OUTPUT_NAME STREQUAL "qcocoa" OR NOT plugin_PLUGIN_TYPE STREQUAL "platforms")
        message(FATAL_ERROR "Unexpected command in the pinned Qt Cocoa build manifest")
    endif()
    qt6_add_plugin(${target} SHARED PLUGIN_TYPE platforms)
    target_sources(${target} PRIVATE ${plugin_SOURCES})
    target_compile_definitions(${target} PRIVATE ${plugin_DEFINES})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${target} PRIVATE ${plugin_LIBRARIES})
    if(plugin_PRECOMPILED_HEADER)
        target_precompile_headers(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:OBJCXX>:${CMAKE_CURRENT_SOURCE_DIR}/qcocoa_plugin_pch.h>")
    endif()
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "qcocoa"
        PREFIX "lib"
        SUFFIX ".dylib"
        LIBRARY_OUTPUT_DIRECTORY "${COCOA_FIX_OUTPUT_DIR}/platforms"
        CXX_VISIBILITY_PRESET hidden
        OBJCXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_RPATH "@loader_path/../../Frameworks"
        INSTALL_RPATH_USE_LINK_PATH OFF
    )
endfunction()

function(qt_disable_apple_app_extension_api_only target)
    set_target_properties(${target} PROPERTIES
        QT_NO_APP_EXTENSION_ONLY_API TRUE
        XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY NO)
endfunction()

function(qt_internal_add_resource target resource_name)
    cmake_parse_arguments(PARSE_ARGV 2 resource "" "PREFIX" "FILES")
    if(resource_UNPARSED_ARGUMENTS OR NOT resource_name STREQUAL "qcocoaresources")
        message(FATAL_ERROR "Unexpected resource command in the pinned manifest")
    endif()
    qt6_add_resources(${target} ${resource_name}
        PREFIX "${resource_PREFIX}" FILES ${resource_FILES})
endfunction()

function(qt_internal_extend_target target)
    cmake_parse_arguments(PARSE_ARGV 1 extension "" "CONDITION" "SOURCES")
    if(extension_UNPARSED_ARGUMENTS OR NOT extension_CONDITION MATCHES
       "^QT_FEATURE_(opengl|vulkan|accessibility|sessionmanager)$")
        message(FATAL_ERROR "Unexpected feature command in the pinned manifest")
    endif()
    if(${extension_CONDITION})
        target_sources(${target} PRIVATE ${extension_SOURCES})
    endif()
endfunction()

add_subdirectory("${cocoa_source}" "cocoa")

add_executable(macos_tray_event_test "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/macos_tray_event_test.mm")
target_link_libraries(macos_tray_event_test PRIVATE Qt6::Gui Qt6::GuiPrivate "${FWAppKit}" "${FWCoreGraphics}")
set_target_properties(macos_tray_event_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${COCOA_FIX_OUTPUT_DIR}")

file(WRITE "${COCOA_FIX_OUTPUT_DIR}/qtbase-source-manifest.txt"
    "Qt version: 6.10.3\n"
    "Official source: https://codeload.github.com/qt/qtbase/tar.gz/refs/tags/v6.10.3\n"
    "Source archive SHA256: ab962447c44a03e02d8164bb9c2db80e2822121cead8bc18ffb63f8c30a413a5\n"
    "Patch: macos27-tray.patch (QTBUG-147449)\n"
    "Official Qt 6.11 backport: https://github.com/qt/qtbase/commit/6192d9edd00caa14ed6b67c32c0cd8cfe95cf815\n"
    "Equivalent guard uses Qt 6.10.3's existing cocoaEvent2QtMouseEvent helper.\n"
    "Architecture: ${CMAKE_OSX_ARCHITECTURES}\n"
    "Deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}\n"
    "SDK features: opengl=${QT_FEATURE_opengl}; vulkan=${QT_FEATURE_vulkan}; accessibility=${QT_FEATURE_accessibility}; sessionmanager=${QT_FEATURE_sessionmanager}; systemtrayicon=${QT_FEATURE_systemtrayicon}\n"
    "Build manifest: official Qt Cocoa CMakeLists.txt with installed-SDK adapters\n"
    "No binary modification or replacement of SDK files is performed.\n")
