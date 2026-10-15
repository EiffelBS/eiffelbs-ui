// TextReadability.h
// eiffelbs-ui - a NEUTRAL backing that keeps text legible over ANY background
// (a video, an animated clip, a photo).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A host that composites its own text over moving pictures cannot rely on the
// text's own colour: white vanishes on a bright frame and black on a dark one,
// and an adaptive text colour FLICKERS as the background moves underneath it.
// The treatment therefore belongs to the BACKING, never to the fill: the fill
// carries the caller's meaning (an accent word, the word to place, a word
// already sung), and a contrast game played on the fill would turn one meaning
// into two.
//
// Four recipes, every one of them neutral black:
//
//   off            today's pixels, unchanged (the default, a no-op)
//   shadow         a soft drop shadow under the glyphs
//   outline        a dark contour around the glyphs (the subtitle look)
//   scrim          a soft dark band behind a focus line
//   outlineScrim   contour + band, for a busy, moving background
//
// Nothing here touches the fill. `drawReadabilityBacking()` paints the backing
// for an already-built glyph path and the caller then fills that same path
// with its own colour; `drawReadabilityScrim()` paints the band and is called
// BEFORE the text. `off` is a no-op, so a host that adds the library paints
// byte-identical pixels until it opts into a mode.

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace ebs
{

/** How a neutral backing keeps text legible over an arbitrary background.
    `off` is the default and changes nothing. */
enum class Readability
{
    off,
    shadow,
    outline,
    scrim,
    outlineScrim
};

/** Below this glyph height a backing stops helping: a contour turns the
    smallest rows into blobs and a blur turns them into grime. Small text is
    carried by the band, not by a halo - and the value is chosen so the NEXT
    line of a roll (the one a reader looks ahead to) still qualifies in a
    small window. */
inline constexpr float kReadabilityMinFontH = 16.0f;

inline bool readabilityHasContour (Readability r) noexcept
{
    return r == Readability::outline || r == Readability::outlineScrim;
}

inline bool readabilityHasShadow (Readability r) noexcept
{
    return r == Readability::shadow;
}

inline bool readabilityHasScrim (Readability r) noexcept
{
    return r == Readability::scrim || r == Readability::outlineScrim;
}

/** Contour stroke thickness, RELATIVE to the glyph height: a fixed pixel width
    is mush on the smallest rows and a hairline on the biggest. The stroke is
    centred on the glyph outline, so only half of it shows outside the letter -
    the effective contour is ~0.04 of the glyph, the discreet subtitle
    weight. */
inline float readabilityContourWidth (float fontH) noexcept
{
    return juce::jlimit (1.2f, 12.0f, fontH * 0.08f);
}

/** The glyph outline of ONE run of text, placed EXACTLY where
    juce::Graphics::drawText() places it: same curtailment, same
    justification - the very arrangement `drawText` builds behind the scenes
    (juce_GraphicsContext.cpp), so the backing and the fill cannot drift apart
    and any hit box measured on the same box stays true. */
inline juce::Path glyphPathForText (const juce::Font& font, const juce::String& text,
                                    juce::Rectangle<float> box,
                                    juce::Justification just, bool ellipses)
{
    juce::GlyphArrangement ga;
    ga.addCurtailedLineOfText (font, text, 0.0f, 0.0f, box.getWidth(), ellipses);
    ga.justifyGlyphs (0, ga.getNumGlyphs(), 0.0f, 0.0f,
                      box.getWidth(), box.getHeight(), just);
    juce::Path path;
    ga.createPath (path);
    path.applyTransform (juce::AffineTransform::translation (box.getX(), box.getY()));
    return path;
}

/** Draw the backing for ONE already-built glyph path (shadow and/or contour).
    The caller fills that same path afterwards: the backing is neutral, the
    fill carries the meaning. `strength` is 0..1; `off` and a strength at or
    below 0.05 are no-ops. */
inline void drawReadabilityBacking (juce::Graphics& g, const juce::Path& glyphPath,
                                    const juce::Font& font, Readability mode,
                                    float strength)
{
    const auto s = juce::jlimit (0.0f, 1.0f, strength);
    if (mode == Readability::off || s <= 0.05f)
        return;
    const auto fontH = font.getHeight();

    if (readabilityHasShadow (mode) && fontH >= kReadabilityMinFontH)
    {
        // A soft shadow, JUCE's own (a blurred single-channel copy of the
        // glyph path). The fill on top is opaque, so the shadow only shows
        // where the glyph does not cover it - separation without boxing the
        // letters. Gated by size like the contour: the blur allocates an image
        // per call and the smallest rows do not need it.
        juce::DropShadow shadow (
            juce::Colour (0xB0000000).withMultipliedAlpha (s),
            (int) juce::jlimit (2.0f, 8.0f, fontH * 0.10f),
            { 0, (int) juce::jmax (1.0f, fontH * 0.06f) });
        shadow.drawForPath (g, glyphPath);
    }

    if (readabilityHasContour (mode) && fontH >= kReadabilityMinFontH)
    {
        juce::Path outline;
        juce::PathStrokeType (readabilityContourWidth (fontH) * s,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded)
            .createStrokedPath (outline, glyphPath);
        // The contour's job is contrast, so the caller must NOT dim it with
        // the fill: a word faded to 0.4 would otherwise lose its backing
        // exactly where the background is busiest.
        g.setColour (juce::Colour (0xE6000000).withMultipliedAlpha (s));
        g.fillPath (outline);
    }
}

/** The soft dark band behind a focus line: the safety net that works on ANY
    background, including one no glyph treatment can tame. A three-stop
    gradient (clear - peak - clear) so the band has no hard edge of its own to
    read as a rectangle. Draw it BEFORE the text it backs. */
inline void drawReadabilityScrim (juce::Graphics& g, juce::Rectangle<float> bounds,
                                  float centreY, float strength)
{
    const auto s = juce::jlimit (0.0f, 1.0f, strength);
    if (s <= 0.05f)
        return;
    const float half = juce::jlimit (28.0f, bounds.getHeight() * 0.5f,
                                     bounds.getHeight() * 0.30f);
    const float top = juce::jmax (bounds.getY(), centreY - half);
    const float bottom = juce::jmin (bounds.getBottom(), centreY + half);
    if (bottom <= top + 2.0f)
        return;
    juce::ColourGradient grad (juce::Colours::black.withAlpha (0.0f), 0.0f, top,
                               juce::Colours::black.withAlpha (0.0f), 0.0f, bottom,
                               false);
    grad.addColour (0.5, juce::Colours::black.withAlpha (0.55f * s));
    g.setGradientFill (grad);
    g.fillRect (juce::Rectangle<float> (bounds.getX(), top,
                                        bounds.getWidth(), bottom - top));
}

} // namespace ebs
