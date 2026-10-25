// PillPainter.h
// eiffelbs-ui - the ONE painter behind every icon+label pill in the app.
//
// The author's rule (2026-10-26): every icon button of the Settings window must
// look the SAME pill - always the same chrome (fill + border, not a chrome that
// appears on hover only) - and the hover must be unmistakable. The hover now
// also changes the GLYPH colour by meaning: a destructive shape turns red,
// everything else turns accent. Both PillButton and IconTextButton delegate
// here, and the Models lists draw their row pills with it too, so the three
// surfaces cannot drift apart again.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"

namespace ebs
{

/** How a pill reads: neutral secondary action, primary action, or gated
    (readable but visibly not actionable). */
enum class PillTone { neutral, primary, gated };

/** Destructive shapes go red on hover: the pointer's feedback says what the
    click would destroy. */
inline bool pillShapeIsDestructive (IconButton::Shape shape)
{
    return shape == IconButton::Shape::trash
        || shape == IconButton::Shape::cross
        || shape == IconButton::Shape::stop;
}

/** Draw an icon+label pill.
    @param area    the button bounds (the pill insets by 1 px)
    @param iconPx  glyph side (IconButton glyph path is drawn at this size)
    @param over    pointer over the button
    @param down    button pressed
    @param enabled false = gated look (dim ink, no hover) */
inline void paintPillButton (juce::Graphics& g, juce::Rectangle<int> area,
                             IconButton::Shape shape,
                             const juce::String& label,
                             const juce::Font& font,
                             float iconPx, PillTone tone, bool over, bool down,
                             bool enabled)
{
    const auto b = area.toFloat().reduced (1.0f);
    const bool gated = ! enabled || tone == PillTone::gated;
    const bool primary = tone == PillTone::primary;
    const bool destructive = pillShapeIsDestructive (shape);

    // Chrome: ALWAYS a fill and a border (uniform look). Hover brightens the
    // border and tints the fill; primary carries a permanent accent tint.
    juce::Colour fill = bgDark();
    juce::Colour border = panelBorder();
    if (! gated)
    {
        if (down)          fill = accent().withAlpha (0.30f);
        else if (over)     fill = (destructive ? danger() : accent())
                                       .withAlpha (0.18f);
        else if (primary)  fill = accent().withAlpha (0.10f);
        if (over)          border = destructive ? danger() : accent();
        else if (primary)  border = accent().withAlpha (0.75f);
    }
    g.setColour (fill);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    // Glyph: dim at rest, accent (or RED for a destructive shape) on hover.
    juce::Colour glyphInk = gated ? textDim().withAlpha (0.55f)
                         : ! enabled ? textDim().withAlpha (0.4f)
                         : over ? (destructive ? danger() : accent())
                         : primary ? accent() : textDim();
    auto row = area.reduced (7, 0);
    const int iconW = (int) std::ceil (iconPx * 1.30f);
    auto iconBox = row.removeFromLeft (iconW).toFloat();
    g.setColour (glyphInk);
    g.fillPath (IconButton::glyphPath (shape,
                                       iconBox.withSizeKeepingCentre (iconPx,
                                                                      iconPx),
                                       false, false, iconPx));
    row.removeFromLeft (4);

    // Label: brighter on hover, dim when gated.
    const juce::Colour textInk = gated ? textDim().withAlpha (0.6f)
                               : over ? ebs::text() : textDim();
    g.setColour (textInk);
    g.setFont (font);
    g.drawFittedText (label, row, juce::Justification::centredLeft, 1);
}

} // namespace ebs
