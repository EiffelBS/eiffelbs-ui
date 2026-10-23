// Fonts.h
// eiffelbs-ui - typeface helpers (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Adapted from OpenVoxTuner Source/ui/OVTFonts.h via the in-house
// app LookAndFeel's font section (original copyright (C) 2026 EiffelBS).
//
// No embedded typeface: use the system default sans-serif on macOS so the
// UI renders with native typography (San Francisco / Helvetica Neue). On
// Windows, hard-code "Segoe UI" because JUCE's getDefaultSansSerifFontName()
// resolves to "Verdana" (a much wider face), which would make all text look
// visibly larger than intended. On macOS, apply a 0.85 scale because San
// Francisco has a larger x-height than Segoe UI at the same nominal size.
//
// Only the GENERIC named sizes are provided here; application-specific ones
// (OVT's fontCents/fontMeter0/..., single-use overlay/player variants) were
// deliberately left behind in the apps.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ebs
{
#if JUCE_MAC
    static const juce::String kTypefaceFamily =
        juce::Font::getDefaultSansSerifFontName();
    static constexpr float kPlatformFontScale = 0.85f;
#else
    static const juce::String kTypefaceFamily = "Segoe UI";
    static constexpr float kPlatformFontScale = 1.0f;
#endif

    /** APP-WIDE type scale, multiplied into every named font helper (AND into
        the LookAndFeel fonts, because those helpers are what it returns).

        WHY A PROCESS-WIDE SCALE AND NOT PER-WIDGET SIZES: the design tokens
        below (13 pt body, 14 pt combo, ...) are measured against the design
        system's own screens; an app whose UI reads too small had to grow every
        token at once, or it ends up with one bigger surface and eleven
        unchanged ones - the exact "mixed sizes" complaint this hook fixes.
        The app sets it ONCE at start-up, before any component exists; the
        default is 1.0 so every other consumer of the design system keeps its
        shipped metrics. */
    inline float& uiTypeScaleRef() noexcept
    {
        static float scale = 1.0f;
        return scale;
    }

    /** Set the process-wide UI type scale (clamped to a sane 0.75..1.6).
        Call once at start-up, before creating components; setTypeScale is not
        a per-widget control and is not meant to change while a UI is live. */
    inline void setUiTypeScale (float scale) noexcept
    {
        uiTypeScaleRef() = juce::jlimit (0.75f, 1.6f, scale);
    }

    inline float uiTypeScale() noexcept { return uiTypeScaleRef(); }

    /** Create a font with the standard typeface.
        The (family, height, style) constructor guarantees the same typeface
        is used for bold and plain variants.
        @param size    font size in points
        @param bold    true for bold, false for regular */
    inline juce::Font createFont (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (kTypefaceFamily,
                            size * kPlatformFontScale * uiTypeScale(),
                            bold ? juce::Font::bold : juce::Font::plain));
    }

    /** Create a font at the EXACT given point size (no platform scale).
        Reserved for popup menus: JUCE's default menu font is a hard-coded
        17pt system font that must not shrink relative to its hit areas.
        Using this for body text or tooltips would re-introduce the
        too-large-macOS-text problem. */
    inline juce::Font createFontRaw (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (kTypefaceFamily, size,
                            bold ? juce::Font::bold : juce::Font::plain));
    }

    /** Monospaced font (status dumps), scaled like the rest of the UI. */
    inline juce::Font createMonospaceFont (float size)
    {
        return juce::Font (juce::FontOptions (
            juce::Font::getDefaultMonospacedFontName(),
            size * kPlatformFontScale * uiTypeScale(), juce::Font::plain));
    }

    // === Named font constants for common sizes ===
    inline juce::Font fontTitle()          { return createFont (26.0f, true);  }
    inline juce::Font fontPanelTitle()     { return createFont (16.0f, true);  }
    inline juce::Font fontSectionLabel()   { return createFont (13.0f, true);  }
    inline juce::Font fontPlayerGroup()    { return createFont (12.0f, true);  }
inline juce::Font fontHint()           { return createFont (13.0f, false); }
    inline juce::Font fontBody()           { return createFont (13.0f, false); }
    inline juce::Font fontOverlayHeading() { return createFont (17.0f, true);  }
    inline juce::Font fontComboBox()       { return createFont (14.0f, false); }
    inline juce::Font fontTooltip()        { return createFont (13.0f, false); }
    inline juce::Font fontToggleButton()   { return createFont (14.0f, false); }

    inline juce::Font fontPopupMenu()
    {
        // Popup menus bypass the platform scale (see createFontRaw).
        return createFontRaw (17.0f, false);
    }
}
