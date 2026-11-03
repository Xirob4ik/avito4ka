# Locates JUCE. Priority:
#   1. -DCLIPLINE_JUCE_DIR=<path to a JUCE checkout>   (apply cmake/patches/juce-8.0.8-clipline.patch to it yourself)
#   2. ./external/JUCE (e.g. a git submodule)          (same)
#   3. FetchContent from GitHub (pinned tag below)    (the patch is applied automatically)
#
# cmake/patches/juce-8.0.8-clipline.patch:
#   - fixes a hang when the plugin window is closed while DXGI cannot wait for vertical blank (display off,
#     remote desktop, virtual displays): JUCE's VBlank thread never noticed the exit request (all builds);
#   - lets JUCE build with llvm-mingw (only matters for the Linux -> Windows cross build; no effect on MSVC).

set(CLIPLINE_JUCE_TAG "8.0.8" CACHE STRING "JUCE git tag fetched when no local JUCE is provided")
set(CLIPLINE_JUCE_PATCH "${CMAKE_CURRENT_LIST_DIR}/patches/juce-8.0.8-clipline.patch")

if(CLIPLINE_JUCE_DIR)
    message(STATUS "Clipline: using JUCE from CLIPLINE_JUCE_DIR = ${CLIPLINE_JUCE_DIR}")
    add_subdirectory("${CLIPLINE_JUCE_DIR}" "${CMAKE_BINARY_DIR}/_juce" EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(STATUS "Clipline: using JUCE from external/JUCE")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" "${CMAKE_BINARY_DIR}/_juce" EXCLUDE_FROM_ALL)
else()
    message(STATUS "Clipline: fetching JUCE ${CLIPLINE_JUCE_TAG} from GitHub")
    find_package(Git REQUIRED)
    include(FetchContent)
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${CLIPLINE_JUCE_TAG}
        GIT_SHALLOW    ON
        PATCH_COMMAND  "${CMAKE_COMMAND}" -DGIT=${GIT_EXECUTABLE} -DPATCH_FILE=${CLIPLINE_JUCE_PATCH}
                       -P "${CMAKE_CURRENT_LIST_DIR}/ApplyPatch.cmake"
        UPDATE_DISCONNECTED ON)
    FetchContent_MakeAvailable(juce)
endif()
