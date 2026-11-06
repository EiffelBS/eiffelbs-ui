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
//   4. SELECTION. The body is real text, not a picture: dragging over it
//      selects it and Ctrl/Cmd+C copies exactly what the drag covered, as in
//      any chat app. The copy button stays for "give me all of it", but a
//      sentence of a long answer is now selectable in place instead of
//      forcing the whole message onto the clipboard.
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

#include <cmath>
#include <limits>
#include <memory>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"
#include "eiffelbs/Avatar.h"
#include "eiffelbs/MermaidDiagram.h"
#include "eiffelbs/MermaidFlowchart.h"
#include "eiffelbs/MermaidPie.h"
#include "eiffelbs/MermaidSequence.h"

namespace ebs
{

class ChatBubble final : public juce::Component
{
public:

    /** A clickable range in the rendered body: `start`/`end` are offsets in
        the PROCESSED string the TextLayout was built from (the Markdown
        markers are already gone). */
    struct LinkSpan
    {
        int start = 0, end = 0;
        juce::String url, label;
    };

    /** The size of one embedded image, in pixels, before it is drawn. */
    struct ImageBox
    {
        float width = 0.0f, height = 0.0f;
    };

    /** One non-text block drawn INSIDE the body: a Mermaid flowchart or an
        embedded image. The block is painted over a run of blank lines the
        Markdown emitter reserved for it, so the single TextLayout - and with
        it selection, links and copy - is untouched: the drawing lives in a
        hole in the text, not in a second layout that would need its own hit
        test. */
    struct VisualBlock
    {
        enum class Kind { diagram, image, audio };

        Kind kind = Kind::diagram;
        int textOffset = 0;      // where the reserved blank lines start
        int reservedLines = 0;   // how many blank lines were reserved
        float lineTop = 0.0f;    // resolved from the finished layout
        float width = 0.0f;      // drawing width (<= body width)
        float height = 0.0f;     // drawing height
        std::unique_ptr<ebs::MermaidDiagram> diagram;
        juce::String url, alt;   // images and audio (alt = title/name)
    };

    /** The state of one audio block's player, supplied by the host on every
        paint. The host owns the playback (the shared library has no audio
        device): `ready` once the file is local, `playing`, and the position
        and duration in seconds. */
    struct AudioState
    {
        bool ready = false;
        bool loading = false;
        bool playing = false;
        double position = 0.0;
        double duration = 0.0;
        juce::String error;
    };

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
        setWantsKeyboardFocus (true);   // Ctrl/Cmd+C must reach keyPressed()
    }

    // === content ==========================================================

    void setSide (Side s)
    {
        if (side == s)
            return;
        side = s;
        avatar.setWho (whoFor (side));
        measuredWidth = -1;   // the side changes the width, so the measure died
        invalidate();
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

    // === selection ========================================================
    //
    // The body is painted from a juce::TextLayout, not a TextEditor (see the
    // file header), so the selection itself is implemented here: the layout
    // already maps every glyph back to a character range, and this component
    // is the only object that knows where its body sits inside its bounds.
    //
    // Offsets index the PROCESSED body string - the Markdown markers are
    // already gone - which is exactly the text the reader sees and what a
    // copy must produce. A selection never spans two bubbles: a message is
    // the unit a chat copies, and a drag that leaves this component clamps to
    // its own first or last character.

    /** True while a non-empty range is highlighted. */
    bool hasTextSelection() const noexcept
    {
        return selAnchor >= 0 && selCaret >= 0 && selAnchor != selCaret;
    }

    /** The highlighted characters, in reading order. Empty when nothing is
        selected. */
    juce::String getSelectedText() const
    {
        if (! hasTextSelection())
            return {};
        return layoutText.substring (juce::jmin (selAnchor, selCaret),
                                     juce::jmax (selAnchor, selCaret));
    }

    /** The highlighted range as [start, end) offsets into the processed
        body. An empty range means nothing is selected. */
    juce::Range<int> getTextSelection() const
    {
        if (! hasTextSelection())
            return {};
        return { juce::jmin (selAnchor, selCaret),
                 juce::jmax (selAnchor, selCaret) };
    }

    /** Highlight [start, end) of the processed body. Out-of-range offsets are
        clamped, and an empty range clears the selection. A host uses this to
        restore a selection; a reader gets here with the mouse. */
    void setTextSelection (int start, int end)
    {
        const int n = layoutText.length();
        const int a = juce::jlimit (0, n, start);
        const int b = juce::jlimit (0, n, end);
        if (a == b)
        {
            clearTextSelection();
            return;
        }
        selAnchor = a;
        selCaret  = b;
        selecting = false;
        repaint();
    }

    /** Select the whole body, the way a third click does. */
    void selectAllText()
    {
        if (layoutText.isEmpty())
            return;
        selAnchor = 0;
        selCaret  = layoutText.length();
        selecting = false;
        repaint();
    }

    void clearTextSelection()
    {
        if (selAnchor < 0 && selCaret < 0)
            return;
        selAnchor = -1;
        selCaret  = -1;
        selecting = false;
        repaint();
    }

    /** Put the selected text on the system clipboard. True when there was
        something to copy. */
    bool copySelectedTextToClipboard()
    {
        const auto selected = getSelectedText();
        if (selected.isEmpty())
            return false;
        juce::SystemClipboard::copyTextToClipboard (selected);
        if (onSelectionCopied != nullptr)
            onSelectionCopied (selected);
        return true;
    }

    /** Fired after a SELECTION reaches the clipboard, so the host can log it
        or show a status line. The copy button keeps using onCopy. */
    std::function<void (const juce::String&)> onSelectionCopied;

    /** Host seams for images embedded by the Markdown. `getChatImage` returns
        the cached image for a URL (invalid when absent); `requestChatImage`
        asks the host to fetch it. The host must re-run the bubble's layout
        when the image arrives: the reserved height follows the image's real
        aspect ratio, so the transcript has to re-measure. */
    std::function<juce::Image (const juce::String& url)> getChatImage;
    std::function<void (const juce::String& url)> requestChatImage;

    /** Host seams for audio blocks. `getAudioState` must be cheap and runs on
        the message thread during paint; `toggleAudio` is called on a click
        (start / pause / resume is the host's policy, because it owns the
        transport). */
    std::function<AudioState (const juce::String& url)> getAudioState;
    std::function<void (const juce::String& url)> toggleAudio;

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

    /** The host's image cache changed (an image arrived, or a fetch ended):
        the reserved height follows the real aspect ratio, so the measure
        cache must die. The host calls this, then re-lays the transcript out. */
    void mediaCacheChanged()
    {
        measuredWidth = -1;
        invalidate();
    }

    /** Draw the rounded box around the message, or just the text on the
     *  panel's background. The USER keeps its chrome: an accent fill is what
     *  makes "what I said" legible back at a glance. The ASSISTANT's long
     *  answers drop it - the avatar and the side already name the speaker,
     *  a bordered box around a paragraph is decoration, and the fill is a
     *  rounded path per message per repaint, which is the one part of a long
     *  conversation that grows without bound. */
    void setShowBubble (bool shouldShow)
    {
        if (showBubble == shouldShow)
            return;
        showBubble = shouldShow;
        invalidate();
    }

    bool getShowBubble() const noexcept { return showBubble; }

    /** Gate seam: where the copy affordance sits, so the host can prove it
        follows the bubble's SIDE. */
    juce::Rectangle<int> copyBoundsForTest() const
    {
        return copyButton.getBounds();
    }

    std::function<void()> onCopy;
    std::function<void()> onHeightChanged;
    std::function<void (bool)> onExpandedToggled;

    /** Body font size in points. A transcript is READ, not skimmed: a long
     *  answer at a fixed size is unreadable on a 4K panel and huge on a
     *  laptop, so the caller owns the size and every bubble re-measures when
     *  it moves. The default is the theme's body font. */
    void setBodyFontSize (float points)
    {
        const auto clamped = juce::jlimit (minBodyPts, maxBodyPts, points);
        if (juce::exactlyEqual (bodyPts, clamped))
            return;
        bodyPts = clamped;
        invalidate();
    }

    float getBodyFontSize() const noexcept { return bodyPts; }

    /** The monospace font the body draws CODE and TABLES with. Sized from the
        EFFECTIVE body height: the body font already carries the app's type
        scale and the reader's zoom, so passing its height through the scaled
        ebs::createMonospaceFont() applied the scale a SECOND time - every
        table and code span came out ~25 % larger than the text around it on
        the app's default 1.25 UI scale. */
    static juce::Font monospaceFontForBody (float bodyHeight)
    {
        return juce::Font (juce::FontOptions (
            juce::Font::getDefaultMonospacedFontName(),
            juce::jmax (9.0f, bodyHeight * 0.92f), juce::Font::plain));
    }

    // === metrics ==========================================================

    /** Measure for a width, THEN place. The width is a PARAMETER, not a read
     *  of the current bounds: the caller measures for the width it is about
     *  to give, and reading getWidth() here would lay out for the width the
     *  bubble had last, which is the width of the previous message. */
    int requiredHeightForWidth (int w) const
    {
        auto* self = const_cast<ChatBubble*> (this);
        // Same inputs, same answer: the measure cache is what makes a
        // streamed delta O(1) instead of a re-layout of every message in the
        // conversation. A transcript of sixty bubbles measured per delta is
        // sixty text layouts per delta, and a streamed answer is a thousand
        // of them - which is exactly how an earlier transcript looked frozen
        // under a reasoning model.
        if (self->measuredWidth == w && self->measuredPts == bodyPts
            && self->measuredText == text && self->measuredExpanded == expanded
            && self->measuredShowCopy == showCopy
            && self->measuredHeader == header)
        {
            return self->height;
        }
        self->layOutForWidth (w);
        self->measuredWidth    = w;
        self->measuredPts      = bodyPts;
        self->measuredText     = text;
        self->measuredExpanded = expanded;
        self->measuredShowCopy  = showCopy;
        self->measuredHeader    = header;
        self->lastHeight = self->height;   // a measure is not a content change
        return self->height;
    }

    void resized() override
    {
        const int w = getWidth();
        avatar.setBounds (side == Side::assistant ? 0 : w - avatarSide, 0,
                          avatarSide, avatarSide);
        if (showCopy)
        {
            // The affordance belongs on ITS OWN side of the row: left under an
            // answer, RIGHT under the user's blue bubble (author report
            // 2026-10-28 - it sat under the wrong side of the question).
            const int copyX = side == Side::user ? juce::jmax (0, w - copyButtonSide)
                                                 : 0;
            copyButton.setBounds (copyX,
                                  juce::jmax (avatarSide,
                                              height - copyRowH + 1),
                                  copyButtonSide, copyButtonSide);
        }
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

        if (showBubble)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (bubble.toFloat(), (float) corner);
            g.setColour (outline);
            g.drawRoundedRectangle (bubble.toFloat(), (float) corner, 1.0f);
        }
        else
        {
            // No chrome: the answer sits on the panel's own background. A note
            // still needs a surface to read as a note rather than an answer,
            // so only the PLAIN assistant message gives its box up.
            if (tone == Tone::note)
            {
                g.setColour (fill);
                g.fillRoundedRectangle (bubble.toFloat(), (float) corner);
            }
        }

        const auto bodyArea = bodyAreaForWidth (w).toFloat();

        // The highlight goes UNDER the glyphs: a fill painted after the text
        // would tint the letters themselves, which is not what a selected
        // word looks like anywhere else.
        paintTextSelection (g, bodyArea);
        bodyLayout.draw (g, bodyArea);

        // The blocks are painted INTO the reserved holes of the text. They
        // come after the glyphs because a box/image is opaque: a drawing under
        // a paragraph that follows it would be painted over.
        for (const auto& block : visuals)
        {
            const auto target = visualRectFor (block, bodyArea);
            if (block.kind == VisualBlock::Kind::diagram
                && block.diagram != nullptr)
                block.diagram->paint (g, target);
            else if (block.kind == VisualBlock::Kind::image)
                paintImageBlock (g, block, target);
            else if (block.kind == VisualBlock::Kind::audio)
                paintAudioBlock (g, block, target);
        }
    }

    /** Where a visual block is drawn inside the body: horizontally centred,
        vertically at the line its hole starts on. */
    juce::Rectangle<float> visualRectFor (const VisualBlock& block,
                                          juce::Rectangle<float> bodyArea) const
    {
        return { bodyArea.getX()
                     + juce::jmax (0.0f,
                                   (bodyArea.getWidth() - block.width) * 0.5f),
                 bodyArea.getY() + block.lineTop,
                 juce::jmax (1.0f, block.width),
                 juce::jmax (1.0f, block.height) };
    }

    /** An embedded image, or its placeholder until the host has fetched it.
        A click opens the original: the picture is a document, not a link in
        the sentence, so it carries its own affordance. */
    void paintImageBlock (juce::Graphics& g, const VisualBlock& block,
                          juce::Rectangle<float> target) const
    {
        g.setColour (ebs::bgDark());
        g.fillRoundedRectangle (target, 6.0f);
        g.setColour (ebs::panelBorder());
        g.drawRoundedRectangle (target, 6.0f, 1.0f);

        const auto image = getChatImage != nullptr ? getChatImage (block.url)
                                                   : juce::Image();
        if (image.isValid())
        {
            g.saveState();
            juce::Path clip;
            clip.addRoundedRectangle (target.reduced (1.0f), 6.0f);
            g.reduceClipRegion (clip);
            g.drawImage (image, target,
                         juce::RectanglePlacement::stretchToFit, false);
            g.restoreState();
            return;
        }

        // Placeholder: the alt text and the fact that it is still coming.
        // A failed fetch keeps the same box; the alt is the only honest
        // description the message itself carried.
        const auto label = block.alt.isNotEmpty() ? block.alt
                                                  : juce::String ("image");
        g.setColour (ebs::textDim());
        g.setFont (ebs::fontHint());
        g.drawFittedText (label, target.reduced (10.0f).toNearestInt(),
                          juce::Justification::centred, 3);
    }

    /** The audio player: a play/pause disc, the clip's name, a progress bar
        and the clock. The HOST owns the state and the transport; this only
        asks for one and draws it, so a player costs the message no audio
        device of its own. */
    void paintAudioBlock (juce::Graphics& g, const VisualBlock& block,
                          juce::Rectangle<float> target) const
    {
        const auto state = getAudioState != nullptr ? getAudioState (block.url)
                                                    : AudioState();

        g.setColour (ebs::bgDark());
        g.fillRoundedRectangle (target, 6.0f);
        g.setColour (ebs::panelBorder());
        g.drawRoundedRectangle (target, 6.0f, 1.0f);

        auto area = target.reduced (8.0f, 6.0f);

        const auto diameter = area.getHeight();
        const auto button = area.removeFromLeft (diameter)
                                .withSizeKeepingCentre (diameter, diameter);
        g.setColour (state.ready ? ebs::accent()
                                 : ebs::panelBorder().brighter (0.25f));
        g.fillEllipse (button);
        g.setColour (ebs::bgPanel());

        if (state.playing)
        {
            auto bars = button.reduced (diameter * 0.32f);
            const auto barWidth = juce::jmax (2.0f, bars.getWidth() * 0.30f);
            g.fillRect (bars.removeFromLeft (barWidth));
            bars.removeFromLeft (juce::jmax (2.0f, bars.getWidth() * 0.4f));
            g.fillRect (bars.removeFromLeft (barWidth));
        }
        else
        {
            const auto centre = button.getCentre();
            const auto radius = diameter * 0.22f;
            juce::Path triangle;
            triangle.addTriangle (centre.x - radius * 0.7f, centre.y - radius,
                                  centre.x - radius * 0.7f, centre.y + radius,
                                  centre.x + radius * 1.1f, centre.y);
            g.fillPath (triangle);
        }

        area.removeFromLeft (8.0f);
        const auto title = block.alt.isNotEmpty() ? block.alt
                                                  : juce::String ("audio");
        g.setColour (ebs::text());
        g.setFont (ebs::fontHint());
        auto titleRow = area.removeFromTop (area.getHeight() * 0.55f);
        g.drawText (title, titleRow, juce::Justification::centredLeft, true);

        const auto duration = juce::jmax (0.0, state.duration);
        const auto position = juce::jlimit (0.0, duration, state.position);
        const auto fraction = duration > 0.0
                                  ? (float) (position / duration) : 0.0f;

        auto progress = area.reduced (0.0f, 2.0f);
        auto timeArea = progress.removeFromRight (86.0f);
        const auto barArea = progress.reduced (0.0f, progress.getHeight() * 0.32f);

        g.setColour (ebs::panelBorder());
        g.fillRoundedRectangle (barArea, 2.0f);
        if (state.ready)
        {
            g.setColour (ebs::accent());
            g.fillRoundedRectangle (barArea.withWidth (barArea.getWidth()
                                                       * fraction), 2.0f);
        }

        juce::String clock;
        if (! state.ready)
            clock = state.loading ? "loading..."
                                  : state.error.isNotEmpty() ? "unavailable"
                                                             : "click to play";
        else
            clock = formatClock (position) + " / " + formatClock (duration);

        g.setColour (ebs::textDim());
        g.setFont (ebs::fontHint());
        g.drawText (clock, timeArea, juce::Justification::centredRight, true);
    }

    /** The embedded image under `p`, or null. */
    const VisualBlock* imageBlockAt (juce::Point<int> p) const
    {
        const auto area = bodyAreaForWidth (getWidth());
        if (area.isEmpty())
            return nullptr;
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::image
                && visualRectFor (block, area).contains (p.toFloat()))
                return &block;
        return nullptr;
    }

    /** The audio block under `p`, or null. */
    const VisualBlock* audioBlockAt (juce::Point<int> p) const
    {
        const auto area = bodyAreaForWidth (getWidth());
        if (area.isEmpty())
            return nullptr;
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::audio
                && visualRectFor (block, area).contains (p.toFloat()))
                return &block;
        return nullptr;
    }

    /** The link under `p`, or null. Hit-tested against the LAYOUT's runs: a
        link was appended as its own accent run, and its range in the processed
        string is what marks it clickable. This is why a link in the sentence
        is clickable IN PLACE instead of being repeated in a footer. */
    const LinkSpan* linkAt (juce::Point<int> p) const
    {
        if (linkSpans.empty() || bodyH <= 0)
            return nullptr;
        const int w = getWidth();
        const int bw = bubbleWidthFor (w);
        const int bx = bubbleLeftFor (w);
        int y = 0;
        if (header.isNotEmpty())
        {
            if (collapsible && ! expanded)
                return nullptr;
            y += headerH + 3;
        }
        const juce::Rectangle<int> bodyArea (bx + padH, y + padV,
                                             bw - 2 * padH, bodyH);
        if (! bodyArea.contains (p))
            return nullptr;
        const float lx = (float) (p.x - bodyArea.getX());
        const float ly = (float) (p.y - bodyArea.getY());
        for (int i = 0; i < bodyLayout.getNumLines(); ++i)
        {
            auto& line = bodyLayout.getLine (i);
            if (! line.getLineBoundsY().contains (ly))
                continue;
            for (auto* run : line.runs)
            {
                const auto xs = run->getRunBoundsX();
                const juce::Range<float> runX (xs.getStart() + line.lineOrigin.x,
                                               xs.getEnd() + line.lineOrigin.x);
                if (! runX.contains (lx))
                    continue;
                for (const auto& span : linkSpans)
                    if (run->stringRange.intersects (juce::Range<int> (
                            span.start, span.end)))
                        return &span;
            }
        }
        return nullptr;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool overHeader = collapsible && header.isNotEmpty()
                             && headerPill.expanded (2, 2)
                                    .contains (e.getPosition());
        const bool overLink = linkAt (e.getPosition()) != nullptr;
        const bool overImage = imageBlockAt (e.getPosition()) != nullptr;
        const bool overAudio = audioBlockAt (e.getPosition()) != nullptr;
        // The I-beam is the affordance that says "this text is selectable",
        // exactly as it does in a browser. Without it a reader has no way to
        // know the body answers a drag rather than only the copy button.
        const bool overText = ! overLink && ! overHeader && ! overImage
                              && ! overAudio
                              && bodyAreaForWidth (getWidth())
                                     .contains (e.getPosition().toFloat());
        setMouseCursor (overLink || overHeader || overImage || overAudio
                            ? juce::MouseCursor::PointingHandCursor
                            : overText ? juce::MouseCursor::IBeamCursor
                                       : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // The keyboard follows a click on the text, as it follows a click on
        // a web page: Ctrl/Cmd+C is delivered to this component while it
        // holds the focus, and the prompt box takes it back when the reader
        // clicks there.
        grabKeyboardFocus();

        // The CONTEXT MENU comes first: a right-click on a link must offer
        // Copy, not take the reader out of the app to the browser.
        if (e.mods.isPopupMenu())
        {
            showSelectionMenu (e);
            return;
        }

        if (const auto* audio = audioBlockAt (e.getPosition()))
        {
            if (toggleAudio != nullptr)
                toggleAudio (audio->url);
            return;
        }

        if (const auto* image = imageBlockAt (e.getPosition()))
        {
            // The image is the document: a click opens the original. It has
            // no text range, so it is not a selection and not a link span.
            if (image->url.isNotEmpty())
                juce::URL (image->url).launchInDefaultBrowser();
            return;
        }

        if (const auto* link = linkAt (e.getPosition()))
        {
            if (link->url.isNotEmpty())
                juce::URL (link->url).launchInDefaultBrowser();
            return;
        }

        if (collapsible && header.isNotEmpty()
            && headerPill.expanded (2, 2).contains (e.getPosition()))
        {
            setExpanded (! expanded);
            if (onExpandedToggled != nullptr)
                onExpandedToggled (expanded);
            return;
        }

        // A second or third click belongs to mouseDoubleClick: JUCE calls
        // mouseDown BEFORE mouseUp and mouseDoubleClick, so starting a new
        // selection here would wipe the word the reader just asked for.
        if (e.getNumberOfClicks() >= 2)
            return;

        // The body is where the text is; the padding around it is not text.
        if (! bodyAreaForWidth (getWidth())
                 .contains (e.getPosition().toFloat()))
            return;

        const int at = indexForPoint (e.getPosition());

        if (e.mods.isShiftDown() && selAnchor >= 0)
        {
            // Shift+click extends the selection the reader already has, the
            // way every text surface works.
            selCaret = at;
            selecting = true;
            repaint();
            return;
        }

        selAnchor = at;
        selCaret  = at;
        selecting = true;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! selecting)
            return;
        const int at = indexForPoint (e.getPosition());
        if (at == selCaret)
            return;
        selCaret = at;
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! selecting)
            return;
        selecting = false;
        // A plain click leaves no highlight behind: the anchor and the caret
        // are the same character, so there is nothing to read and nothing to
        // copy - and a later Ctrl+C must not hand over a stale range.
        if (selAnchor == selCaret)
        {
            selAnchor = -1;
            selCaret  = -1;
        }
        repaint();
    }

    /** The second click selects the word under the pointer, the third the
        whole message - the standard browser/chat ladder. */
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (layoutText.isEmpty())
            return;
        const auto p = e.getPosition();
        if (collapsible && header.isNotEmpty()
            && headerPill.expanded (2, 2).contains (p))
            return;   // the pill is a control, not text
        if (! bodyAreaForWidth (getWidth()).contains (p.toFloat()))
            return;

        if (e.getNumberOfClicks() >= 3)
        {
            selectAllText();
            return;
        }

        const auto word = wordRangeAt (layoutText, indexForPoint (p));
        selAnchor = word.getStart();
        selCaret  = word.getEnd();
        selecting = false;
        repaint();
    }

    /** Ctrl/Cmd+C copies the selected text, Ctrl/Cmd+A selects the body and
        Escape drops the selection. The bubble takes the keyboard focus on a
        click, so these reach a reader who never touches the menu. */
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress (juce::KeyPress::escapeKey))
        {
            if (! hasTextSelection())
                return false;
            clearTextSelection();
            return true;
        }

        // JUCE reports the platform's command key through commandModifier
        // (Command on macOS, Ctrl elsewhere); ctrlModifier is accepted too so
        // the Windows key answers whichever accessor it arrives through.
        if (key == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)
            || key == juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0))
        {
            copySelectedTextToClipboard();
            return true;
        }
        if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)
            || key == juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0))
        {
            selectAllText();
            return true;
        }
        return false;
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
    /** Inline Markdown -> styled runs. Supported: **bold**, *italic*,
        `code`, [label](url). The URL itself is not printed (a weather answer's
        links would double the paragraph); the label is accented, which is the
        same affordance the rest of the app uses for a link. */
    static void appendInlineRuns (juce::AttributedString& as,
                                  const juce::String& line,
                                  juce::Font base, juce::Colour ink,
                                  juce::Font mono,
                                  int& pos,
                                  std::vector<LinkSpan>* spans = nullptr)
    {
        const auto styleOf = [] (juce::Font f, bool bold, bool italic)
        {
            int flags = 0;
            if (bold)   flags |= juce::Font::bold;
            if (italic) flags |= juce::Font::italic;
            f.setStyleFlags (flags);
            return f;
        };
        bool bold = false, italic = false;
        juce::String run;
        const auto flush = [&]
        {
            if (run.isEmpty())
                return;
            as.append (run, styleOf (base, bold, italic), ink);
            pos += run.length();
            run.clear();
        };
        for (int i = 0; i < line.length(); ++i)
        {
            const auto c = line[i];
            const auto next = i + 1 < line.length() ? line[i + 1] : juce::juce_wchar (0);
            // A BACKSLASH escapes the Markdown character that follows it:
            // "5 \* 3" means a literal star, not the start of italics.
            if (c == '\\' && i + 1 < line.length())
            {
                const auto escaped = line[i + 1];
                if (escaped == '*' || escaped == '_' || escaped == '`'
                    || escaped == '[' || escaped == ']' || escaped == '('
                    || escaped == ')' || escaped == '\\' || escaped == '|')
                {
                    run << escaped;
                    ++i;
                    continue;
                }
            }
            // BARE URLs: LLM answers print them without Markdown grammar.
            // They are shown as links (accent) and recorded so the bubble can
            // expose a clickable row (author request 2026-10-29).
            if ((c == 'h' || c == 'H') && line.substring (i).startsWithIgnoreCase ("http"))
            {
                const bool https = line.substring (i).startsWithIgnoreCase ("https://");
                if (https || line.substring (i).startsWithIgnoreCase ("http://"))
                {
                    int end = i;
                    while (end < line.length())
                    {
                        const auto ch = line[end];
                        if (ch == ' ' || ch == '\t' || ch == ')' || ch == ']'
                            || ch == '>' || ch == '"' || ch == '\n')
                            break;
                        ++end;
                    }
                    // Trailing sentence punctuation belongs to the sentence.
                    while (end > i && (line[end - 1] == '.' || line[end - 1] == ','
                                       || line[end - 1] == ';'))
                        --end;
                    const auto url = line.substring (i, end).trim();
                    if (url.length() > 8)
                    {
                        flush();
                        if (spans != nullptr)
                            spans->push_back ({ pos, pos + url.length(),
                                                url, url });
                        as.append (url, styleOf (base, bold, italic),
                                   ebs::accent());
                        pos += url.length();
                        i = end - 1;
                        continue;
                    }
                }
            }
            if (c == '`')
            {
                flush();
                int end = line.indexOfChar (i + 1, '`');
                if (end < 0) { run << c; continue; }
                {
                    const auto code = line.substring (i + 1, end);
                    as.append (code, mono,
                               ebs::accent().withMultipliedBrightness (1.1f));
                    pos += code.length();
                }
                i = end;
                continue;
            }
            if ((c == '*' || c == '_') && next == c)   // **bold** / __bold__
            {
                flush();
                bold = ! bold;
                ++i;
                continue;
            }
            if (c == '[')
            {
                const int close = line.indexOfChar (i + 1, ']');
                const int open  = close >= 0 ? line.indexOfChar (close + 1, '(')
                                             : -1;
                const int shut  = open >= 0 ? line.indexOfChar (open + 1, ')')
                                            : -1;
                if (close > i && open == close + 1 && shut > open)
                {
                    flush();
                    const auto label = line.substring (i + 1, close);
                    const auto url = line.substring (open + 1, shut).trim();
                    if (spans != nullptr)
                        spans->push_back ({ pos, pos + label.length(),
                                            url, label });
                    as.append (label, styleOf (base, bold, italic),
                               ebs::accent());
                    pos += label.length();
                    i = shut;
                    continue;
                }
            }
            if (c == '*' || c == '_')
            {
                // An underscore INSIDE a word is part of the identifier
                // (snake_case, file_name.py), not an emphasis marker: taking
                // it out turned a filename italic and deleted the underscore.
                if (c == '_'
                    && juce::CharacterFunctions::isLetterOrDigit (
                           i > 0 ? line[i - 1] : juce::juce_wchar (0))
                    && juce::CharacterFunctions::isLetterOrDigit (next))
                {
                    run << c;
                    continue;
                }
                flush();
                italic = ! italic;
                continue;
            }
            run << c;
        }
        flush();
    }

    /** How deep a list item is nested, in display levels. Two spaces are one
        level (the convention every Markdown writer uses), and the result is
        capped so a deeply indented answer does not run off the bubble. */
    static int listIndentLevel (const juce::String& line)
    {
        int spaces = 0;
        for (const auto c : line)
        {
            if (c == ' ')       ++spaces;
            else if (c == '\t') spaces += 2;
            else break;
        }
        return juce::jlimit (0, 3, spaces / 2);
    }

    /** One pipe row, split into its cells. `\|` is a literal bar, and empty
        cells are kept: a table with a blank cell must not lose its column. */
    static juce::StringArray splitTableRow (const juce::String& line)
    {
        auto body = line.trim();
        if (body.startsWithChar ('|'))
            body = body.substring (1);
        if (body.endsWithChar ('|'))
            body = body.dropLastCharacters (1);

        juce::StringArray cells;
        juce::String cell;
        for (int i = 0; i < body.length(); ++i)
        {
            const auto c = body[i];
            if (c == '\\' && i + 1 < body.length() && body[i + 1] == '|')
            {
                cell << '|';
                ++i;
                continue;
            }
            if (c == '|')
            {
                cells.add (cell.trim());
                cell.clear();
                continue;
            }
            cell << c;
        }
        cells.add (cell.trim());
        return cells;
    }

    /** Markdown has NO audio syntax. These are the spellings a model actually
        uses when it links a clip: a bare URL, a Markdown link to one, or an
        <audio src> tag. */
    static bool audioUrlLooksPlayable (const juce::String& url)
    {
        const auto clean = url.trim().upToFirstOccurrenceOf ("?", false, false)
                                 .upToFirstOccurrenceOf ("#", false, false)
                                 .toLowerCase();

        static const char* const extensions[] = {
            ".wav", ".mp3", ".ogg", ".oga", ".m4a", ".aac", ".flac",
            ".opus", ".aiff", ".aif", ".wma", ".mp4"
        };
        for (const auto* extension : extensions)
            if (clean.endsWith (extension))
                return true;
        return false;
    }

    static juce::String audioNameFromUrl (const juce::String& url)
    {
        const auto clean = url.upToFirstOccurrenceOf ("?", false, false)
                              .upToFirstOccurrenceOf ("#", false, false);
        const auto name = clean.fromLastOccurrenceOf ("/", false, false);
        return name.isNotEmpty() ? name : juce::String ("audio");
    }

    /** The value of `name` inside an HTML-ish tag, quoted or bare. */
    static juce::String attributeValue (const juce::String& tag,
                                        const juce::String& name)
    {
        const auto key = name + "=";
        int at = tag.indexOfIgnoreCase (key);
        if (at < 0)
            return {};
        at += key.length();
        if (at >= tag.length())
            return {};

        const auto quote = tag[at];
        if (quote == '"' || quote == '\'')
        {
            const int end = tag.indexOfChar (at + 1, quote);
            return end > at ? tag.substring (at + 1, end).trim()
                            : juce::String();
        }

        const int end = tag.indexOfChar (at, ' ');
        return tag.substring (at, end > at ? end : tag.length()).trim();
    }

    static juce::String formatClock (double seconds)
    {
        const int total = juce::jmax (0, juce::roundToInt (seconds));
        return juce::String (total / 60) + ":"
               + juce::String (total % 60).paddedLeft ('0', 2);
    }

    /** A line that is only `:?---+:?` cells: the separator that turns the
        previous row into a table header, and that carries each column's
        alignment (0 left, 1 centre, 2 right). A separator with NO bar is a
        horizontal rule instead - a single-column table is not worth taking
        the rule away from every answer. */
    static bool tableSeparator (const juce::String& line,
                                std::vector<int>& alignmentsOut)
    {
        const auto trimmed = line.trim();
        if (! trimmed.containsChar ('|'))
            return false;

        auto cells = splitTableRow (trimmed);
        if (cells.isEmpty())
            return false;

        std::vector<int> alignments;
        for (auto cell : cells)
        {
            cell = cell.trim();
            if (cell.isEmpty())
                return false;

            // GFM: `:---` left, `:---:` centre, `---:` right. A leading colon
            // alone is LEFT alignment - it is the side the text is pinned to,
            // not a centre mark.
            const bool leftColon = cell.startsWithChar (':');
            const bool rightColon = cell.endsWithChar (':');
            const int alignment = leftColon && rightColon ? 1
                                : rightColon              ? 2
                                                          : 0;
            if (leftColon == true)
                cell = cell.substring (1);
            if (rightColon)
                cell = cell.dropLastCharacters (1);
            if (cell.isEmpty())
                return false;
            for (const auto c : cell)
                if (c != '-')
                    return false;
            alignments.push_back (alignment);
        }

        if (alignments.empty())
            return false;
        alignmentsOut = std::move (alignments);
        return true;
    }

    /** The width the INLINE renderer gives this text, in the font it will draw
        it with. Measured through appendInlineRuns itself, then through the
        LAYOUT's own run extents - NOT AttributedString's bounding box, which
        is the INK and stops at the last glyph's shoulder, so padding computed
        from it came up a space short on a word ending in a flat stem. */
    static float inlineWidth (const juce::String& text, juce::Font base,
                              juce::Font mono)
    {
        juce::AttributedString tmp;
        int pos = 0;
        appendInlineRuns (tmp, text, base, ebs::text(), mono, pos, nullptr);

        juce::TextLayout layout;
        layout.createLayout (tmp, std::numeric_limits<float>::max());
        if (layout.getNumLines() == 0)
            return 0.0f;
        return layout.getLine (0).getLineBoundsX().getLength();
    }

    /** The whole body as Markdown-aware styled text. The BLOCK subset is the
        one LLM answers actually use: headings, bullets, numbered items, task
        boxes, quotes, fenced code, horizontal rules, pipe TABLES and Mermaid
        FLOWCHARTS. A table is drawn as a monospace block with padded columns
        and a rule under its header; a flowchart is handed to
        ebs::MermaidFlowchart and an embedded image to the host's cache, both
        painted over blank lines reserved in the layout, so a drawing between
        two paragraphs costs the text nothing. */
    static juce::AttributedString markdownBody (
        const juce::String& raw,
        juce::Font base,
        juce::Colour ink,
        std::vector<LinkSpan>& spansOut,
        float maxWidth,
        std::vector<VisualBlock>& blocksOut,
        const std::function<ImageBox (const juce::String&)>& imageBoxFor)
    {
        const auto mono = monospaceFontForBody (base.getHeight());
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        int pos = 0;
        const auto emit = [&] (const juce::String& t, juce::Font f,
                               juce::Colour c)
        {
            as.append (t, f, c);
            pos += t.length();
        };

        // The real line height of the body font: the blank lines that reserve
        // a drawing's space get their height from the LAST non-empty line's
        // metrics, so a measured value is the one to trust (a nominal 1.2x
        // under-reserves a face with a tight ascent+descent).
        juce::AttributedString probeText;
        probeText.append ("M\n", base);
        juce::TextLayout probe;
        probe.createLayout (probeText, maxWidth);
        const auto bodyLineHeight = juce::jmax (
            1.0f, probe.getNumLines() > 0
                      ? probe.getLine (0).getLineBoundsY().getLength()
                      : base.getHeight() * 1.2f);

        // A paragraph is CLOSED explicitly: consecutive source lines in one
        // paragraph are a soft break (a space), which is what a model that
        // wraps its answer at a fixed column means. Every block element
        // closes the open paragraph first, so a heading, a list or a table
        // never shares a line with the sentence above it.
        bool paragraphOpen = false;
        const auto closeParagraph = [&]
        {
            if (! paragraphOpen)
                return;
            emit ("\n", base, ink);
            paragraphOpen = false;
        };

        const auto lines = juce::StringArray::fromLines (raw);
        bool inFence = false;
        for (int n = 0; n < lines.size(); ++n)
        {
            auto line = lines[n];
            if (line.trimStart().startsWith ("```"))
            {
                closeParagraph();
                const auto info = line.trimStart().substring (3).trim();
                if (info.startsWithIgnoreCase ("mermaid"))
                {
                    // Collect the fence body and consume BOTH fences: a
                    // diagram must not leak its source into the transcript.
                    juce::StringArray body;
                    int closing = n + 1;
                    for (; closing < lines.size(); ++closing)
                    {
                        if (lines[closing].trimStart().startsWith ("```"))
                            break;
                        body.add (lines[closing]);
                    }
                    n = closing;   // the loop's ++n resumes after the fence

                    // The dialect is the FIRST meaningful line of the body,
                    // never a guess from its content: a renderer that accepts
                    // a source it does not understand draws a lie. Anything
                    // outside these three falls back to the source below.
                    std::unique_ptr<ebs::MermaidDiagram> diagram;
                    for (const auto& sourceLine : body)
                    {
                        const auto first = sourceLine.trim();
                        if (first.isEmpty() || first.startsWith ("%%"))
                            continue;
                        if (first.startsWithIgnoreCase ("flowchart")
                            || first.startsWithIgnoreCase ("graph"))
                            diagram = std::make_unique<ebs::MermaidFlowchart>();
                        else if (first.startsWithIgnoreCase ("pie"))
                            diagram = std::make_unique<ebs::MermaidPie>();
                        else if (first.startsWithIgnoreCase ("sequenceDiagram"))
                            diagram = std::make_unique<ebs::MermaidSequence>();
                        break;
                    }

                    const auto diagramFont = base.withHeight (
                        juce::jmax (9.0f, base.getHeight() * 0.85f));
                    if (diagram != nullptr
                        && diagram->layout (body.joinIntoString ("\n"), maxWidth,
                                            diagramFont))
                    {
                        // Reserve the drawing's height as BLANK LINES. One
                        // extra line covers a TextLayout that drops the final
                        // empty line; the drawing is anchored at the first
                        // reserved line's top.
                        const int reserved = juce::jmax (
                            1, (int) std::ceil (diagram->getHeight()
                                                / bodyLineHeight)) + 1;
                        VisualBlock block;
                        block.kind = VisualBlock::Kind::diagram;
                        block.textOffset = pos;
                        block.reservedLines = reserved;
                        block.width = (float) diagram->getWidth();
                        block.height = (float) diagram->getHeight();
                        block.diagram = std::move (diagram);
                        blocksOut.push_back (std::move (block));
                        emit (juce::String::repeatedString ("\n", reserved),
                              base, ink);
                        continue;
                    }

                    // Not a diagram these renderers understand: show the
                    // source as the code block it is, rather than inventing a
                    // picture.
                    for (const auto& codeLine : body)
                        emit (codeLine + "\n", mono, ebs::textDim());
                    continue;
                }
                inFence = ! inFence;
                continue;
            }
            if (inFence)
            {
                closeParagraph();
                emit (line + "\n", mono, ebs::textDim());
                continue;
            }

            auto trimmed = line.trim();
            if (trimmed.isEmpty())
            {
                closeParagraph();
                emit ("\n", base, ink);
                continue;
            }

            // ---- embedded image ---------------------------------------
            // A Markdown image on its own line is a MEDIA BLOCK. The host
            // loads http(s) images; the bubble reserves the space from the
            // image's real aspect ratio once it is cached, and a placeholder
            // box until then.
            if (trimmed.startsWith ("![") && trimmed.endsWithChar (')'))
            {
                const int altEnd = trimmed.indexOfChar (2, ']');
                const int urlOpen = altEnd >= 0
                                        ? trimmed.indexOfChar (altEnd + 1, '(')
                                        : -1;
                if (altEnd > 1 && urlOpen == altEnd + 1)
                {
                    auto url = trimmed.substring (urlOpen + 1,
                                                  trimmed.length() - 1).trim();
                    url = url.upToFirstOccurrenceOf (" ", false, false);
                    if (url.startsWithIgnoreCase ("http://")
                        || url.startsWithIgnoreCase ("https://"))
                    {
                        closeParagraph();
                        const auto box = imageBoxFor (url);
                        const int reserved = juce::jmax (
                            1, (int) std::ceil (box.height / bodyLineHeight))
                                                + 1;
                        VisualBlock block;
                        block.kind = VisualBlock::Kind::image;
                        block.textOffset = pos;
                        block.reservedLines = reserved;
                        block.width = box.width;
                        block.height = box.height;
                        block.url = url;
                        block.alt = trimmed.substring (2, altEnd).trim();
                        blocksOut.push_back (std::move (block));
                        emit (juce::String::repeatedString ("\n", reserved),
                              base, ink);
                        continue;
                    }
                }
            }

            // ---- audio link -------------------------------------------
            // The player is a fixed-height block; the host supplies its state
            // and owns the transport. Only http(s) URLs become blocks: a local
            // path is not a thing an untrusted answer gets to open.
            {
                juce::String audioUrl, audioTitle;
                if (trimmed.startsWithIgnoreCase ("http")
                    && audioUrlLooksPlayable (trimmed))
                {
                    audioUrl = trimmed;
                    audioTitle = audioNameFromUrl (audioUrl);
                }
                else if (trimmed.startsWithChar ('[')
                         && trimmed.endsWithChar (')'))
                {
                    const int close = trimmed.indexOfChar (1, ']');
                    const int open = close > 0
                                         ? trimmed.indexOfChar (close + 1, '(')
                                         : -1;
                    if (close > 1 && open == close + 1)
                    {
                        const auto url = trimmed
                                             .substring (open + 1,
                                                         trimmed.length() - 1)
                                             .trim();
                        if (audioUrlLooksPlayable (url))
                        {
                            audioUrl = url;
                            audioTitle = trimmed.substring (1, close).trim();
                            if (audioTitle.isEmpty())
                                audioTitle = audioNameFromUrl (audioUrl);
                        }
                    }
                }
                else if (trimmed.startsWithIgnoreCase ("<audio"))
                {
                    const auto url = attributeValue (trimmed, "src");
                    if (audioUrlLooksPlayable (url))
                    {
                        audioUrl = url;
                        audioTitle = audioNameFromUrl (audioUrl);
                    }
                }

                if (audioUrl.startsWithIgnoreCase ("http://")
                    || audioUrl.startsWithIgnoreCase ("https://"))
                {
                    closeParagraph();
                    const auto blockHeight = 46.0f;
                    const int reserved = juce::jmax (
                        1, (int) std::ceil (blockHeight / bodyLineHeight)) + 1;
                    VisualBlock block;
                    block.kind = VisualBlock::Kind::audio;
                    block.textOffset = pos;
                    block.reservedLines = reserved;
                    block.width = maxWidth;
                    block.height = blockHeight;
                    block.url = audioUrl;
                    block.alt = audioTitle;
                    blocksOut.push_back (std::move (block));
                    emit (juce::String::repeatedString ("\n", reserved),
                          base, ink);
                    continue;
                }
            }
            // ---- pipe tables -------------------------------------------
            // A table is the header row, the alignment separator, and the
            // body rows that follow. Every row is ONE monospace line: that is
            // what makes the columns line up, and the rule under the header
            // is what makes it read as a table instead of a paragraph that
            // happens to contain bars.
            if (trimmed.containsChar ('|') && n + 1 < lines.size())
            {
                std::vector<int> alignments;
                if (tableSeparator (lines[n + 1], alignments))
                {
                    closeParagraph();
                    const int columns = (int) alignments.size();
                    std::vector<juce::StringArray> rows;
                    rows.push_back (splitTableRow (line));

                    int rowIndex = n + 2;
                    for (; rowIndex < lines.size(); ++rowIndex)
                    {
                        const auto rowLine = lines[rowIndex];
                        if (rowLine.trim().isEmpty()
                            || ! rowLine.containsChar ('|'))
                            break;
                        rows.push_back (splitTableRow (rowLine));
                    }
                    n = rowIndex - 1;   // the loop's ++n resumes after the table

                    // Widths measured from the STYLED cells, once, in the
                    // mono font the rows are drawn with. The cells are drawn
                    // through the same inline renderer that was measured, so
                    // padding and text cannot disagree about a link or a bold
                    // word.
                    std::vector<float> widths ((size_t) columns, 0.0f);
                    std::vector<std::vector<float>> cellWidths (rows.size());
                    for (size_t r = 0; r < rows.size(); ++r)
                    {
                        cellWidths[r].assign ((size_t) columns, 0.0f);
                        for (int c = 0; c < columns; ++c)
                        {
                            const auto cell = c < rows[r].size()
                                                  ? rows[r][c] : juce::String();
                            const float w = inlineWidth (cell, mono, mono);
                            cellWidths[r][(size_t) c] = w;
                            widths[(size_t) c] = juce::jmax (widths[(size_t) c], w);
                        }
                    }
                    for (auto& width : widths)
                        width = juce::jmax (width,
                                            mono.getStringWidthFloat ("M"));

                    const auto spaceW = juce::jmax (
                        1.0f, mono.getStringWidthFloat (" "));
                    const auto dashChar = juce::String::charToString (0x2500);
                    const auto dashW = juce::jmax (
                        1.0f, mono.getStringWidthFloat (dashChar));

                    const auto emitRow = [&] (size_t rowIndex, juce::Colour rowInk)
                    {
                        const auto& row = rows[rowIndex];
                        for (int c = 0; c < columns; ++c)
                        {
                            const auto cell = c < row.size() ? row[c]
                                                             : juce::String();
                            const int needed = juce::jmax (
                                0, juce::roundToInt (
                                       (widths[(size_t) c]
                                        - cellWidths[rowIndex][(size_t) c])
                                       / spaceW));
                            int leftPad = 0;
                            if (alignments[(size_t) c] == 1)
                                leftPad = needed / 2;
                            else if (alignments[(size_t) c] == 2)
                                leftPad = needed;

                            if (c > 0)
                                emit (" " + juce::String::charToString (0x2502)
                                          + " ", mono, ebs::textDim());
                            if (leftPad > 0)
                                emit (juce::String::repeatedString (" ", leftPad),
                                      mono, rowInk);
                            appendInlineRuns (as, cell, mono, rowInk, mono,
                                              pos, &spansOut);
                            const int rightPad = needed - leftPad;
                            if (rightPad > 0)
                                emit (juce::String::repeatedString (" ", rightPad),
                                      mono, rowInk);
                        }
                        emit ("\n", mono, rowInk);
                    };

                    emitRow (0, ebs::accent());

                    // The rule under the header: its junction sits under the
                    // row's own column bar.
                    for (int c = 0; c < columns; ++c)
                    {
                        if (c > 0)
                            emit (juce::String::charToString (0x2500)
                                      + juce::String::charToString (0x253c)
                                      + juce::String::charToString (0x2500),
                                  mono, ebs::textDim());
                        emit (juce::String::repeatedString (
                                  dashChar,
                                  juce::jmax (1, juce::roundToInt (
                                         widths[(size_t) c] / dashW))),
                              mono, ebs::textDim());
                    }
                    emit ("\n", mono, ebs::textDim());

                    for (size_t r = 1; r < rows.size(); ++r)
                        emitRow (r, ink);
                    continue;
                }
            }
            if (trimmed == "---" || trimmed == "***" || trimmed == "___")
            {
                closeParagraph();
                emit (juce::String::repeatedString ("\u2500", 18) + "\n",
                      mono, ebs::textDim());
                continue;
            }
            if (trimmed.startsWith ("#"))
            {
                closeParagraph();
                int level = 0;
                while (level < trimmed.length() && trimmed[level] == '#')
                    ++level;
                auto title = trimmed.substring (level).trim();
                auto f = base;
                f.setHeight (juce::jmax (base.getHeight(),
                                         base.getHeight() + 3.0f
                                             - (float) level));
                f.setStyleFlags (juce::Font::bold);
                appendInlineRuns (as, title, f, ink, mono, pos, &spansOut);
                emit ("\n", f, ink);
                continue;
            }
            // A list item keeps its DEPTH: a nested bullet drawn at the same
            // left edge as its parent is not a nested list, it is a flat run
            // of bullets.
            const int indentLevel = listIndentLevel (line);
            const juce::String indentPrefix = juce::String::repeatedString (
                "  ", indentLevel);

            if (trimmed.startsWith ("> ") || trimmed == ">")
            {
                closeParagraph();
                emit (indentPrefix + juce::String::fromUTF8 ("\u258f "), mono,
                      ebs::textDim());
                appendInlineRuns (as,
                                  trimmed.length() > 2 ? trimmed.substring (2)
                                                       : juce::String(),
                                  base, ebs::textDim(), mono, pos, &spansOut);
                emit ("\n", base, ink);
                continue;
            }
            if (trimmed.startsWith ("- ") || trimmed.startsWith ("* ")
                || trimmed.startsWith ("+ "))
            {
                closeParagraph();
                auto content = trimmed.substring (2);
                // The glyph follows the DEPTH, the way a browser renders a
                // nested list: a level-two bullet drawn with the same disc as
                // its parent reads as a sibling, not as a child.
                juce::String marker = juce::String::fromUTF8 (
                    indentLevel == 0 ? "\u2022 "
                                     : indentLevel == 1 ? "\u25e6 " : "\u25aa ");
                // GFM task list: the box replaces the bullet, so a checklist
                // reads as a checklist instead of as a bullet that begins
                // with brackets.
                if (content.startsWith ("[ ] "))
                {
                    marker = juce::String::fromUTF8 ("\u2610 ");
                    content = content.substring (4);
                }
                else if (content.startsWith ("[x] ")
                         || content.startsWith ("[X] "))
                {
                    marker = juce::String::fromUTF8 ("\u2611 ");
                    content = content.substring (4);
                }
                emit (indentPrefix + marker, base, ebs::textDim());
                appendInlineRuns (as, content, base, ink, mono, pos, &spansOut);
                emit ("\n", base, ink);
                continue;
            }
            // Ordered list: keep the WRITER's number (a list may start at 3),
            // and put the text after it so a numbered answer does not read as
            // a paragraph that begins with a digit.
            {
                int digits = 0;
                while (digits < trimmed.length()
                       && juce::CharacterFunctions::isDigit (trimmed[digits]))
                    ++digits;
                if (digits > 0 && digits + 1 < trimmed.length()
                    && (trimmed[digits] == '.' || trimmed[digits] == ')')
                    && trimmed[digits + 1] == ' ')
                {
                    closeParagraph();
                    emit (indentPrefix + trimmed.substring (0, digits + 1) + " ",
                          base, ebs::textDim());
                    appendInlineRuns (as, trimmed.substring (digits + 2),
                                      base, ink, mono, pos, &spansOut);
                    emit ("\n", base, ink);
                    continue;
                }
            }

            // A soft break inside a paragraph is a SPACE: a model that wraps
            // its answer at a fixed column means one sentence, not one line
            // per sentence. The paragraph is closed by the next block element
            // (or by the end of the message), which emits the newline.
            if (paragraphOpen)
                emit (" ", base, ink);
            else
                paragraphOpen = true;
            appendInlineRuns (as, line, base, ink, mono, pos, &spansOut);
        }
        closeParagraph();
        return as;
    }

    /** The size to reserve for an embedded image. A cached image's aspect
        ratio is the truth; until it is here, a 16:9 placeholder and a fetch
        request. Capped so a tall phone screenshot does not eat the whole
        transcript, and never upscaled past the source's own pixels. */
    ImageBox imageBoxFor (const juce::String& url, float maxWidth)
    {
        const auto image = getChatImage != nullptr ? getChatImage (url)
                                                   : juce::Image();
        if (image.isValid() && image.getWidth() > 0 && image.getHeight() > 0)
        {
            float width = juce::jmin (maxWidth, (float) image.getWidth());
            float height = width * (float) image.getHeight()
                                 / (float) image.getWidth();
            const float maxHeight = 420.0f;
            if (height > maxHeight)
            {
                height = maxHeight;
                width = height * (float) image.getWidth()
                               / (float) image.getHeight();
            }
            return { juce::jmax (40.0f, width), juce::jmax (30.0f, height) };
        }

        if (requestChatImage != nullptr)
            requestChatImage (url);

        return { juce::jmax (40.0f, maxWidth),
                 juce::jmin (200.0f, maxWidth * 9.0f / 16.0f) };
    }

    /** The area the body text is painted into, in this component's space.
        paint(), the link hit test and the selection all use this ONE
        rectangle: a second copy of the offsets is how a highlight ends up
        drawn one padding away from the glyphs it is supposed to cover. */
    juce::Rectangle<float> bodyAreaForWidth (int w) const
    {
        if (w <= 0)
            return {};
        int y = 0;
        if (header.isNotEmpty())
        {
            if (collapsible && ! expanded)
                return {};   // folded: the pill is the whole message
            y += headerH + 3;
        }
        const int bw = bubbleWidthFor (w);
        const int bx = bubbleLeftFor (w);
        return { (float) (bx + padH), (float) (y + padV),
                 (float) juce::jmax (1, bw - padH * 2), (float) bodyH };
    }

    /** Which character of the processed body sits under a point, in this
        component's space. A point above the first line names its start, a
        point below the last line names its end. */
    int indexForPoint (juce::Point<int> p) const
    {
        if (bodyLayout.getNumLines() == 0 || layoutText.isEmpty())
            return 0;

        const auto area = bodyAreaForWidth (getWidth());
        if (area.isEmpty())
            return 0;

        const float x = (float) p.x - area.getX();
        const float y = (float) p.y - area.getY();

        // The line whose vertical span contains the point; a point in the
        // leading between two lines belongs to the upper one.
        int lineIndex = bodyLayout.getNumLines() - 1;
        for (int i = 0; i < bodyLayout.getNumLines(); ++i)
        {
            if (y <= bodyLayout.getLine (i).getLineBoundsY().getEnd())
            {
                lineIndex = i;
                break;
            }
        }

        const auto& line = bodyLayout.getLine (lineIndex);
        const float lx = x - line.lineOrigin.x;

        if (lx <= 0.0f)
            return juce::jlimit (0, layoutText.length(),
                                 line.stringRange.getStart());

        for (auto* run : line.runs)
        {
            const auto runBounds = run->getRunBoundsX();
            if (lx < runBounds.getStart())
                return juce::jlimit (0, layoutText.length(),
                                     run->stringRange.getStart());
            if (lx <= runBounds.getEnd() || run == line.runs.getLast())
            {
                const int glyphs = run->glyphs.size();
                for (int g = 0; g < glyphs; ++g)
                {
                    const auto& glyph = run->glyphs.getReference (g);
                    if (lx < glyph.anchor.x + glyph.width * 0.5f)
                        return juce::jlimit (
                            0, layoutText.length(),
                            run->stringRange.getStart()
                                + charOffsetInRun (*run, g));
                }
                return juce::jlimit (0, layoutText.length(),
                                     run->stringRange.getEnd());
            }
        }

        return juce::jlimit (0, layoutText.length(), line.stringRange.getEnd());
    }

    /** The word around a character offset. Letters, digits, the underscore
        and the apostrophe make a word; a double-click on whitespace selects
        the whitespace run, which is what a browser does. */
    static juce::Range<int> wordRangeAt (const juce::String& body, int index)
    {
        const int n = body.length();
        if (n == 0)
            return {};

        int at = juce::jlimit (0, n - 1, index);
        const auto isWord = [] (juce::juce_wchar c)
        {
            return juce::CharacterFunctions::isLetterOrDigit (c)
                   || c == '_' || c == '\'';
        };

        // A click just AFTER a word belongs to that word: double-clicking the
        // space that follows "answer" selects "answer", not the space.
        if (! isWord (body[at]) && at > 0 && isWord (body[at - 1]))
            --at;

        const bool wantWord = isWord (body[at]);
        int start = at;
        int end   = at + 1;
        while (start > 0 && isWord (body[start - 1]) == wantWord)
            --start;
        while (end < n && isWord (body[end]) == wantWord)
            ++end;
        return { start, end };
    }

    /** The right-click menu of the body: Copy (the selection, when there is
        one), Select All and Copy Message. The menu is how a reader who does
        not know the shortcut still gets the text out. */
    void showSelectionMenu (const juce::MouseEvent& e)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Copy", hasTextSelection());
        menu.addItem (2, "Select All");
        if (text.isNotEmpty())
        {
            menu.addSeparator();
            menu.addItem (3, "Copy Message");
        }

        juce::Component::SafePointer<ChatBubble> safe (this);
        menu.showMenuAsync (
            juce::PopupMenu::Options().withTargetScreenArea (
                juce::Rectangle<int> (e.getScreenPosition().x,
                                      e.getScreenPosition().y, 1, 1)),
            [safe] (int result)
            {
                if (safe == nullptr)
                    return;
                if (result == 1)
                    safe->copySelectedTextToClipboard();
                else if (result == 2)
                    safe->selectAllText();
                else if (result == 3)
                    juce::SystemClipboard::copyTextToClipboard (safe->text);
            });
    }

    /** Paint the highlight under the glyphs it covers. One rectangle per line
        is enough because a selection is a contiguous character range, and it
        avoids the seams a rectangle per glyph leaves at fractional widths. */
    void paintTextSelection (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        if (! hasTextSelection())
            return;

        const int start = juce::jmin (selAnchor, selCaret);
        const int end   = juce::jmax (selAnchor, selCaret);
        g.setColour (ebs::accent().withAlpha (0.30f));

        for (int i = 0; i < bodyLayout.getNumLines(); ++i)
        {
            const auto& line = bodyLayout.getLine (i);
            float minX = std::numeric_limits<float>::max();
            float maxX = -std::numeric_limits<float>::max();

            for (auto* run : line.runs)
            {
                const int glyphs = run->glyphs.size();
                for (int g = 0; g < glyphs; ++g)
                {
                    const int rangeStart = run->stringRange.getStart()
                                           + charOffsetInRun (*run, g);
                    const int rangeEnd = run->stringRange.getStart()
                                         + charOffsetInRun (*run, g + 1);
                    if (rangeEnd <= start || rangeStart >= end)
                        continue;
                    const auto& glyph = run->glyphs.getReference (g);
                    const float x = line.lineOrigin.x + glyph.anchor.x;
                    minX = juce::jmin (minX, x);
                    maxX = juce::jmax (maxX, x + glyph.width);
                }
            }

            if (maxX > minX)
            {
                const auto y = line.getLineBoundsY();
                g.fillRect (area.getX() + minX, area.getY() + y.getStart(),
                            maxX - minX, y.getLength());
            }
        }
    }

    /** The character span a glyph covers: a ligature is ONE glyph over several
        characters, so a run's glyphs are spread over its character range
        rather than counted one for one. */
    static int charOffsetInRun (const juce::TextLayout::Run& run, int glyph)
    {
        const int glyphs = run.glyphs.size();
        const int chars  = run.stringRange.getLength();
        if (glyphs <= 0)
            return 0;
        return juce::jlimit (0, chars,
                             juce::roundToInt ((double) glyph * (double) chars
                                               / (double) glyphs));
    }

    /** The layout was rebuilt (a delta, a resize, a font step): the offsets
        must still name characters that exist. A selection whose end is past
        the new text is dropped rather than clipped to the last word - the
        reader selected something that is no longer there. A selection ABOVE
        an appended delta keeps its range, exactly as a chat that grows below
        the highlight should. */
    void clampSelectionToText()
    {
        if (selAnchor < 0 || selCaret < 0)
            return;
        const int n = layoutText.length();
        if (selAnchor > n || selCaret > n)
        {
            selAnchor = -1;
            selCaret  = -1;
            selecting = false;
            return;
        }
    }

    void layOutForWidth (int w)
    {
        auto f = ebs::fontBody().withHeight (bodyPts);
        if (italic)
            f.setStyleFlags (juce::Font::italic);
        const int textW = juce::jmax (24, bubbleWidthFor (w) - padH * 2);
        linkSpans.clear();
        visuals.clear();
        const auto imageBox = [this, textW] (const juce::String& url)
        {
            return imageBoxFor (url, (float) textW);
        };
        auto as = markdownBody (text, f, italic ? ebs::textDim() : ebs::text(),
                                linkSpans, (float) textW, visuals, imageBox);
        // The processed string is what the layout draws and what a selection
        // indexes: keep exactly that one, or a copy would return the Markdown
        // markers the reader never saw.
        layoutText = as.getText();
        bodyLayout.createLayout (as, (float) textW);
        resolveVisualPlacements();
        clampSelectionToText();
        // CEIL, then two pixels of slack. roundToInt truncates, and at the
        // larger chat sizes the last line's descender ended up outside the
        // bubble: a fractional line height MUST round up, and the extra pixel
        // covers the layout's reported height not including the full glyph
        // extents (author report 2026-10-29).
        bodyH = (int) std::ceil (bodyLayout.getHeight()) + 2
                + (int) std::ceil (visualTailHeight);

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

    /** Anchor every diagram at the first blank line it reserved. The mapping
        exists only once the text has been laid out (the Markdown emitter runs
        before there are lines), so it is resolved here, once.

        A drawing at the END of a message has a special case: a TextLayout
        drops its TRAILING blank lines, so the reserved hole has no line to
        anchor to. Those blocks are stacked below the last line, and the
        bubble grows by exactly their height - the text above stays untouched
        and the copy gains no placeholder characters. */
    void resolveVisualPlacements()
    {
        visualTailHeight = 0.0f;
        float tailTop = bodyLayout.getHeight();

        for (auto& block : visuals)
        {
            // The reserved run is the TAIL of the message when nothing but
            // whitespace follows it. A TextLayout gives trailing blank lines
            // no height (only the interior ones), so a drawing at the end of a
            // message is stacked BELOW the last line and the bubble grows by
            // its height instead of trusting a hole that has no pixels.
            const auto rest = layoutText.substring (
                juce::jmin (layoutText.length(),
                            block.textOffset + block.reservedLines)).trim();
            const bool trailing = rest.isEmpty();

            bool anchored = false;
            if (! trailing)
            {
                for (int i = 0; i < bodyLayout.getNumLines(); ++i)
                {
                    const auto& line = bodyLayout.getLine (i);
                    if (line.stringRange.getStart() >= block.textOffset)
                    {
                        // A small breath between the paragraph and the box.
                        block.lineTop = line.getLineBoundsY().getStart() + 6.0f;
                        anchored = true;
                        break;
                    }
                }
            }

            if (! anchored)
            {
                block.lineTop = tailTop + 6.0f;
                tailTop += 6.0f + block.height + 12.0f;
                visualTailHeight = juce::jmax (visualTailHeight,
                                               tailTop - bodyLayout.getHeight());
            }
        }
    }

    /** Re-measure, then TELL THE CALLER. A bubble is placed by its host (see
     *  requiredHeightForWidth), so a body that grew - a streamed delta, a
     *  folded block - has changed the height the host gave it. Nothing inside
     *  this component can move the neighbours that sit under it: without the
     *  callback the text keeps growing past a box that never follows, which is
     *  both an overflow and, under a streamed answer, a transcript that stops
     *  showing the words as they arrive. */
    void invalidate()
    {
        const int w = getWidth();
        const auto f = ebs::fontBody().withHeight (bodyPts);
        const int textW = juce::jmax (24, bubbleWidthFor (w) - padH * 2);

        // Width, size and text unchanged: the layout is still valid, so a
        // repaint alone answers. Measuring a whole transcript for every delta
        // is the cost this guard removes.
        if (measuredWidth == w && measuredPts == bodyPts && measuredText == text
            && measuredExpanded == expanded && measuredShowCopy == showCopy
            && measuredHeader == header)
        {
            repaint();
            if (onHeightChanged != nullptr && lastHeight != height)
            {
                lastHeight = height;
                onHeightChanged();
            }
            return;
        }

        layOutForWidth (w);
        measuredWidth   = w;
        measuredPts     = bodyPts;
        measuredText    = text;
        measuredExpanded = expanded;
        measuredShowCopy  = showCopy;

        resized();
        repaint();
        if (onHeightChanged != nullptr && lastHeight != height)
        {
            lastHeight = height;
            onHeightChanged();
        }
    }

public:
    /** Tests: how many clickable link rows this bubble exposes. */
    int linkRowCountForTest() const noexcept { return (int) linkSpans.size(); }

    /** Tests: where a clickable run sits in the PROCESSED body, so a gate can
        prove the offset still matches the text the layout drew. A span that
        drifts from its glyphs is a link that highlights one word and opens
        another - the hit test intersects the two. */
    LinkSpan linkSpanForTest (int index) const
    {
        if (index < 0 || index >= (int) linkSpans.size())
            return {};
        return linkSpans[(size_t) index];
    }

    /** Tests: the PROCESSED body the layout draws (the Markdown markers are
        gone, a table is already padded). */
    juce::String renderedTextForTest() const { return layoutText; }

    /** Tests: how many Mermaid flowcharts the body paints. */
    int diagramCountForTest() const noexcept
    {
        int count = 0;
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::diagram)
                ++count;
        return count;
    }

    /** Tests: the dialect of the Nth diagram block ("" when none). */
    juce::String diagramKindForTest (int index) const
    {
        int seen = 0;
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::diagram)
            {
                if (seen++ == index)
                    return block.diagram != nullptr
                               ? block.diagram->kindForTest()
                               : juce::String();
            }
        return {};
    }

    /** Tests: how many embedded images the body lays out. */
    int imageCountForTest() const noexcept
    {
        int count = 0;
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::image)
                ++count;
        return count;
    }

    /** Tests: the URL of the first embedded image, or an empty string. */
    juce::String firstImageUrlForTest() const
    {
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::image)
                return block.url;
        return {};
    }

    /** Tests: how many audio players the body lays out. */
    int audioCountForTest() const noexcept
    {
        int count = 0;
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::audio)
                ++count;
        return count;
    }

    /** Tests: the URL of the first audio player, or an empty string. */
    juce::String firstAudioUrlForTest() const
    {
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::audio)
                return block.url;
        return {};
    }

    /** Tests: where the first audio player is drawn, so a click can be fed
        to the real mouse handler. */
    juce::Rectangle<float> audioRectForTest (int index) const
    {
        int seen = 0;
        for (const auto& block : visuals)
            if (block.kind == VisualBlock::Kind::audio)
            {
                if (seen++ == index)
                    return visualRectFor (block,
                                          bodyAreaForWidth (getWidth()));
            }
        return {};
    }

private:
    Side  side        = Side::assistant;
    Tone  tone        = Tone::plain;
    bool  showCopy    = false;
    bool  showBubble  = true;
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
    std::vector<LinkSpan> linkSpans;   // clickable ranges in the body layout
    /** The non-text blocks this body draws (diagrams and images), and where:
        they live in holes reserved in the text, so they never touch
        `layoutText`. */
    std::vector<VisualBlock> visuals;
    /** Height the layout had to add BELOW its last line for a drawing that
        sits at the end of the message (a TextLayout drops trailing blanks). */
    float visualTailHeight = 0.0f;
    /** The PROCESSED body the layout was built from: the string a selection
        indexes and copies (the Markdown markers are gone). */
    juce::String layoutText;
    /** Selection offsets in `layoutText`. Both -1 = nothing selected; the
        pair is always read as [min, max). */
    int selAnchor = -1;
    int selCaret  = -1;
    bool selecting = false;
    int bodyH  = 0;
    int height = 0;
    int lastHeight = 0;
    juce::Rectangle<int> headerPill;

    /** Zoom range for the body, in points. The bottom keeps a paragraph
     *  legible on a laptop; the top keeps a long answer on one screen's worth
     *  of rows without the transcript turning into a slide deck. */
    static constexpr float minBodyPts = 11.0f;
    static constexpr float maxBodyPts = 19.0f;
    float bodyPts = ebs::fontBody().getHeight();

    int  measuredWidth    = -1;
    float measuredPts     = -1.0f;
    bool measuredExpanded = true;
    bool measuredShowCopy = false;
    juce::String measuredText, measuredHeader;

    Avatar avatar;
    ebs::IconButton copyButton { ebs::IconButton::Shape::copy };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChatBubble)
};

} // namespace ebs
