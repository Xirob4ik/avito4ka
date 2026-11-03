# Applies PATCH_FILE to the current directory (a JUCE checkout) with git, once. Re-running is a no-op.
#   cmake -DGIT=<git> -DPATCH_FILE=<file> -P ApplyPatch.cmake
execute_process(COMMAND "${GIT}" apply --reverse --check "${PATCH_FILE}"
                RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
if(already_applied EQUAL 0)
    message(STATUS "JUCE patch already applied: ${PATCH_FILE}")
    return()
endif()
execute_process(COMMAND "${GIT}" apply --whitespace=nowarn "${PATCH_FILE}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not apply ${PATCH_FILE} to JUCE (is CLIPLINE_JUCE_TAG still 8.0.8?)")
endif()
message(STATUS "Applied JUCE patch: ${PATCH_FILE}")
