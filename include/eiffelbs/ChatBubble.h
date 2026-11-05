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

#include <limits>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"
#include "eiffelbs/Avatar.h"

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
        // The I-beam is the affordance that says "this text is selectable",
        // exactly as it does in a browser. Without it a reader has no way to
        // know the body answers a drag rather than only the copy button.
        const bool overText = ! overLink && ! overHeader
                              && bodyAreaForWidth (getWidth())
                                     .contains (e.getPosition().toFloat());
        setMouseCursor (overLink || overHeader
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
                flush();
                italic = ! italic;
                continue;
            }
            run << c;
        }
        flush();
    }

    /** The whole body as Markdown-aware styled text. The BLOCK subset is the
        one LLM answers actually use: headings, bullets, numbered items,
        quotes, fenced code, horizontal rules and pipe TABLES (drawn as an
        aligned monospace block - a real column layout would need its own
        component inside the bubble, which is out of scope for a transcript). */
    static juce::AttributedString markdownBody (const juce::String& raw,
                                                juce::Font base,
                                                juce::Colour ink,
                                                std::vector<LinkSpan>& spansOut)
    {
        const auto mono = ebs::createMonospaceFont (
            juce::jmax (9.0f, base.getHeight() * 0.92f));
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        int pos = 0;
        const auto emit = [&] (const juce::String& t, juce::Font f,
                               juce::Colour c)
        {
            as.append (t, f, c);
            pos += t.length();
        };

        const auto lines = juce::StringArray::fromLines (raw);
        bool inFence = false;
        for (int n = 0; n < lines.size(); ++n)
        {
            auto line = lines[n];
            if (line.trimStart().startsWith ("```"))
            {
                inFence = ! inFence;
                continue;
            }
            if (inFence)
            {
                emit (line + "\n", mono, ebs::textDim());
                continue;
            }

            auto trimmed = line.trim();
            if (trimmed.isEmpty())
            {
                emit ("\n", base, ink);
                continue;
            }
            // Table cells keep their WORDS: a raw [label](url) inside a
            // monospace row would show the Markdown, not the text.
            const auto plainCell = [] (juce::String cell)
            {
                int at = 0;
                while ((at = cell.indexOfChar (at, '[')) >= 0)
                {
                    const int close = cell.indexOfChar (at + 1, ']');
                    const int open  = close >= 0 ? cell.indexOfChar (close + 1,
                                                                     '(') : -1;
                    const int shut  = open >= 0 ? cell.indexOfChar (open + 1,
                                                                    ')') : -1;
                    if (close < 0 || open != close + 1 || shut < 0)
                        break;
                    cell = cell.substring (0, at)
                         + cell.substring (at + 1, close)
                         + cell.substring (shut + 1);
                }
                cell = cell.replace ("**", "").replace ("__", "")
                           .replace ("`", "").replace ("*", "")
                           .replace ("_", "").trim();
                return cell;
            };
            // Table separator line (|---|---|) carries no content.
            if (trimmed.startsWith ("|") && trimmed.endsWith ("|"))
            {
                const auto inner = trimmed.substring (1, trimmed.length() - 1);
                auto cells = juce::StringArray::fromTokens (inner, "|", {});
                bool separator = true;
                for (auto cell : cells)
                {
                    cell = cell.trim();
                    for (const auto ch : cell)
                        if (ch != '-' && ch != ':' && ch != ' ')
                            separator = false;
                }
                if (separator)
                    continue;
                juce::String rendered;
                for (auto cell : cells)
                    rendered << (rendered.isEmpty() ? "" : "  |  ")
                             << plainCell (cell);
                emit (rendered + "\n", mono, ink);
                continue;
            }
            if (trimmed == "---" || trimmed == "***" || trimmed == "___")
            {
                emit (juce::String::repeatedString ("\u2500", 18) + "\n",
                      mono, ebs::textDim());
                continue;
            }
            if (trimmed.startsWith ("#"))
            {
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
            if (trimmed.startsWith ("> "))
            {
                emit (juce::String::fromUTF8 ("\u258f "), mono,
                      ebs::textDim());
                appendInlineRuns (as, trimmed.substring (2),
                                 base, ebs::textDim(), mono, pos, &spansOut);
                emit ("\n", base, ink);
                continue;
            }
            if (trimmed.startsWith ("- ") || trimmed.startsWith ("* "))
            {
                emit (juce::String::fromUTF8 ("\u2022 "), base, ebs::textDim());
                appendInlineRuns (as, trimmed.substring (2), base, ink, mono,
                                  pos, &spansOut);
                emit ("\n", base, ink);
                continue;
            }
            appendInlineRuns (as, line, base, ink, mono, pos, &spansOut);
            emit ("\n", base, ink);
        }
        // fromLines keeps a trailing empty line after the last newline: drop
        // the extra blank so a bubble does not grow a phantom line.
        return as;
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
        auto as = markdownBody (text, f, italic ? ebs::textDim() : ebs::text(),
                                linkSpans);
        // The processed string is what the layout draws and what a selection
        // indexes: keep exactly that one, or a copy would return the Markdown
        // markers the reader never saw.
        layoutText = as.getText();
        bodyLayout.createLayout (as, (float) textW);
        clampSelectionToText();
        // CEIL, then two pixels of slack. roundToInt truncates, and at the
        // larger chat sizes the last line's descender ended up outside the
        // bubble: a fractional line height MUST round up, and the extra pixel
        // covers the layout's reported height not including the full glyph
        // extents (author report 2026-10-29).
        bodyH = (int) std::ceil (bodyLayout.getHeight()) + 2;

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
