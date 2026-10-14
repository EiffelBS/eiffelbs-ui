cmake_minimum_required(VERSION 3.22)

project(eiffelbs-ui VERSION 0.12.7 LANGUAGES C CXX)

# eiffelbs-ui: header-only shared JUCE library implementing the EiffelBS
# design system (palette, fonts, LookAndFeel, generic widgets).
#
# Consumption (FetchContent, pinned tags):
#   include(FetchContent)
#   FetchContent_Declare(eiffelbs-ui
#       GIT_REPOSITORY https://github.com/EiffelBS/eiffelbs-ui.git
#       GIT_TAG        v0.12.7)
#   FetchContent_MakeAvailable(eiffelbs-ui)
#   target_link_libraries(myApp PRIVATE eiffelbs::ui juce::juce_gui_basics)
#
# The INTERFACE target carries the include directory and the ABI-critical
# JUCE compile definitions; it pulls no JUCE of its own except the
# juce_gui_extra link below - consumers link their own JUCE modules.

add_library(eiffelbs-ui INTERFACE)
add_library(eiffelbs::ui ALIAS eiffelbs-ui)

target_compile_features(eiffelbs-ui INTERFACE cxx_std_20)

target_include_directories(eiffelbs-ui INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# v0.12.7: ebs::LogView (include/eiffelbs/StatusBar.h) is built on
# juce::CodeEditorComponent + juce::CodeDocument, which live in
# juce_gui_extra. Consumers must therefore have their JUCE modules
# defined BEFORE FetchContent_MakeAvailable(eiffelbs-ui) - they always do,
# since every ebs header includes juce_gui_basics. The link is
# UNCONDITIONAL on purpose: a consumer that forgot the module gets a
# configure-time "target juce::juce_gui_extra not found" instead of a
# cryptic C1083 inside its own build.
target_link_libraries(eiffelbs-ui INTERFACE juce::juce_gui_extra)

# ABI contract: every binary in a process MUST share these values with its
# JUCE build (two juce::String ABIs in one process is an ODR violation ->
# random crashes). Same doctrine as OpenVoxTuner, which set
# exactly these values project-wide.
target_compile_definitions(eiffelbs-ui INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STRING_UTF_TYPE=8)

if (MSVC)
    # Library sources are UTF-8; without this flag MSVC reads them in an
    # ANSI code page and non-ASCII literals turn to mojibake.
    target_compile_options(eiffelbs-ui INTERFACE
        "$<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>")
endif()

# Standalone smoke test (offscreen rendering proof against a real JUCE).
# OFF by default so FetchContent consumers pay nothing.
option(EIFFELBS_UI_BUILD_SMOKE_TEST
    "Build the standalone smoke-test executable" OFF)

if (PROJECT_IS_TOP_LEVEL AND EIFFELBS_UI_BUILD_SMOKE_TEST)
    add_subdirectory(tests)
endif()
