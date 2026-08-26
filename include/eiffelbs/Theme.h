// Theme.h
// eiffelbs-ui - shared theme palette (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Adapted from OpenTimbre source/ui/OTTheme.h, itself adapted from
// OpenVoxTuner Source/ui/OVTTheme.h (original copyright (C) 2026 EiffelBS).
//
// Both palettes (Dark and Light) are self-contained inline functions, so
// both themes ship; the active one is selected through currentTheme() and
// defaults to Dark. Switch themes by assigning currentTheme(); the
// LookAndFeel picks the colours up through refreshThemeColours().

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ebs
{
    /** Theme mode enumeration. */
    enum class Theme { Dark, Light };

    /** Get/set the current active theme (thread-safe for UI thread). */
    inline Theme& currentTheme()
    {
        static Theme theme = Theme::Dark;
        return theme;
    }

    /** Check if the current theme is dark. */
    inline bool isDark() { return currentTheme() == Theme::Dark; }

    // === Theme colour palette ===
    // Index 0 = Dark, Index 1 = Light.

    /** Main background (deepest layer: window, tab pages, forms). */
    inline juce::Colour bgDark()
    {
        return isDark() ? juce::Colour::fromString ("#FF26282B")
                        : juce::Colour::fromString ("#FFF0F1F5");
    }

    /** Panel background (cards, blocks, framed groups). */
    inline juce::Colour bgPanel()
    {
        return isDark() ? juce::Colour::fromString ("#FF373A3E")
                        : juce::Colour::fromString ("#FFE8E9ED");
    }

    /** Accent colour (primary interactive elements: titles, arrows, checks). */
    inline juce::Colour accent()
    {
        return isDark() ? juce::Colour::fromString ("#FF1A9AF0")
                        : juce::Colour::fromString ("#FF1565C0");
    }

    /** Soft accent (selection backgrounds, subtle highlights). */
    inline juce::Colour accentSoft()
    {
        return isDark() ? juce::Colour::fromString ("#401A9AF0")
                        : juce::Colour::fromString ("#301565C0");
    }

    /** Primary text colour. */
    inline juce::Colour text()
    {
        return isDark() ? juce::Colour::fromString ("#FFE1E1E6")
                        : juce::Colour::fromString ("#FF2C2C34");
    }

    /** Secondary/dimmed text (hints, older runs, disabled states). */
    inline juce::Colour textDim()
    {
        return isDark() ? juce::Colour (0xff868686)
                        : juce::Colour (0xff666666);
    }

    /** Visualizer background (waveform previews): always dark so traces
        read identically on any theme. Shared constant for now - it becomes
        an overridable hook at the ColourIds milestone. */
    inline juce::Colour vizBg()
    {
        return juce::Colour (0xff15151b);
    }

    /** Header accent line (thin decorative rule under headers / tab bars). */
    inline juce::Colour headerAccent()
    {
        return isDark() ? juce::Colour (0x331A9AF0)
                        : juce::Colour (0x331565C0);
    }

    /** Border colour derived from the panel background: card edges, group
        boxes and input outlines (replaces old hard-coded greys). */
    inline juce::Colour panelBorder()
    {
        return isDark() ? bgPanel().brighter (0.08f)
                        : bgPanel().darker (0.12f);
    }

    /** Playback green ("already played" waveform part, playing icon).
        Shared app constant kept here because widgets reference it;
        hook planned at the ColourIds milestone. */
    inline juce::Colour outputColour()
    {
        return juce::Colour (0xff00e676);
    }

    /** Destructive action red (delete crosses). */
    inline juce::Colour danger()
    {
        return juce::Colour (0xffe5484d);
    }

    // === Viz chrome ======================================================
    //
    // Fixed (theme-independent) accents for always-dark visualization
    // surfaces: waveform displays, pitch curve editor, CPU meter. Like
    // vizBg()/outputColour()/danger() above they do not follow Dark/Light -
    // these surfaces keep their dark look under either app theme so traces
    // read identically everywhere. Values carried over verbatim from the
    // OpenVoxTuner palette (original copyright (C) 2026 EiffelBS).

    /** Grid lines drawn over a dark viz background (waveform mirrors). */
    inline juce::Colour grid()
    {
        return juce::Colour (0x20ffffff);
    }

    /** Fainter horizontal lines marking scale notes / lanes. */
    inline juce::Colour scaleLine()
    {
        return juce::Colour (0x10ffffff);
    }

    /** Ruler strip background of the pitch curve editor. */
    inline juce::Colour rulerBg()
    {
        return juce::Colour (0xff1a1a1a);
    }

    /** Brighter grid lines of the curve editor plotting area. */
    inline juce::Colour curveGrid()
    {
        return juce::Colour (0x40ffffff);
    }

    /** Header band behind a visualizer title row. */
    inline juce::Colour vizHeaderBg()
    {
        return juce::Colour (0xff191b1e);
    }

    /** Accent line under a visualizer header (same tint as headerAccent
        at full strength, without the Dark/Light switch). */
    inline juce::Colour vizHeaderAccent()
    {
        return juce::Colour (0x331A9AF0);
    }

    /** Legend block background overlaying a visualizer. */
    inline juce::Colour vizLegendBg()
    {
        return juce::Colour (0xff191b1e);
    }

    /** CPU/performance meter well (visible on any app theme). */
    inline juce::Colour cpuBg()
    {
        return juce::Colour (0xff222230);
    }

    /** CPU/performance meter text and scale ticks. */
    inline juce::Colour cpuText()
    {
        return juce::Colours::white.withAlpha (0.9f);
    }
}
