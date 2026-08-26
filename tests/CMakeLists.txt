# Standalone smoke test for eiffelbs-ui.
#
# Fetches JUCE 8.0.8 (same pin as OpenTimbre / OpenVoxTuner). To reuse an
# existing local checkout instead of downloading ~150 MB:
#   cmake -B build -DEIFFELBS_UI_BUILD_SMOKE_TEST=ON \
#         -DEIFFELBS_UI_JUCE_DIR="C:/path/to/JUCE"

set(EIFFELBS_UI_JUCE_DIR "" CACHE PATH
    "Local JUCE checkout used for the smoke test (skips the download)")

if (EIFFELBS_UI_JUCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${EIFFELBS_UI_JUCE_DIR}" CACHE PATH
        "FetchContent override: local JUCE source directory" FORCE)
endif()

include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS  OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

add_executable(eiffelbs-ui-smoke Smoke.cpp)

target_link_libraries(eiffelbs-ui-smoke PRIVATE
    eiffelbs-ui
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)
