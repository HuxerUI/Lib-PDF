if(HUXERUI_PROJECT_PLAN_OUTPUT)
    get_filename_component(_pdf_demo_plan_dir "${HUXERUI_PROJECT_PLAN_OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${_pdf_demo_plan_dir}")
    file(WRITE "${HUXERUI_PROJECT_PLAN_OUTPUT}" [=[
{
  "schema": 1,
  "kind": "app",
  "name": "PDF Preview",
  "id": "org.huxerui.lib.pdf.preview",
  "target": "example_pdf"
}
]=])
endif()
if(HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif()
if(NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    enable_language(CXX)
endif()
if(NOT TARGET HuxerUI::huxerui)
    set(HUXERUI_HOME "$ENV{HUXERUI_HOME}" CACHE PATH "HuxerUI SDK or source directory")
    set(HUXERUI_HOST_TOOL_ROOT "$ENV{HUXERUI_HOST_TOOL_ROOT}" CACHE PATH "HuxerUI host tools")
    if(EXISTS "${HUXERUI_HOME}/CMakeLists.txt" AND EXISTS "${HUXERUI_HOME}/include/huxerui/huxerui.h")
        set(HUXERUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(HUXERUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(HUXERUI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(HUXERUI_BUILD_STATIC ON CACHE BOOL "" FORCE)
        add_subdirectory("${HUXERUI_HOME}" "${CMAKE_BINARY_DIR}/huxerui-sdk" EXCLUDE_FROM_ALL)
    elseif(HUXERUI_HOME)
        find_package(HuxerUI CONFIG REQUIRED PATHS "${HUXERUI_HOME}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    else()
        find_package(HuxerUI CONFIG REQUIRED)
    endif()
endif()
if(WIN32)
    include("${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/huxerui.cmake")
elseif(APPLE AND NOT IOS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/huxerui.cmake")
endif()
set(HUXERUI_LIBRARY_GRAPH_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/.huxerui/generated/libraries.json")
