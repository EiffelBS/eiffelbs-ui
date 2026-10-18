cmake_minimum_required(VERSION 3.22)

project(eiffelbs-ui VERSION 0.17.3 LANGUAGES C CXX)


# eiffelbs-ui: header-only shared JUCE library implementing the EiffelBS
# design system (palette, fonts, LookAndFeel, generic widgets).
#
# Consumption (FetchContent, pinned tags):
#   include(FetchContent)
#   FetchContent_Declare(eiffelbs-ui
#       GIT_REPOSITORY https://github.com/EiffelBS/eiffelbs-ui.git
#       GIT_TAG        v0.17.3)
#   FetchContent_MakeAvailable(eiffelbs-ui)
#   target_link_libraries(myApp PRIVATE eiffelbs::ui juce::juce_gui_basics)
#
# The INTERFACE target carries the include directory and the ABI-critical
# JUCE compile definitions; it pulls no JUCE of its own except the
# juce_gui_extra link below - consumers link their own JUCE modules.
# The two audio widgets (Waveform.h, ThumbnailPool.h) are deliberately NOT in the
# umbrella eiffelbs.h: they need juce::AudioThumbnail, so they pull
# <juce_audio_utils> and that compiles the whole juce_audio_devices module inside
# every consumer TU. A host that uses them includes the header directly and links
# juce::juce_audio_utils.
#
# v0.12.29: ebs::Readability (include/eiffelbs/TextReadability.h) backs a glyph
# run so it reads over an arbitrary background (drop shadow, contour, scrim),
# for text painted over a video or an animation. It needs only juce_gui_basics,
# so it IS in the umbrella.

# v0.13.0: ebs::FrameBand (include/eiffelbs/FrameBand.h) - a horizontal row of
# frames that keeps each block at the width its content asks for, gives spare room
# to the blocks that can use it, and scrolls only below the hard floor. The scroll
# cues share the chevron glyph, now factored out as ebs::drawChevronGlyph.

# v0.13.1: FrameBand::setFrameWidths takes the growable flag, so a host can
# re-state a block whose width is fixed by its content without making it
# absorb spare room. Same behaviour for callers that pass three arguments.

# v0.14.0: FrameBand lays every frame at the width its content asks for and never
# stretches it - spare room stays empty at the right edge, because a stretched block
# has a layout that no longer matches its content. growable now means only "may be
# shrunk toward its floor before the row scrolls", so a host can open an advanced
# zone: a block that used to be squeezed shut keeps its full width.

# v0.14.1: drawReadabilityBacking() takes the caller's own `fade` for the run
# (0..1, default 1) and multiplies the backing's ALPHA by it, leaving the
# stroke width to `strength`. A host that fades its rows - a lyric roll dims
# the lines away from the centre - had a full-strength contour around a
# nearly-gone word, which reads as the outline being the text. Source
# compatible: the parameter is defaulted, so a host that fades nothing paints
# the same pixels as before.

# v0.15.0: FrameBand gains a discreet scroll strip along its bottom edge - a hairline
# track with a thumb as long as the share of the row the view shows, painted only
# while content is hidden. Drag the thumb to move the row, press in the trough to
# jump one view width toward that side. Additive API: trackWidth(), thumbLength(),
# thumbX() and scrollTrack(), so a host can put a tooltip on the strip.

# v0.15.1: the strip is pushed to the front of the child list by addFrame. JUCE z-order
# is the child index, where 0 is the BACK and the last entry is the front, so adding the
# strip before the frames left it painted UNDER them - invisible in a real host, which no
# headless paint test can see. The cues are re-fronted after it, so they stay on top.
# v0.15.2: IconButton shapes `expand` / `compress` (Feather maximize-2 /
# minimize-2, verbatim): the fullscreen enter / exit pair, four corners
# pointing out / in.

# v0.15.3: drawReadabilityBacking() takes the caller's `contour` colour for
# the outline (7th parameter, default opaque black). The shadow stays
# neutral black whatever it says; the historic 0xE6 base opacity is kept
# for every colour, so a host that passes nothing paints the same pixels
# as before.

# v0.15.4: ebs::LevelMeter (include/eiffelbs/LevelMeter.h) - a compact output meter:
# a peak bar, a gain-reduction segment drawn past the peak, and one dB text line with
# host-supplied captions. It takes three numbers (peak linear, gain reduction in dB,
# trim in dB) and computes nothing, so the audio code stays the single source of truth.
# Needs only juce_gui_basics, so it IS in the umbrella.

# v0.15.5: the LevelMeter gain-reduction segment is sized by the reduction itself, on a
# 12 dB scale, drawn from the right edge inwards OVER the peak fill. The v0.15.4 segment
# was exact arithmetic (the level asked for minus the level got) and measured almost
# nothing on screen: a limiter at its ceiling leaves only a sliver of that difference
# inside a bar normalised to full scale, so the red tail the smoke test proved at 6 dB
# was 1-2 px wide in a real cell. 6 dB now covers half the bar, 12 dB all of it.

# v0.15.6: the contour's WIDTH is the reader's to set.
# readabilityContourWidth (fontH, weight = 1) and drawReadabilityBacking()'s
# 8th parameter `contourWeight` (default 1) multiply the designed stroke
# width, AFTER its relative clamp - a weight folded INTO the clamp would do
# nothing on the smallest rows, which is exactly where a reader who asks for
# a heavier contour is looking. The rail is 0.25..4, and 1 is byte-identical
# to v0.15.3.

# v0.15.7: nothing moves but a note. v0.15.6 shipped an NB claiming the pushed
# v0.15.4 tag carried the TextReadability weight - it does not: the remote's
# v0.15.4 is 0c9c13c, the LevelMeter bar. Two lines of work landed the same hour
# and a local tag that was refused its push never left that checkout, so v0.15.4
# is exactly what its own entry says. This version is that correction and
# nothing else.
# v0.15.11: ebs::FrameTitleBand (include/eiffelbs/FrameTitleBand.h) - a left vertical title spine for a
# frame, reading bottom-to-top, clipping to the frame's own rounded path so its corners hug the curve and
# shrinking its font from 13 pt to 9 pt until the title fits the band height. It costs the frame WIDTH =
# ebs::kBandWidth (17 px) fixed for every locale, since a locale's longer name eats the spare HEIGHT of the
# row, not the width each frame is measured against. The shared curve became ebs::frameCornerRadius so the
# band and drawGroupFrame can no longer drift apart. Additive: existing consumers paint the same pixels.

# v0.15.12-v0.15.15: the title spine's caption. Uppercase bold, tracked apart, at
# 50 % strength, drawn WITHOUT accents (ebs::bandCaps - French all-caps convention,
# CJK untouched). A longer translated name shrinks its glyphs from 13 pt to 9 pt
# rather than widening the fixed 17 px band, and every shipped locale title is
# proven to fit the band height.

# v0.16.0: documentation and cleanup release. The README's Contents table now lists
# every shipped component, its consumption example pins the current tag, and the
# roadmap is no longer a second, drifting copy of these notes. The transitional
# CompatOt.h shim is REMOVED - its only consumer migrated, and it named the old
# in-house `ot::` namespace and its OT* headers, which have no place in a public
# library - as are the last OT* file names in the Theme / Fonts / LookAndFeel
# provenance notes. No behaviour change otherwise.

# v0.17.0: a fold has an AXIS. ExpansibleFrame::setFoldVertical (false by default) tells
# the frame that its zone grows DOWNWARD instead of sideways - what a short, wide frame in
# a bottom band needs, since it has no width to spare and all the room below it. The strip
# itself does NOT move: it stays on the right edge, because a horizontal strip would spend
# the very height such a frame is short of, and interior() / resized() are untouched.
# What follows the axis is the GLYPH: DisclosureChevron::setVerticalFold draws a DOWN
# chevron when folded and an UP one when open, since a chevron pointing right at a zone
# that opens downward is the decoration the component refuses. drawChevronGlyph gains a
# four-way ChevronDirection overload (Left / Right / Up / Down); the historic bool overload
# still draws the horizontal pair, so FrameBand's scroll cues are byte-identical. The
# default keeps every existing host - the karaoke band included - painting the same pixels.

# v0.17.1: ebs::ActivityDot (include/eiffelbs/ActivityDot.h) - one small dot that says whether
# a signal PATH is carrying anything. A knob at 0 % and a switched-off stage look identical on
# screen, so "no sound" is diagnosed by guessing; this reads the AUDIO instead: a disc sized by
# the level the path produced, dark below -45 dBFS and full at full scale, with a one-call
# attack and a ~0.6 s release so a syllable between two notes reads as continuous activity.
# Display only - the host feeds it from its own atomic snapshot at its own tick rate, and the
# component repaints only when the disc it would draw actually moved. Wordless on purpose: the
# caption and its locale belong to the host, exactly like LevelMeter's. Additive: nothing else
# changes, and the dB arithmetic stays in <cmath> so a UI header never drags juce_audio_basics
# into a consumer.

# v0.17.2: ebs::ActivityKnob (include/eiffelbs/ActivityKnob.h) - an ebs::Knob that carries the
# activity dot AT ITS CENTRE. A component that owns exactly ONE knob has nowhere else to put an
# indicator: a dot beside the toggle label needs 80-94 px inside a 74 px cell (measured in
# OpenVoxTuner), and a strip under the cell costs 12 px of height and reads as a second object.
# The dial paints exactly as a plain knob paints - the shared LookAndFeel draws it, then the disc
# goes on top of the centre, which ebs::drawKnob leaves free because the pointer line starts 8 px
# out and grows outward. The disc is bounded by the knob's face, so a squeezed dial shrinks the
# indicator instead of spilling over its rim, and no slot background is painted by default: on a
# dark cap a dark disc reads as a hole. Additive: ebs::ActivityState (the level, the dB mapping,
# the release ramp) and ebs::paintActivityDisc() are now shared with ActivityDot, so a dot in a
# knob and a dot on a panel cannot drift apart, and ebs::Knob itself is untouched.

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
