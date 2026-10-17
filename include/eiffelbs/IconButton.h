// IconButton.h
// eiffelbs-ui - shared vector icon button (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from an in-house icon button (original copyright
// (C) 2026 EiffelBS); icon sources originally ported from OpenVoxTuner.
//
// undo / redo / settings / refresh are VECTOR icons ported from real SVG
// sources (OpenVoxTuner's Feather set + Material "refresh"), parsed once
// through JUCE's SVG engine and flattened to outlines - see the source
// table below. play/stop/cross/folder/grip remain simple procedural
// juce::Path shapes (no Unicode glyphs: font rendering varies across
// systems). Icon centred in a 20..28 px button, configurable colour,
// brighter on hover; tooltip shown by the host's TooltipWindow. Primary
// actions keep their strong colours (play = accent blue, delete = red);
// secondary icons default to quiet textDim. A button TALLER than the
// classic 20..28 px slot scales its glyph with its height (the karaoke
// transport row runs 44 px buttons beside a waveform); the small sizes
// keep the 10 px glyph unchanged.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <map>

#include "eiffelbs/Theme.h"
#include "eiffelbs/LookAndFeel.h"   // resolved(): theme-level colour hook

namespace ebs
{

// === Vector icon sources (ported, NOT hand-drawn) ===========================
//
// kSvgUndo / kSvgRedo / kSvgGear are the VERBATIM Feather-style SVG sources
// from OpenVoxTuner Source/PluginEditor.cpp (the gear is exactly what OVT's
// wrench-menu button renders). kSvgRefresh has no OVT counterpart: it is the
// standard Material Icons "refresh" glyph. All of them go through JUCE's
// real SVG engine and are flattened ONCE into fillable outline paths;
// paintButton only scales + recolours them.
static const char* const kSvgUndo = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 14 4 9 9 4"/><path d="M4 9h11a5 5 0 0 1 0 10h-4"/></svg>)";
static const char* const kSvgRedo = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 14 20 9 15 4"/><path d="M20 9H9a5 5 0 0 0 0 10h4"/></svg>)";
static const char* const kSvgGear = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>)";
// Feather "tool" (wrench) - tools/models-management entry point.
static const char* const kSvgTool = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z"/></svg>)";
// Feather "search" (magnifier) - reveal-a-folder rows.
static const char* const kSvgSearch = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>)";
// Feather-style padlock: body rectangle + shackle arc, stroked outline.
static const char* const kSvgLock   = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="11" width="18" height="10" rx="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>)";
static const char* const kSvgRefresh = R"(<svg viewBox="0 0 24 24" fill="#010101"><path d="M17.65 6.35C16.2 4.9 14.21 4 12 4c-4.42 0-7.99 3.58-7.99 8s3.57 8 7.99 8c3.73 0 6.84-2.55 7.73-6h-2.08c-.82 2.33-3.04 4-5.65 4-3.31 0-6-2.69-6-6s2.69-6 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z"/></svg>)";

static const char* const kSvgWand = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round"><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/></svg>)";
// Material "download" (arrow into tray) - model install actions.
static const char* const kSvgDownload = R"(<svg viewBox="0 0 24 24" fill="#010101"><path d="M19 9h-4V3H9v6H5l7 7 7-7zM5 18v2h14v-2H5z"/></svg>)";
// Feather "eye" - visible state of the model visibility toggle.
static const char* const kSvgEye = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>)";
// Feather "eye-off" - hidden state of the model visibility toggle.
static const char* const kSvgEyeOff = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/><path d="M14.12 14.12a3 3 0 1 1-4.24-4.24"/><line x1="1" y1="1" x2="23" y2="23"/></svg>)";

// Feather "copy" (two overlapping squares) - the clipboard affordance
// every copyable text needs. Drawn as a STROKE outline like the rest of the
// Feather set, so it stays legible at the 10 px the icon slot renders.
static const char* const kSvgCopy = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></svg>)";

// Material "pan_tool" filled glyph: reads as a real HAND at small sizes
// (the previous 2x3 dot grid did not).
static const char* const kSvgHand = R"(<svg viewBox="0 0 24 24" fill="#010101"><path d="M23 5.5V20c0 2.2-1.8 4-4 4h-7.3c-1.08 0-2.1-.43-2.85-1.19L1 14.83s1.26-1.23 1.3-1.25c.22-.19.49-.29.79-.29.22 0 .42.06.6.16.04.01 4.31 2.46 4.31 2.46V4.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5V11h1V1.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5V11h1V2.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5V11h1V5.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5z"/></svg>)";


// Kebab menu (three vertical dots) - the options/overflow affordance.
// Drawn as a FILLED outline (three dots), unlike the stroked Feather set:
// dots below the stroke-width floor vanish at the 10 px the icon slot uses.
static const char* const kSvgKebab = R"(<svg viewBox="0 0 24 24" fill="#010101"><circle cx="12" cy="5" r="1.5"/><circle cx="12" cy="12" r="1.5"/><circle cx="12" cy="19" r="1.5"/></svg>)";
// Repeat/loop: two arrows chasing each other (Feather "repeat").
static const char* const kSvgRepeat = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="17 1 21 5 17 9"/><path d="M3 11V9a4 4 0 0 1 4-4h14"/><polyline points="7 23 3 19 7 15"/><path d="M21 13v2a4 4 0 0 1-4 4H3"/></svg>)";
// Typography (Feather "type"): the A-shaped mark a typeface control carries.
static const char* const kSvgType = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="4 7 4 4 20 4 20 7"/><line x1="9" y1="20" x2="15" y2="20"/><line x1="12" y1="4" x2="12" y2="20"/></svg>)";
// A picture (Feather "image"): the frame, a sun and a horizon.
static const char* const kSvgImage = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>)";
// A film strip (Feather "film"): the VIDEO a backing mode is judged against.
static const char* const kSvgFilm = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="2" width="20" height="20" rx="2.18" ry="2.18"/><line x1="7" y1="2" x2="7" y2="22"/><line x1="17" y1="2" x2="17" y2="22"/><line x1="2" y1="12" x2="22" y2="12"/><line x1="2" y1="7" x2="7" y2="7"/><line x1="2" y1="17" x2="7" y2="17"/><line x1="17" y1="7" x2="22" y2="7"/><line x1="17" y1="17" x2="22" y2="17"/></svg>)";

// Tab-strip glyphs (Feather set, same 24-grid and stroke as the rest,
// except keys). One recognisable mark per section so the pill reads at
// a glance instead of seven text labels in a row.
static const char* const kSvgMusic = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/></svg>)";
static const char* const kSvgLayers = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="12 2 2 7 12 12 22 7 12 2"/><polyline points="2 17 12 22 22 17"/><polyline points="2 12 12 17 22 12"/></svg>)";
static const char* const kSvgActivity = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>)";
static const char* const kSvgChat = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 11.5a8.38 8.38 0 0 1-.9 3.8 8.5 8.5 0 0 1-7.6 4.7 8.38 8.38 0 0 1-3.8-.9L3 21l1.9-5.7a8.38 8.38 0 0 1-.9-3.8 8.5 8.5 0 0 1 4.7-7.6 8.38 8.38 0 0 1 3.8-.9h.5a8.48 8.48 0 0 1 8 8v.5z"/></svg>)";
static const char* const kSvgMic = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 1a3 3 0 0 0-3 3v8a3 3 0 0 0 6 0V4a3 3 0 0 0-3-3z"/><path d="M19 10v2a7 7 0 0 1-14 0v-2"/><line x1="12" y1="19" x2="12" y2="23"/><line x1="8" y1="23" x2="16" y2="23"/></svg>)";
// Piano keys (no Feather equivalent): stroked shell and separators, FILLED
// black keys. Mixed like the rest of the set (kebab is fill-only).
static const char* const kSvgKeys = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="6" width="20" height="12" rx="2"/><line x1="8" y1="6" x2="8" y2="18"/><line x1="16" y1="6" x2="16" y2="18"/><path d="M6.4 6h3.2v6.5H6.4z" fill="#010101" stroke="none"/><path d="M14.4 6h3.2v6.5h-3.2z" fill="#010101" stroke="none"/></svg>)";

// Viewer-toolbar glyphs, ported VERBATIM from OpenVoxTuner Source/PluginEditor.cpp
// (the same Feather set the gear, undo and redo came from): zoom in / zoom out
// (magnifier with a plus / minus), the four chevrons (scroll up / down and
// prev / next), grid (snap-to-grid), step (staircase = the quantise mode),
// power (the same mark ebs::PowerToggle draws, as an icon button), trash
// (clear-all) and save (write to disk).
static const char* const kSvgZoomIn      = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="11" y1="8" x2="11" y2="14"/><line x1="8" y1="11" x2="14" y2="11"/></svg>)";
static const char* const kSvgZoomOut     = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="8" y1="11" x2="14" y2="11"/></svg>)";
static const char* const kSvgChevronUp   = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="18 15 12 9 6 15"/></svg>)";
static const char* const kSvgChevronDown = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"/></svg>)";
static const char* const kSvgChevronLeft = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 18 9 12 15 6"/></svg>)";
static const char* const kSvgChevronRight= R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 18 15 12 9 6"/></svg>)";
static const char* const kSvgGrid        = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/></svg>)";
static const char* const kSvgStep        = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20v-5h6v-6h6V4h4"/></svg>)";
static const char* const kSvgPower       = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2v10"/><path d="M18.36 6.64a9 9 0 1 1-12.73 0"/></svg>)";
static const char* const kSvgTrash       = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 6h18"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/><line x1="10" y1="11" x2="10" y2="17"/><line x1="14" y1="11" x2="14" y2="17"/></svg>)";
static const char* const kSvgSave        = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/></svg>)";

// Feather "maximize-2" / "minimize-2" (verbatim): the fullscreen enter /
// exit pair - four corners pointing out / in. One glyph for both states
// would read as a single meaning; the pair names the direction of travel.
static const char* const kSvgExpand      = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 3 21 3 21 9"/><polyline points="9 21 3 21 3 15"/><line x1="21" y1="3" x2="14" y2="10"/><line x1="3" y1="21" x2="10" y2="14"/></svg>)";
static const char* const kSvgCompress    = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="4 14 10 14 10 20"/><polyline points="20 10 14 10 14 4"/><line x1="14" y1="10" x2="21" y2="3"/><line x1="3" y1="21" x2="10" y2="14"/></svg>)";

class IconButton : public juce::Button
{
public:
    /** Displayed shape: play = filled triangle (or stop square when
     *  toggled), stop = forced square, cross = delete cross, refresh =
     *  circular re-roll arrow, download = arrow into tray (model install),
     *  eye / eyeOff = visibility toggle pair, folder = folder silhouette,
     *  grip = drag handle, undo/redo = circular arrows, settings = gear,
     *  star = favourite marker (filled or thin outline, see setFilled),
     *  lock = padlock (host-transport link semantics are app-defined),
     *  rewind = return to start (transport: bar + left-pointing triangle),
     *  copy = two overlapping squares (clipboard). Tab-strip marks:
     *  music, layers, activity (audio over time), chat (speech bubble),
     *  mic, keys (piano) - one per section so navigation reads at
     *  a glance; the Assistant reuses sparkle, the established AI mark.
     *  repeat = loop toggle (two chasing arrows). Viewer toolbar: zoomIn /
     *  zoomOut, the four chevrons (scroll and prev / next), grid, step
     *  (staircase quantise), power, trash (clear-all) and save.
     *  expand / compress = fullscreen enter / exit pair (four corners
     *  pointing out / in - one glyph for both would name no direction). */
    enum class Shape { play, stop, cross, refresh, folder, grip,
                       undo, redo, settings, star, wand, tool, search,
                       lock, download, eye, eyeOff, rewind, copy, sparkle,
                       kebab, music, layers, activity, chat, mic, keys,
                       repeat, zoomIn, zoomOut, chevronUp, chevronDown,
                       chevronLeft, chevronRight, grid, step, power, trash,
                       save, expand, compress,
                       // The three a MENU can name: a typeface, a picture, and
                       // the video a backing mode is judged against. Added
                       // rather than borrowed - `layers` means Stems, `keys`
                       // means MIDI and `eye` is a show/hide pair, so reusing
                       // one would hang a second meaning on a shape this app
                       // already reads.
                       type, image, film,
                       // The transport's own pair: `stop` is a SQUARE (and is
                       // what a play/stop TOGGLE wears while it runs), while a
                       // control that PAUSES asks for this one - two bars with
                       // a gap. They look nothing alike on purpose, and the
                       // smoke tells them apart by their centre pixel, because
                       // "it painted something" cannot see a pause that fell
                       // through to the stop shape.
                       pause };

    /** JUCE-standard per-instance / theme-level colour hooks. Resolution
     *  order: Component::setColour() override > LookAndFeel::
     *  widgetThemeColour() hook > built-in shape-aware palette default.
     *  iconColourId maps to transparent at theme level by default so each
     *  shape keeps its semantic default (red cross, blue wand, ...). */
    enum ColourIds
    {
        iconColourId          = 0x1e4b0101,  ///< glyph colour
        frameFillColourId     = 0x1e4b0102,  ///< framed-mode background
        frameOutlineColourId  = 0x1e4b0103   ///< framed-mode outline
    };

    explicit IconButton (Shape s = Shape::play)
        : juce::Button ("IconButton"), shape (s)
    {
        if (s == Shape::cross)
        {
            setTooltip ("Delete");
            icon = danger();   // RED cross by default
        }
        else if (s == Shape::refresh)
        {
            setTooltip ("Re-roll");
            icon = textDim();   // secondary action: quiet grey
        }
        else if (s == Shape::folder)
        {
            setTooltip ("Reveal");
            icon = textDim();
        }
        else if (s == Shape::grip)
        {
            setTooltip ("Drag to DAW or Desktop");
            icon = textDim();
        }
        else if (s == Shape::undo)
        {
            setTooltip ("Undo");
            icon = textDim();
        }
        else if (s == Shape::redo)
        {
            setTooltip ("Redo");
            icon = textDim();
        }
        else if (s == Shape::settings)
        {
            setTooltip ("Settings");
            icon = textDim();
        }
        else if (s == Shape::star)
        {
            setTooltip ("Favorite");
            // Favourite gold: app-flavoured constant kept for visual parity;
            // per-instance or theme overrides go through iconColourId.
            icon = juce::Colour (0xffe8c34a);
        }
        else if (s == Shape::wand)
        {
            setTooltip ("Menu");
            icon = accent();
        }
        else if (s == Shape::tool)
        {
            setTooltip ("Tools");
            icon = textDim();
        }
        else if (s == Shape::search)
        {
            setTooltip ("Reveal folder in Explorer");
            icon = textDim();
        }
        else if (s == Shape::lock)
        {
            setTooltip ("Link");
            icon = textDim();   // quiet grey; ON state is app-driven (alpha/colourId)
        }
        else if (s == Shape::download)
        {
            setTooltip ("Download");
            icon = accent();
        }
        else if (s == Shape::eye)
        {
            setTooltip ("Visible");
            icon = textDim();
        }
        else if (s == Shape::eyeOff)
        {
            setTooltip ("Hidden");
            icon = juce::Colour (0xffe8c34a);
        }
        else if (s == Shape::rewind)
        {
            setTooltip ("Back to start");
            icon = textDim();   // navigation, not the main action
        }
        else if (s == Shape::copy)
        {
            setTooltip ("Copy");
            icon = textDim();   // quiet: it acts on text already on screen
        }
        else if (s == Shape::sparkle)
        {
            setTooltip ("Generate with AI");
            icon = accent();     // the accent is what this button is FOR here
        }
        else if (s == Shape::zoomIn)
        {
            setTooltip ("Zoom in");
            icon = textDim();   // viewer toolbar: secondary, never the main action
        }
        else if (s == Shape::zoomOut)
        {
            setTooltip ("Zoom out");
            icon = textDim();
        }
        else if (s == Shape::chevronUp)
        {
            setTooltip ("Scroll up");
            icon = textDim();
        }
        else if (s == Shape::chevronDown)
        {
            setTooltip ("Scroll down");
            icon = textDim();
        }
        else if (s == Shape::chevronLeft)
        {
            setTooltip ("Previous");
            icon = textDim();
        }
        else if (s == Shape::chevronRight)
        {
            setTooltip ("Next");
            icon = textDim();
        }
        else if (s == Shape::grid)
        {
            setTooltip ("Grid");
            icon = textDim();   // the ON state is carried by the toggle, as elsewhere
        }
        else if (s == Shape::step)
        {
            setTooltip ("Step");
            icon = textDim();
        }
        else if (s == Shape::power)
        {
            setTooltip ("Power");
            icon = textDim();   // OFF is the quiet state; the host tints it on
        }
        else if (s == Shape::trash)
        {
            setTooltip ("Clear");
            icon = textDim();   // clear-all, NOT the red per-item delete cross
        }
        else if (s == Shape::save)
        {
            setTooltip ("Save");
            icon = textDim();
        }
        else
        {
            setTooltip ("Play");
        }
    }

    /** true -> Stop glyph on the Play shape (track is playing). The state
     *  rides the NATIVE toggle state of juce::Button. */
    void setPlayingState (bool nowPlaying)
    {
        if (getToggleState() == nowPlaying)
            return;
        setToggleState (nowPlaying, juce::dontSendNotification);
        repaint();
    }

    /** The glyph itself, changeable: a transport button is one control whose
     *  MEANING moves (play <-> pause) and the shape is the only thing that
     *  says which way it points. It repaints, because a glyph that moved
     *  without one is the "says one thing, does another" defect wearing
     *  pixels. */
    void setShape (Shape s)
    {
        if (s == shape)
            return;
        shape = s;
        repaint();
    }

    /** Icon colour (accent blue by default; grey for old runs, bright
        green for the playing line, red for Delete - see ctor defaults). */
    void setIconColour (juce::Colour c)
    {
        if (icon == c)
            return;
        icon = c;
        repaint();
    }

    /** star shape only: true = filled star (favourite), false = thin
     *  1.2 px outline (not a favourite). Repaints locally. */
    void setFilled (bool shouldBeFilled)
    {
        if (filled == shouldBeFilled)
            return;
        filled = shouldBeFilled;
        repaint();
    }

    /** FRAMED mode: draws a real button chrome around the icon - rounded
        bgDark fill + panelBorder outline, brighter on hover (accent 40%
        outline), darker when down, pointing-hand cursor. For standalone
        mini action buttons. */
    void setFramed (bool shouldBeFramed)
    {
        if (framed == shouldBeFramed)
            return;
        framed = shouldBeFramed;
        if (framed)
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    /** Shared glyph renderer: builds the fillable path for a shape inside
        `box` (already sized to the glyph area). DataList row actions use
        this so painted glyphs stay pixel-identical to IconButton widgets.
        `playingToggle` mirrors the native toggle state (play -> stop). */
    static juce::Path glyphPath (Shape s, juce::Rectangle<float> box,
                                 bool filledStar, bool playingToggle,
                                 float iconSide = 10.0f)
    {
        juce::Path p;
        if (s == Shape::cross)
        {
            const float r = iconSide * 0.5f;
            const auto c = box.getCentre();
            juce::Path x;
            x.startNewSubPath (c.x - r, c.y - r);
            x.lineTo (c.x + r, c.y + r);
            x.startNewSubPath (c.x + r, c.y - r);
            x.lineTo (c.x - r, c.y + r);
            juce::PathStrokeType (2.0f).createStrokedPath (p, x);
        }
        else if (const char* glyph = svgFor (s))
        {
            const float scale = iconSide * 1.30f / 24.0f;
            p.addPath (svgOutline (glyph),
                       juce::AffineTransform::translation (-12.0f, -12.0f)
                           .scaled (scale)
                           .translated (box.getCentreX(), box.getCentreY()));
        }
        else if (s == Shape::folder)
        {
            p.startNewSubPath (box.getX(),       box.getY() + 1.5f);
            p.lineTo (box.getX() + 3.8f,         box.getY() + 1.5f);
            p.lineTo (box.getX() + 5.4f,         box.getY() + 3.2f);
            p.lineTo (box.getRight(),            box.getY() + 3.2f);
            p.lineTo (box.getRight(),            box.getBottom());
            p.lineTo (box.getX(),                box.getBottom());
            p.closeSubPath();
        }
        else if (s == Shape::star)
        {
            const float sStar = filledStar ? iconSide * 1.18f : iconSide;
            const auto fb = box.withSizeKeepingCentre (sStar, sStar);
            const auto c = fb.getCentre();
            const float ro = sStar * 0.5f;
            const float ri = ro * 0.382f;
            const float pi = juce::MathConstants<float>::pi;
            for (int k = 0; k < 10; ++k)
            {
                const float ang = pi * -0.5f + k * pi / 5.0f;
                const float rad = (k % 2 == 0) ? ro : ri;
                const float x = c.x + rad * std::cos (ang);
                const float y = c.y + rad * std::sin (ang);
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            p.closeSubPath();
            if (! filledStar)
            {
                juce::Path stroked;
                juce::PathStrokeType (1.2f).createStrokedPath (stroked, p);
                p = stroked;
            }
        }
        else if (s == Shape::rewind)
        {
            // Transport "return to start": a bar on the left plus a
            // left-pointing triangle, drawn inside the glyph box.
            const float bar = iconSide * 0.18f;
            const float gap = iconSide * 0.12f;
            const float tri  = iconSide - bar - gap;
            const float h    = tri * 0.5f;
            const auto c     = box.getCentre();
            p.addRectangle (juce::Rectangle<float> (c.x - iconSide * 0.5f,
                                                    c.y - h, bar, tri));
            p.addTriangle (c.x + (tri - bar) * 0.5f, c.y - h,
                           c.x + (tri - bar) * 0.5f, c.y + h,
                           c.x - gap - bar, c.y);
        }
        else if (s == Shape::sparkle)
        {
            // The AI glyph: a big four-pointed star with CONCAVE sides, plus a
            // smaller companion down-right. Drawn procedurally like star, not
            // from an SVG, so it scales without a parse and stays a fillable
            // outline the caller can recolour.
            const float r  = iconSide * 0.5f;
            const auto  c  = box.getCentre();

            auto one = [&] (juce::Point<float> ctr, float rad)
            {
                const float rIn = rad * 0.17f;
                p.startNewSubPath (ctr.x, ctr.y - rad);
                p.quadraticTo (ctr.x + rIn, ctr.y - rIn, ctr.x + rad, ctr.y);
                p.quadraticTo (ctr.x + rIn, ctr.y + rIn, ctr.x, ctr.y + rad);
                p.quadraticTo (ctr.x - rIn, ctr.y + rIn, ctr.x - rad, ctr.y);
                p.quadraticTo (ctr.x - rIn, ctr.y - rIn, ctr.x, ctr.y - rad);
                p.closeSubPath();
            };

            one (c - juce::Point<float> (r * 0.32f, r * 0.30f), r * 0.95f);
            one (c + juce::Point<float> (r * 0.52f, r * 0.46f), r * 0.46f);
        }
        else if (s == Shape::pause)
        {
            // Two bars with a gap between them: the universal pause, and
            // deliberately not the stop square - a transport that KEEPS its
            // position must not wear the glyph that says it threw it away.
            const float bar = iconSide * 0.27f;
            const float gap = iconSide * 0.18f;
            const float h   = iconSide * 0.88f;
            const float r   = bar * 0.25f;
            const float x0  = box.getCentreX() - gap * 0.5f - bar;
            const float y0  = box.getCentreY() - h * 0.5f;
            p.addRoundedRectangle (x0, y0, bar, h, r);
            p.addRoundedRectangle (x0 + bar + gap, y0, bar, h, r);
        }
        else if (s == Shape::stop || playingToggle)
        {
            p.addRectangle (
                box.withSizeKeepingCentre (iconSide - 1.5f, iconSide - 1.5f));
        }
        else
        {
            p.addTriangle (
                box.getX(), box.getY(),
                box.getX(), box.getBottom(),
                box.getRight(), box.getCentreY());
        }
        return p;
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        if (framed && isEnabled())          // real button affordance
        {
            auto frame   = getLocalBounds().toFloat().reduced (1.0f);
            auto base    = resolved (frameFillColourId,    bgDark());
            auto outline = resolved (frameOutlineColourId, panelBorder());
            if (down)
            {
                base    = base.darker (0.08f);
                outline = outline.brighter (0.08f);
            }
            else if (over)
            {
                base    = base.brighter (0.07f);
                outline = accent().withAlpha (0.40f);
            }
            g.setColour (base);
            g.fillRoundedRectangle (frame, 5.0f);
            g.setColour (outline);
            g.drawRoundedRectangle (frame, 5.0f, 1.0f);
        }
        else if ((over || down) && isEnabled()) // quiet hover backdrop
        {
            // A BLUE hover, not a grey one: grey reads as disabled, and an
            // affordance that looks dead while the pointer is on it teaches
            // the user that it is dead. The semantic colours (a red delete,
            // gold favourite) keep their own ink, so the accent is the
            // backdrop and never fights the glyph.
            const auto hue = shape == Shape::cross
                                 ? danger() : accent();
            g.setColour (hue.withAlpha (down ? 0.28f : 0.16f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        }

        auto colour = resolved (iconColourId, icon);
        if (! isEnabled())
        {
            // Custom paint bypasses the framework's alpha pass; grey the
            // glyph explicitly so disabled transport is obvious.
            colour = textDim().withAlpha (0.35f);
        }
        else if (down)
            colour = colour.brighter (0.30f);
        else if (over && semanticInk())
            colour = colour.brighter (0.20f);
        else if (over)
            // A neutral glyph BLUE on hover: the same accent the framed mode
            // outlines with, so a quiet icon and a button share one language.
            colour = accent();
        g.setColour (colour);

        // A TALL button scales its glyph with it. The karaoke player's
        // transport row asks for buttons the height of the waveform
        // (44 px), and a 10 px glyph in a 44 px frame reads as an empty
        // box with a dot in it. The ramp starts ABOVE the classic
        // 20..30 px sizes, which keep the exact 10 px they have always
        // drawn: a threshold, not a slope, so no existing host's pixels
        // move.
        float s = 10.0f;
        if (const float h = (float) getHeight(); h >= 36.0f)
            s = juce::jlimit (10.0f, 18.0f, h * 0.42f);
        const auto box = getLocalBounds().toFloat()
                             .withSizeKeepingCentre (s, s);
        juce::Path p = glyphPath (shape, box, filled, getToggleState(), s);
        g.fillPath (p);
    }

    void enablementChanged() override
    {
        repaint();
    }

private:
    /** Colour resolution order for every widget ColourId:
        1. per-instance setColour() override (standard JUCE),
        2. theme-level hook ebs::LookAndFeel::widgetThemeColour(),
        3. built-in default (shape-aware palette constant). */
    juce::Colour resolved (int id, juce::Colour builtin) const
    {
        if (isColourSpecified (id))
            return findColour (id);
        if (auto* l = dynamic_cast<LookAndFeel*> (&getLookAndFeel()))
            if (juce::Colour themed = l->widgetThemeColour (id);
                ! themed.isTransparent())
                return themed;
        return builtin;
    }

    /** false for the shapes whose ink already MEANS something (a red delete,
        a gold favourite, a golden hidden eye): those keep their own hue on
        hover, because switching them to the accent would drop the meaning.
        Everything else is neutral, and turns accent blue. */
    bool semanticInk() const noexcept
    {
        return shape == Shape::cross || shape == Shape::star
            || shape == Shape::eyeOff;
    }

    /** SVG source string for a shape, or nullptr when the shape is still
     *  drawn procedurally (play/stop/cross/folder/star). */
    static const char* svgFor (Shape s)
    {
        switch (s)
        {
            case Shape::undo:     return kSvgUndo;
            case Shape::redo:     return kSvgRedo;
            case Shape::settings: return kSvgGear;
            case Shape::refresh:  return kSvgRefresh;
            case Shape::wand:     return kSvgWand;
            case Shape::grip:     return kSvgHand;
            case Shape::tool:     return kSvgTool;
            case Shape::search:   return kSvgSearch;
            case Shape::lock:     return kSvgLock;
            case Shape::download: return kSvgDownload;
            case Shape::eye:      return kSvgEye;
            case Shape::eyeOff:   return kSvgEyeOff;
            case Shape::copy:     return kSvgCopy;
            case Shape::kebab:     return kSvgKebab;
            case Shape::music:     return kSvgMusic;
            case Shape::layers:    return kSvgLayers;
            case Shape::activity:  return kSvgActivity;
            case Shape::chat:      return kSvgChat;
            case Shape::mic:       return kSvgMic;
            case Shape::keys:      return kSvgKeys;
            case Shape::repeat:    return kSvgRepeat;
            case Shape::type:      return kSvgType;
            case Shape::image:     return kSvgImage;
            case Shape::film:      return kSvgFilm;
            case Shape::zoomIn:      return kSvgZoomIn;
            case Shape::zoomOut:     return kSvgZoomOut;
            case Shape::chevronUp:   return kSvgChevronUp;
            case Shape::chevronDown: return kSvgChevronDown;
            case Shape::chevronLeft: return kSvgChevronLeft;
            case Shape::chevronRight:return kSvgChevronRight;
            case Shape::grid:        return kSvgGrid;
            case Shape::step:        return kSvgStep;
            case Shape::power:       return kSvgPower;
            case Shape::trash:       return kSvgTrash;
            case Shape::save:        return kSvgSave;
            case Shape::expand:      return kSvgExpand;
            case Shape::compress:    return kSvgCompress;
            default:              return nullptr;
        }
    }

    /** Flatten an SVG source into a fillable outline path, parsed ONCE
     *  through JUCE's SVG engine and cached (static strings -> pointer
     *  identity is a stable key). The outline includes stroke thickness
     *  (DrawableShape::getOutlineAsPath returns the stroked outline). */
    static const juce::Path& svgOutline (const char* svgText)
    {
        static std::map<const char*, juce::Path> cache;
        auto it = cache.find (svgText);
        if (it == cache.end())
        {
            juce::Path outline;
            if (auto xml = juce::XmlDocument::parse (svgText))
                if (auto drawable = juce::Drawable::createFromSVG (*xml))
                    outline = drawable->getOutlineAsPath();
            it = cache.emplace (svgText, std::move (outline)).first;
        }
        return it->second;
    }

    Shape shape = Shape::play;
    juce::Colour icon { accent() };       // theme accent default
    bool filled = true;                   // star only: filled vs outline
    bool framed = false;                  // real-button chrome mode

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconButton)
};

} // namespace ebs
