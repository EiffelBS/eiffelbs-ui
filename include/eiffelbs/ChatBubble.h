// ChatBubble.h
// eiffelbs-ui - one message of a chat transcript.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A chat transcript is not a text box with a word on top of each row. Three
// things make it read as a conversation, and each is a decision the component
// takes rather than the caller:
//
//   1. SIDE + AVATAR. The user's own text sits on the right behind a person
//      badge, the assistant's on the left behind a sparkle. A wall of
//      equal-width rows is a log, and a log has no speaker.
//   2. ITALIC THINKING. The model's reasoning is italic and lives in its own
//      COLLAPSIBLE block, so a trace never reads as the answer. The block
//      collapses because a reasoning model streams thousands of tokens of
//      trace, and a transcript that shows all of it buries the answer - which
//      is the whole point of the conversation.
//   3. COPY. Every answer carries a copy affordance, so "give me that text"
//      is one click instead of a selection that fights the wrapping.
//
// The height depends on the WIDTH the caller gives, so a bubble is measured
// before it is placed: requiredHeightForWidth() then setBounds(). The
// alternative (a fixed height) truncates a long answer, and a scrollbar
// inside a bubble is not a chat.
//
// The body is laid out with a juce::TextLayout rather than a nested
// TextEditor: the bubble owns its own metrics, and a streamed delta costs
// one layout of ONE message instead of a re-render of the whole transcript -
// which is what made an earlier transcript look frozen under a reasoning
// model streaming thousands of small deltas.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"
#include "eiffelbs/Avatar.h"

namespace ebs
{

class ChatBubble final : public juce::Component
{
public:
    /** Which speaker. Two sides, no third state. */
    enum class Side { user, assistant };

    /** What the message IS, so a failure and a note never read as an
     *  answer. plain = the answer or the question. */
    enum class Tone { plain, note, error };

    ChatBubble()
    {
        addAndMakeVisible (avatar);
        addChildComponent (copyButton);
        copyButton.onClick = [this] { if (onCopy != nullptr) onCopy(); };
    }

    // === content ==========================================================

    void setSide (Side s)
    {
        if (side == s)
            return;
        side = s;
        avatar.setWho (whoFor (side));
        resized();
        repaint();
    }

    Side getSide() const noexcept { return side; }

    void setTone (Tone t)
    {
        if (tone == t)
            return;
        tone = t;
        repaint();
    }

    /** Small label above the bubble ("Thinking", "Note", "Error"), so a
     *  one-shot note and the model's trace are never read as an answer. */
    void setHeader (const juce::String& h)
    {
        if (header == h)
            return;
        header = h;
        invalidate();
    }

    juce::String getHeader() const noexcept { return header; }

    /** true makes the header row a collapse control: a reasoning block is
     *  opened and closed, not read past. */
    void setCollapsible (bool shouldCollapse)
    {
        if (collapsible == shouldCollapse)
            return;
        collapsible = shouldCollapse;
        invalidate();
    }

    /** false hides the body and leaves the header pill. A collapsed block is
     *  still a header, never nothing. */
    void setExpanded (bool shouldExpand)
    {
        if (expanded == shouldExpand)
            return;
        expanded = shouldExpand;
        invalidate();
    }

    bool isExpanded() const noexcept { return expanded; }

    /** The reasoning is italic: the shape alone tells the reader it is the
     *  model's train of thought and not the answer to the question. */
    void setItalicBody (bool shouldBeItalic)
    {
        if (italic == shouldBeItalic)
            return;
        italic = shouldBeItalic;
        invalidate();
    }

    void setText (const juce::String& t)
    {
        if (text == t)
            return;
        text = t;
        invalidate();
    }

    /** One streamed delta. The body grows IN PLACE: a bubble per delta is a
     *  wall of bubbles, and one layout per delta of a growing message is
     *  still one message. */
    void appendText (const juce::String& delta)
    {
        if (delta.isEmpty())
            return;
        text << delta;
        invalidate();
    }

    juce::String getText() const noexcept { return text; }

    /** The copy affordance: shown for an answer, hidden for the question
     *  (nobody copies back what they just typed). */
    void setShowCopyButton (bool shouldShow)
    {
        if (showCopy == shouldShow)
            return;
        showCopy = shouldShow;
        copyButton.setVisible (shouldShow);
        invalidate();
    }

    std::function<void()> onCopy;
    std::function<void (bool)> onExpandedToggled;

    // === metrics ==========================================================

    /** Measure for a width, THEN place. The width is a PARAMETER, not a read
     *  of the current bounds: the caller measures for the width it is about
     *  to give, and reading getWidth() here would lay out for the width the
     *  bubble had last, which is the width of the previous message. */
    int requiredHeightForWidth (int w) const
    {
        auto* self = const_cast<ChatBubble*> (this);
        self->layOutForWidth (w);
        return self->height;
    }

    void resized() override
    {
        const int w = getWidth();
        avatar.setBounds (side == Side::assistant ? 0 : w - avatarSide, 0,
                          avatarSide, avatarSide);
        if (showCopy)
            copyButton.setBounds (0, juce::jmax (avatarSide,
                                                 height - copyRowH + 1),
                                  copyButtonSide, copyButtonSide);
    }

    void paint (juce::Graphics& g) override
    {
        const int w = getWidth();
        const auto b = getLocalBounds();
        if (b.isEmpty() || w < 8)
            return;

        const int bw = bubbleWidthFor (w);
        const int bx = bubbleLeftFor (w);
        int y = 0;

        if (header.isNotEmpty())
        {
            if (collapsible)
            {
                // The collapsed state IS the affordance: a pill naming what
                // is folded away, with a chevron pointing at its state.
                const int labelW = juce::roundToInt (
                    ebs::fontHint().getStringWidth (header));
                const int pillW = juce::jmin (w - avatarReserve(),
                                              labelW + chevronSide + pillPad * 2);
                const int px = side == Side::assistant
                                    ? avatarReserve()
                                    : w - avatarReserve() - pillW;
                headerPill = { px, 1, pillW, headerH - 2 };

                g.setColour (ebs::bgDark());
                g.fillRoundedRectangle (headerPill.toFloat(), 6.0f);
                g.setColour (ebs::panelBorder());
                g.drawRoundedRectangle (headerPill.toFloat(), 6.0f, 1.0f);

                const int cy = headerPill.getCentreY();
                const int cx = headerPill.getX() + pillPad;
                g.setColour (ebs::textDim());
                juce::Path chevron;
                if (expanded)
                {
                    chevron.addTriangle (cx - 3.0f, cy - 2.0f,
                                         cx + 3.0f, cy - 2.0f,
                                         cx,       cy + 2.0f);
                }
                else
                {
                    chevron.addTriangle (cx - 2.0f, cy - 3.0f,
                                         cx + 2.0f, (float) cy,
                                         cx - 2.0f, cy + 3.0f);
                }
                g.fillPath (chevron);
                g.setFont (ebs::fontHint());
                g.drawText (header, headerPill.getX() + chevronSide + pillPad,
                            headerPill.getY(),
                            headerPill.getWidth() - chevronSide - pillPad * 2,
                            headerPill.getHeight(),
                            juce::Justification::centredLeft, false);

                if (! expanded)
                    return;   // folded: the pill is the whole message
                y += headerH + 3;
            }
            else
            {
                g.setColour (tone == Tone::error ? ebs::danger()
                                                 : ebs::textDim());
                g.setFont (ebs::fontHint());
                g.drawText (header, bx, y, bw, headerH,
                            juce::Justification::centredLeft, false);
                y += headerH + 3;
            }
        }

        const auto bubble = juce::Rectangle<int> (bx, y, bw,
                                                  padV + bodyH + padV);

        auto fill = ebs::bgDark();
        auto outline = ebs::panelBorder();
        auto ink = ebs::text();
        if (side == Side::user)
        {
            fill    = ebs::accent().withAlpha (0.22f);
            outline = ebs::accent().withAlpha (0.45f);
        }
        else if (tone == Tone::note)
        {
            fill = ebs::bgPanel().brighter (0.06f);
        }
        else if (tone == Tone::error)
        {
            fill    = ebs::danger().withAlpha (0.20f);
            outline = ebs::danger().withAlpha (0.50f);
            ink     = ebs::danger().brighter (0.25f);
        }
        if (italic)
            ink = ebs::textDim();

        g.setColour (fill);
        g.fillRoundedRectangle (bubble.toFloat(), (float) corner);
        g.setColour (outline);
        g.drawRoundedRectangle (bubble.toFloat(), (float) corner, 1.0f);

        bodyLayout.draw (g, bubble.withTrimmedLeft (padH)
                                  .withTrimmedRight (padH)
                                  .withTrimmedTop (padV)
                                  .withTrimmedBottom (padV)
                                  .toFloat());
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool over = collapsible && header.isNotEmpty()
                       && headerPill.expanded (2, 2).contains (e.getPosition());
        setMouseCursor (over ? juce::MouseCursor::PointingHandCursor
                             : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! collapsible || header.isEmpty())
            return;
        if (! headerPill.expanded (2, 2).contains (e.getPosition()))
            return;
        setExpanded (! expanded);
        if (onExpandedToggled != nullptr)
            onExpandedToggled (expanded);
    }

private:
    static Avatar::Who whoFor (Side s) noexcept
    {
        return s == Side::user ? Avatar::Who::user : Avatar::Who::assistant;
    }

    /** The column the avatar occupies, on the side it is drawn. The bubble
     *  never runs under an avatar, so a badge is never text's backdrop. */
    static int avatarReserve() noexcept { return avatarSide + avatarGap; }

    /** The assistant's bubble uses the width it needs; the user's own bubble
     *  is narrower than the row, because a full-width block of one's own
     *  words reads as a section header and not as a message. */
    int bubbleWidthFor (int w) const
    {
        const int room = w - avatarReserve();
        if (side == Side::user)
            return juce::jmax (40, juce::jmin (room, w * 62 / 100));
        return juce::jmax (40, room);
    }

    int bubbleLeftFor (int w) const
    {
        return side == Side::assistant ? avatarReserve()
                                       : w - avatarReserve() - bubbleWidthFor (w);
    }

    /** Rebuild the measured layout for the width the caller is about to
     *  give. Nothing here touches the bounds: setBounds() comes from the
     *  caller and triggers resized(), which places the children. */
    void layOutForWidth (int w)
    {
        const int textW = juce::jmax (24, bubbleWidthFor (w) - padH * 2);
        juce::AttributedString as;
        as.append (text, ebs::fontHint(), ebs::text());
        as.setWordWrap (juce::AttributedString::byWord);
        bodyLayout.createLayout (as, (float) textW);
        bodyH = juce::roundToInt (bodyLayout.getHeight());

        height = 0;
        if (header.isNotEmpty())
        {
            height += headerH;
            if (collapsible && ! expanded)
            {
                height += 4;
                return;
            }
            height += 3;
        }
        height += padV + bodyH + padV;
        if (showCopy)
            height += copyRowH;
    }

    void invalidate()
    {
        layOutForWidth (getWidth());
        resized();
        repaint();
    }

    Side  side        = Side::assistant;
    Tone  tone        = Tone::plain;
    bool  showCopy    = false;
    bool  collapsible = false;
    bool  expanded    = true;
    bool  italic      = false;
    juce::String header, text;

    static constexpr int padH           = 9;
    static constexpr int padV           = 6;
    static constexpr int avatarSide     = 22;
    static constexpr int avatarGap      = 8;
    static constexpr int headerH        = 15;
    static constexpr int copyRowH       = 18;
    static constexpr int copyButtonSide = 15;
    static constexpr int corner         = 10;
    static constexpr int chevronSide    = 10;
    static constexpr int pillPad        = 5;

    juce::TextLayout bodyLayout;
    int bodyH  = 0;
    int height = 0;
    juce::Rectangle<int> headerPill;

    Avatar avatar;
    ebs::IconButton copyButton { ebs::IconButton::Shape::copy };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChatBubble)
};

} // namespace ebs
