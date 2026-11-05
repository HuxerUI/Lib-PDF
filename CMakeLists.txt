cmake_minimum_required(VERSION 3.20)
if(CMAKE_HOST_APPLE AND NOT CMAKE_SYSTEM_NAME AND NOT CMAKE_TOOLCHAIN_FILE
        AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment target (pinned PDFium SDK)")
endif()
project(lib_pdf VERSION 0.1.0 LANGUAGES NONE)

option(HUXERUI_PDF_BUILD_VIEW "Build HuxerUI PDF components" ON)
option(HUXERUI_PDF_BUILD_TESTS "Build PDF tests" OFF)
option(HUXERUI_PDF_BUILD_EXAMPLES "Build PDF preview" OFF)
include("${CMAKE_CURRENT_SOURCE_DIR}/HuxerUIProject.cmake")
if(HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif()

set(_pdf_sources src/document.cpp src/png.cpp)
if(HUXERUI_PDF_BUILD_VIEW)
    huxerui_add_library(huxerui_pdf SOURCES src/pdf.cpp ${_pdf_sources})
else()
    add_library(huxerui_pdf STATIC ${_pdf_sources})
endif()
add_library(HuxerUI::PDF ALIAS huxerui_pdf)
if(HUXERUI_LIBRARY_GRAPH_ONLY)
    return()
endif()

include(cmake/PDFium.cmake)
set_target_properties(huxerui_pdf PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_compile_features(huxerui_pdf PUBLIC cxx_std_20)
target_include_directories(huxerui_pdf PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
target_link_libraries(huxerui_pdf PRIVATE PDFium::PDFium)
target_compile_definitions(huxerui_pdf PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(huxerui_pdf PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8;/W4;/permissive->"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>")
if(HUXERUI_PDF_BUILD_VIEW)
    target_link_libraries(huxerui_pdf PUBLIC HuxerUI::huxerui)
endif()

# Call this for each executable when using a shared PDFium SDK on Windows.
function(huxerui_pdf_copy_runtime target)
    if(WIN32 AND HUXERUI_PDF_RUNTIME_FILE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HUXERUI_PDF_RUNTIME_FILE}" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()

if(HUXERUI_PDF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(HUXERUI_PDF_BUILD_EXAMPLES)
    if(NOT HUXERUI_PDF_BUILD_VIEW)
        message(FATAL_ERROR "The preview requires HUXERUI_PDF_BUILD_VIEW=ON")
    endif()
    add_subdirectory(examples/preview)
endif()
