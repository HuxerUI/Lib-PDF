cmake_minimum_required(VERSION 3.20)
if(CMAKE_HOST_APPLE AND NOT CMAKE_SYSTEM_NAME AND NOT CMAKE_TOOLCHAIN_FILE
        AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment target")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION" PDF_PREVIEW_VERSION LIMIT_COUNT 1)
string(STRIP "${PDF_PREVIEW_VERSION}" PDF_PREVIEW_VERSION)
project(example_pdf VERSION ${PDF_PREVIEW_VERSION} LANGUAGES NONE)
include("${CMAKE_CURRENT_SOURCE_DIR}/HuxerUIProject.cmake")
if(HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif()

set(_pdf_demo_sources src/app.cpp)
set(_pdf_demo_resource_args)
if(NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    if(ANDROID AND HUXERUI_ANDROID_RESOURCE_OUTPUT_ROOT)
        list(APPEND _pdf_demo_resource_args RESOURCE_OUTPUT_DIRECTORY
            "${HUXERUI_ANDROID_RESOURCE_OUTPUT_ROOT}/${ANDROID_ABI}")
    endif()
    if(WIN32)
        list(APPEND _pdf_demo_sources platform/windows/main.cpp)
    elseif(APPLE AND NOT IOS)
        list(APPEND _pdf_demo_sources platform/macos/main.cpp)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND _pdf_demo_sources platform/linux/main.cpp)
    elseif(ANDROID)
        # MainActivity loads the application shared library declared by app.cpp.
    elseif(IOS)
        # The Xcode shell links the static application core declared by app.cpp.
    else()
        message(FATAL_ERROR "The PDF preview provides Windows, macOS, Linux, Android, and iOS entry points")
    endif()
endif()

huxerui_add_app(example_pdf
    SOURCES ${_pdf_demo_sources}
    RESOURCES resources
    RESOURCE_NAMESPACE pdf_demo
    ${_pdf_demo_resource_args}
    BUNDLE_NAME "PDF Preview"
    BUNDLE_IDENTIFIER "org.huxerui.lib.pdf.preview")
if(TARGET HuxerUI::PDF)
    huxerui_use_library(example_pdf TARGET HuxerUI::PDF)
else()
    huxerui_use_library(example_pdf TARGET HuxerUI::PDF PATH "${CMAKE_CURRENT_SOURCE_DIR}/../..")
endif()
if(HUXERUI_LIBRARY_GRAPH_ONLY)
    return()
endif()
option(PDF_PREVIEW_MOBILE_SMOKE "Write the first rendered sample page result for mobile CI" OFF)
if(PDF_PREVIEW_MOBILE_SMOKE)
    target_compile_definitions(example_pdf PRIVATE PDF_PREVIEW_MOBILE_SMOKE=1)
endif()
if(WIN32)
    set(PDF_PREVIEW_PLATFORM "Windows")
elseif(ANDROID)
    set(PDF_PREVIEW_PLATFORM "Android")
elseif(IOS)
    set(PDF_PREVIEW_PLATFORM "iOS")
elseif(APPLE)
    set(PDF_PREVIEW_PLATFORM "macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PDF_PREVIEW_PLATFORM "Linux")
else()
    set(PDF_PREVIEW_PLATFORM "Unknown")
endif()
target_compile_definitions(example_pdf PRIVATE
    PDF_PREVIEW_VERSION="${PROJECT_VERSION}"
    PDF_PREVIEW_PLATFORM="${PDF_PREVIEW_PLATFORM}")
target_compile_options(example_pdf PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
target_include_directories(example_pdf PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(WIN32)
    string(REPLACE "." "," _pdf_preview_version_commas "${PROJECT_VERSION}")
    set(PDF_PREVIEW_VERSION_COMMAS "${_pdf_preview_version_commas}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY)
    configure_file(
        "${HUXERUI_WINDOWS_MANIFEST_SOURCE}"
        "${CMAKE_CURRENT_BINARY_DIR}/app.manifest"
        @ONLY)
    target_sources(example_pdf PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.manifest"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    add_custom_command(TARGET example_pdf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION" "$<TARGET_FILE_DIR:example_pdf>/VERSION"
        VERBATIM)
elseif(APPLE AND NOT IOS)
    set_target_properties(example_pdf PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${HUXERUI_MACOS_INFO_PLIST}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_command(TARGET example_pdf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION" "$<TARGET_FILE_DIR:example_pdf>/VERSION"
        VERBATIM)
endif()
huxerui_pdf_copy_runtime(example_pdf)

if(HUXERUI_PDF_BUILD_TESTS AND TARGET HuxerUI::testing)
    add_executable(pdf_demo_tests tests/smoke.cpp src/app.cpp)
    huxerui_enable_codegen(pdf_demo_tests)
    target_link_libraries(pdf_demo_tests PRIVATE HuxerUI::PDF HuxerUI::testing)
    target_include_directories(pdf_demo_tests PRIVATE "$<TARGET_PROPERTY:example_pdf,INCLUDE_DIRECTORIES>")
    target_compile_definitions(pdf_demo_tests PRIVATE PDF_DEMO_PACKAGE="$<TARGET_FILE_DIR:example_pdf>/example_pdf.resources")
    target_compile_options(pdf_demo_tests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_dependencies(pdf_demo_tests example_pdf)
    huxerui_pdf_copy_runtime(pdf_demo_tests)
    add_test(NAME pdf.demo COMMAND pdf_demo_tests)
    set_tests_properties(pdf.demo PROPERTIES TIMEOUT 60)
endif()
