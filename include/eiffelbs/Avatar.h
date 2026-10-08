// Avatar.h
// eiffelbs-ui - conversation avatar badge.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A chat transcript reads as a conversation only when the two speakers are
// distinguishable at a glance, before any word is read. One circular badge
// with one glyph per speaker: a person for the user, a sparkle for the
// assistant. The glyph is a filled vector path (no font glyph: a Unicode
// character renders differently across systems, and an emoji is not a design
// system), so the badge looks identical everywhere.
//
// The two fills are the two MEANINGS, not two decorations: accent for the
// assistant (the app's own voice) and the dimmed text colour for the user
// (the operator's own words). No third state - an avatar that changes shape
// per message is a decoration, not a speaker.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"

namespace ebs
{

class Avatar final : public juce::Component
{
public:
    /** Who is speaking. Two speakers, no third state. */
    enum class Who { user, assistant };

    Avatar() = default;

    void setWho (Who w)
    {
        if (who == w)
            return;
        who = w;
        repaint();
    }

    Who getWho() const noexcept { return who; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float side = juce::jmin (b.getWidth(), b.getHeight());
        if (side <= 1.0f)
            return;   // no badge is drawn at a zero size
        const auto circle = b.withSizeKeepingCentre (side, side);

        g.setColour (who == Who::assistant ? accent() : textDim());
        g.fillEllipse (circle);

        // Glyph area: the badge inside the badge, so the shape never touches
        // the circle edge on either theme.
        const auto glyph = circle.withSizeKeepingCentre (side * 0.62f,
                                                        side * 0.62f);
        g.setColour (juce::Colours::white);
        g.fillPath (glyphPath (who, glyph));
    }

private:
    /** One filled glyph per speaker, built for the box it is drawn in. */
    static juce::Path glyphPath (Who speaker, juce::Rectangle<float> box)
    {
        juce::Path p;
        const auto c = box.getCentre();

        if (speaker == Who::assistant)
        {
            // Four-point sparkle: two concave quadratics per axis. A filled
            // star of straight lines reads as a starburst; the curve through
            // the centre is what makes it read as a sparkle.
            const float r = box.getWidth() * 0.5f;
            p.startNewSubPath (c.x, c.y - r);
            p.quadraticTo (c.x, c.y, c.x + r, c.y);
            p.quadraticTo (c.x, c.y, c.x, c.y + r);
            p.quadraticTo (c.x, c.y, c.x - r, c.y);
            p.quadraticTo (c.x, c.y, c.x, c.y - r);
            p.closeSubPath();
            return p;
        }

        // Person: a head and the shoulders under it, both filled.
        const float headR = box.getWidth() * 0.22f;
        p.addEllipse (c.x - headR, box.getY(), headR * 2.0f, headR * 2.0f);

        const float shoulderTop = box.getY() + headR * 2.0f + headR * 0.35f;
        const float shoulderR    = box.getWidth() * 0.42f;
        p.startNewSubPath (c.x - shoulderR, box.getBottom());
        p.lineTo (c.x - shoulderR, shoulderTop + shoulderR * 0.5f);
        p.quadraticTo (c.x - shoulderR, shoulderTop, c.x, shoulderTop);
        p.quadraticTo (c.x + shoulderR, shoulderTop,
                       c.x + shoulderR, shoulderTop + shoulderR * 0.5f);
        p.lineTo (c.x + shoulderR, box.getBottom());
        p.closeSubPath();
        return p;
    }

    Who who = Who::assistant;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Avatar)
};

} // namespace ebs
