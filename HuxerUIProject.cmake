if(HUXERUI_PROJECT_PLAN_OUTPUT)
    get_filename_component(_pdf_plan_dir "${HUXERUI_PROJECT_PLAN_OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${_pdf_plan_dir}")
    file(WRITE "${HUXERUI_PROJECT_PLAN_OUTPUT}" [=[
{
  "schema": 1,
  "kind": "library",
  "name": "Lib-PDF",
  "id": "org.huxerui.lib.pdf",
  "target": "huxerui_pdf",
  "namespace": "huxerui",
  "publicTarget": "HuxerUI::PDF"
}
]=])
endif()
if(HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif()

if(NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    enable_language(CXX)
endif()

if(HUXERUI_PDF_BUILD_VIEW AND NOT TARGET HuxerUI::huxerui)
    set(HUXERUI_HOME "$ENV{HUXERUI_HOME}" CACHE PATH "HuxerUI SDK or source directory")
    set(HUXERUI_HOST_TOOL_ROOT "$ENV{HUXERUI_HOST_TOOL_ROOT}" CACHE PATH "HuxerUI host tool directory")
    if(EXISTS "${HUXERUI_HOME}/CMakeLists.txt" AND EXISTS "${HUXERUI_HOME}/include/huxerui/huxerui.h")
        set(HUXERUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(HUXERUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        if(ANDROID)
            set(HUXERUI_BUILD_SHARED ON CACHE BOOL "" FORCE)
            set(HUXERUI_BUILD_STATIC OFF CACHE BOOL "" FORCE)
        else()
            set(HUXERUI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
            set(HUXERUI_BUILD_STATIC ON CACHE BOOL "" FORCE)
        endif()
        add_subdirectory("${HUXERUI_HOME}" "${CMAKE_BINARY_DIR}/huxerui-sdk" EXCLUDE_FROM_ALL)
    elseif(HUXERUI_HOME)
        find_package(HuxerUI CONFIG REQUIRED PATHS "${HUXERUI_HOME}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    else()
        find_package(HuxerUI CONFIG REQUIRED)
    endif()
endif()
