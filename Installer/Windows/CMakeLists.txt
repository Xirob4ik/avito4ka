# Clipline-<version>-Win64-Setup.exe: a single-file installer with the plugin embedded (see Installer.cpp).
# Built with the plugin on Windows (Visual Studio) and in the llvm-mingw cross build.

set(CLIPLINE_INSTALLER_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CLIPLINE_SOURCE_DIR    "${PROJECT_SOURCE_DIR}")
set(CLIPLINE_INSTALLER_NAME "Clipline-${PROJECT_VERSION}-Win64-Setup")

# Stage the plugin binary at a fixed path (stripped in MinGW builds) so the resource compiler can embed it and
# the installer is rebuilt whenever the plugin changes.
set(CLIPLINE_PAYLOAD_PLUGIN "${CMAKE_CURRENT_BINARY_DIR}/payload/Clipline.vst3.bin")
if(MINGW AND CMAKE_STRIP)
    set(_stage_cmd "${CMAKE_STRIP}" --strip-unneeded -o "${CLIPLINE_PAYLOAD_PLUGIN}" "$<TARGET_FILE:Clipline_VST3>")
else()
    set(_stage_cmd "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:Clipline_VST3>" "${CLIPLINE_PAYLOAD_PLUGIN}")
endif()
add_custom_command(
    OUTPUT  "${CLIPLINE_PAYLOAD_PLUGIN}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/payload"
    COMMAND ${_stage_cmd}
    DEPENDS Clipline_VST3 "$<TARGET_FILE:Clipline_VST3>"
    COMMENT "Staging the plugin for the installer"
    VERBATIM)

configure_file(installer.rc.in "${CMAKE_CURRENT_BINARY_DIR}/installer.rc" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/installer.rc" PROPERTIES
    OBJECT_DEPENDS "${CLIPLINE_PAYLOAD_PLUGIN};${CMAKE_CURRENT_SOURCE_DIR}/README.txt;${CMAKE_CURRENT_SOURCE_DIR}/installer.manifest;${CMAKE_CURRENT_SOURCE_DIR}/clipline.ico")

add_executable(CliplineSetup WIN32
    Installer.cpp
    resource.h
    "${CLIPLINE_PAYLOAD_PLUGIN}"
    "${CMAKE_CURRENT_BINARY_DIR}/installer.rc")
set_source_files_properties("${CLIPLINE_PAYLOAD_PLUGIN}" PROPERTIES HEADER_FILE_ONLY TRUE)

set_target_properties(CliplineSetup PROPERTIES OUTPUT_NAME "${CLIPLINE_INSTALLER_NAME}")
target_compile_definitions(CliplineSetup PRIVATE UNICODE _UNICODE CLIPLINE_VERSION_W=L"${PROJECT_VERSION}")
target_include_directories(CliplineSetup PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CliplineSetup PRIVATE shell32 ole32 uuid user32 gdi32 advapi32)

if(MSVC)
    target_compile_options(CliplineSetup PRIVATE /utf-8)        # Installer.cpp contains Russian text
    # The manifest comes from installer.rc; do not let the linker add a second one.
    target_link_options(CliplineSetup PRIVATE /MANIFEST:NO)
else()
    target_link_options(CliplineSetup PRIVATE -municode -mwindows -s)
endif()
