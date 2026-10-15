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
//
// The backing also carries the caller's own opacity for the run, as `fade`:
// a host that fades its rows (a lyric roll dims the far ones) needs the
// backing to fade with them, or the contour becomes the last thing visible
// around a word that is nearly gone. `fade` multiplies OPACITY only - the
// stroke width stays the caller's `strength` - and it defaults to 1, so a
// host that fades nothing is byte-identical to before this parameter existed.

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
    below 0.05 are no-ops.

    `fade` (0..1, default 1) is the caller's own OPACITY for this run, and it
    multiplies the backing's ALPHA while leaving its WIDTH to `strength`. A
    roll fades its rows with the distance to the centre (the largest line at
    full opacity, the far ones dimmer), and a backing held at full strength
    under a faded word is the loudest thing on screen exactly where the text
    is quietest - a dark outline the eye reads as the word, around a word it
    can no longer read. Width stays the caller's: a contour that thinned as
    well as faded would stop being a contour on the far rows instead of
    becoming a quieter one.

    `contour` (default opaque black) tints the CONTOUR only: the shadow stays
    neutral black whatever it says, because a coloured blur is a glow, not a
    shadow. Its own alpha composes with the run's (an opaque pick behaves
    exactly like the historic black), and the designed 0xE6 base opacity is
    kept for every colour, so a host that passes nothing paints the same
    pixels as before this parameter existed. */
inline void drawReadabilityBacking (juce::Graphics& g, const juce::Path& glyphPath,
                                    const juce::Font& font, Readability mode,
                                    float strength, float fade = 1.0f,
                                    juce::Colour contour = juce::Colour (0xFF000000))
{
    const auto s = juce::jlimit (0.0f, 1.0f, strength);
    const auto f = juce::jlimit (0.0f, 1.0f, fade);
    // The opacity this run's backing is drawn with: `strength` (how strong
    // the reader asked for it) times `fade` (how present this ROW is), one
    // value, so the shadow and the contour cannot disagree about either.
    const auto alpha = s * f;
    if (mode == Readability::off || alpha <= 0.05f)
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
            juce::Colour (0xB0000000).withMultipliedAlpha (alpha),
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
        // The contour's opacity FOLLOWS THE FILL's (the caller's `fade`): the
        // rule the roll already applies to the words applies to what backs
        // them, so a row that is fading out takes its contour with it instead
        // of leaving an outline around nothing. Its WIDTH does not follow -
        // that is the caller's `strength`, and thinning it too would delete
        // the contour rather than quiet it.
        //
        // The tint keeps the historic 0xE6 base opacity explicitly: the old
        // line read `0xE6000000 times alpha`, so an opaque black must land on
        // exactly that - and an opaque pick of any hue lands beside it, same
        // strength, same fade, only the hue moved.
        g.setColour (juce::Colour::fromRGB (contour.getRed(),
                                              contour.getGreen(),
                                              contour.getBlue())
                         .withMultipliedAlpha (0xE6 / 255.0f
                                               * contour.getFloatAlpha()
                                               * alpha));
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
