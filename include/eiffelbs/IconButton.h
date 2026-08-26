// IconButton.h
// eiffelbs-ui - shared vector icon button (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from OpenTimbre source/ui/IconButton.h (original copyright
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
// secondary icons default to quiet textDim.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <map>

#include "eiffelbs/Theme.h"

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
static const char* const kSvgRefresh = R"(<svg viewBox="0 0 24 24" fill="#010101"><path d="M17.65 6.35C16.2 4.9 14.21 4 12 4c-4.42 0-7.99 3.58-7.99 8s3.57 8 7.99 8c3.73 0 6.84-2.55 7.73-6h-2.08c-.82 2.33-3.04 4-5.65 4-3.31 0-6-2.69-6-6s2.69-6 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z"/></svg>)";

static const char* const kSvgWand = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round"><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/></svg>)";

// Material "pan_tool" filled glyph: reads as a real HAND at small sizes
// (the previous 2x3 dot grid did not).
static const char* const kSvgHand = R"(<svg viewBox="0 0 24 24" fill="#010101"><path d="M23 5.5V20c0 2.2-1.8 4-4 4h-7.3c-1.08 0-2.1-.43-2.85-1.19L1 14.83s1.26-1.23 1.3-1.25c.22-.19.49-.29.79-.29.22 0 .42.06.6.16.04.01 4.31 2.46 4.31 2.46V4.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5V11h1V1.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5V11h1V2.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5V11h1V5.5c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5z"/></svg>)";


class IconButton : public juce::Button
{
public:
    /** Displayed shape: play = filled triangle (or stop square when
     *  toggled), stop = forced square, cross = delete cross, refresh =
     *  circular re-roll arrow, folder = folder silhouette, grip = drag
     *  handle, undo/redo = circular arrows, settings = gear, star =
     *  favourite marker (filled or thin outline, see setFilled). */
    enum class Shape { play, stop, cross, refresh, folder, grip,
                       undo, redo, settings, star, wand, tool, search };

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
            // overridable hook planned at the ColourIds milestone.
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

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        if (framed)                          // real button affordance
        {
            auto frame   = getLocalBounds().toFloat().reduced (1.0f);
            auto base    = bgDark();
            auto outline = panelBorder();
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
        else if (over || down)               // quiet hover backdrop
        {
            g.setColour (bgPanel());
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        }

        auto colour = icon;
        if (down)
            colour = colour.brighter (0.30f);
        else if (over)
            colour = colour.brighter (0.20f);
        g.setColour (colour);

        const float s = 10.0f;               // icon side
        const auto box = getLocalBounds().toFloat()
                             .withSizeKeepingCentre (s, s);
        juce::Path p;
        if (shape == Shape::cross)
        {
            const float r = s * 0.5f;
            const auto c = box.getCentre();
            juce::Path x;
            x.startNewSubPath (c.x - r, c.y - r);
            x.lineTo (c.x + r, c.y + r);
            x.startNewSubPath (c.x + r, c.y - r);
            x.lineTo (c.x - r, c.y + r);
            juce::PathStrokeType (2.0f).createStrokedPath (p, x);
        }
        else if (const char* glyph = svgFor (shape))
        {
            // Vector icon ported verbatim (see the SVG table at the top of
            // this file): scale the 24-unit viewBox around its centre into
            // the icon box and fill with the current colour. The outline
            // path already includes the stroke thickness.
            const float scale = s * 1.30f / 24.0f;
            p.addPath (svgOutline (glyph),
                       juce::AffineTransform::translation (-12.0f, -12.0f)
                           .scaled (scale)
                           .translated (box.getCentreX(), box.getCentreY()));
        }
        else if (shape == Shape::folder)
        {
            // Folder silhouette with its little tab, filled.
            p.startNewSubPath (box.getX(),       box.getY() + 1.5f);
            p.lineTo (box.getX() + 3.8f,         box.getY() + 1.5f);
            p.lineTo (box.getX() + 5.4f,         box.getY() + 3.2f);
            p.lineTo (box.getRight(),            box.getY() + 3.2f);
            p.lineTo (box.getRight(),            box.getBottom());
            p.lineTo (box.getX(),                box.getBottom());
            p.closeSubPath();
        }
        else if (shape == Shape::star)
        {
            // Standard five-pointed star: inner radius from the regular
            // pentagram ratio (~0.382). Favourites draw the glyph LARGER
            // so they clearly pop next to the plain grey ones.
            const float sStar = filled ? s * 1.18f : s;
            const auto fb = getLocalBounds().toFloat()
                                .withSizeKeepingCentre (sStar, sStar);
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
        }
        else if (shape == Shape::stop || getToggleState())
        {
            p.addRectangle (                 // solid square, slightly inset
                box.withSizeKeepingCentre (s - 1.5f, s - 1.5f));
        }
        else
        {
            p.addTriangle (                  // solid triangle pointing right
                box.getX(), box.getY(),
                box.getX(), box.getBottom(),
                box.getRight(), box.getCentreY());
        }
        g.fillPath (p);
    }

private:
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
