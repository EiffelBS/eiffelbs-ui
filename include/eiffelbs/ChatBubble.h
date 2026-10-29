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
        syncLinkButtons();
        {
            const int left = bubbleLeftFor (w) + padH;
            const int right = left + bubbleWidthFor (w) - 2 * padH;
            int y = height - (showCopy ? copyRowH : 0)
                        - (int) bodyLinks.size() * linkRowH;
            for (size_t i = 0; i < linkButtons.size() && i < bodyLinks.size(); ++i)
            {
                linkButtons[i]->setBounds (left, y,
                                           juce::jmax (40, right - left),
                                           linkRowH - 2);
                y += linkRowH;
            }
        }
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
    /** Inline Markdown -> styled runs. Supported: **bold**, *italic*,
        `code`, [label](url). The URL itself is not printed (a weather answer's
        links would double the paragraph); the label is accented, which is the
        same affordance the rest of the app uses for a link. */
    struct BodyLink
    {
        juce::String label, url;
    };

    static void appendInlineRuns (juce::AttributedString& as,
                                  const juce::String& line,
                                  juce::Font base, juce::Colour ink,
                                  juce::Font mono,
                                  std::vector<BodyLink>* links = nullptr)
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
                        as.append (url, styleOf (base, bold, italic),
                                   ebs::accent());
                        if (links != nullptr)
                            links->push_back ({ url, url });
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
                as.append (line.substring (i + 1, end), mono,
                           ebs::accent().withMultipliedBrightness (1.1f));
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
                    as.append (label, styleOf (base, bold, italic),
                               ebs::accent());
                    if (links != nullptr)
                        links->push_back ({ label, url });
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
                                                std::vector<BodyLink>& linksOut)
    {
        const auto mono = ebs::createMonospaceFont (
            juce::jmax (9.0f, base.getHeight() * 0.92f));
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);

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
                as.append (line + "\n", mono, ebs::textDim());
                continue;
            }

            auto trimmed = line.trim();
            if (trimmed.isEmpty())
            {
                as.append ("\n", base, ink);
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
                {
                    juce::AttributedString sink;
                    appendInlineRuns (sink, cell, base, ink, mono, &linksOut);
                    rendered << (rendered.isEmpty() ? "" : "  |  ")
                             << plainCell (cell);
                }
                as.append (rendered + "\n", mono, ink);
                continue;
            }
            if (trimmed == "---" || trimmed == "***" || trimmed == "___")
            {
                as.append (juce::String::repeatedString ("\u2500", 18) + "\n",
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
                appendInlineRuns (as, title, f, ink, mono, &linksOut);
                as.append ("\n", f, ink);
                continue;
            }
            if (trimmed.startsWith ("> "))
            {
                as.append (juce::String::fromUTF8 ("\u258f "), mono,
                           ebs::textDim());
                appendInlineRuns (as, trimmed.substring (2),
                                 base, ebs::textDim(), mono, &linksOut);
                as.append ("\n", base, ink);
                continue;
            }
            if (trimmed.startsWith ("- ") || trimmed.startsWith ("* "))
            {
                as.append (juce::String::fromUTF8 ("\u2022 "), base, ebs::textDim());
                appendInlineRuns (as, trimmed.substring (2), base, ink, mono,
                                  &linksOut);
                as.append ("\n", base, ink);
                continue;
            }
            appendInlineRuns (as, line, base, ink, mono, &linksOut);
            as.append ("\n", base, ink);
        }
        // fromLines keeps a trailing empty line after the last newline: drop
        // the extra blank so a bubble does not grow a phantom line.
        return as;
    }

    void layOutForWidth (int w)
    {
        auto f = ebs::fontBody().withHeight (bodyPts);
        if (italic)
            f.setStyleFlags (juce::Font::italic);
        const int textW = juce::jmax (24, bubbleWidthFor (w) - padH * 2);
        bodyLinks.clear();
        auto as = markdownBody (text, f, italic ? ebs::textDim() : ebs::text(),
                                bodyLinks);
        // One clickable row per DISTINCT target; the label is what the reader
        // sees, the URL is what opens. Duplicates (same link cited twice) are
        // one row.
        {
            std::vector<BodyLink> unique;
            for (const auto& l : bodyLinks)
            {
                bool seen = false;
                for (const auto& u : unique)
                    if (u.url == l.url) { seen = true; break; }
                if (! seen && unique.size() < 8)
                    unique.push_back (l);
            }
            bodyLinks = std::move (unique);
        }
        bodyLayout.createLayout (as, (float) textW);
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
        height += (int) bodyLinks.size() * linkRowH;
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

    void syncLinkButtons()
    {
        while (linkButtons.size() < bodyLinks.size())
        {
            auto b = std::make_unique<juce::HyperlinkButton>();
            b->setFont (ebs::fontHint(), false);
            b->setColour (juce::HyperlinkButton::textColourId, ebs::accent());
            b->setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (*b);
            linkButtons.push_back (std::move (b));
        }
        for (size_t i = 0; i < linkButtons.size(); ++i)
        {
            auto& b = *linkButtons[i];
            const bool used = i < bodyLinks.size();
            b.setVisible (used);
            if (! used)
                continue;
            auto label = bodyLinks[i].label;
            if (label.length() > 64)
                label = label.substring (0, 61) + "...";
            if (b.getButtonText() != label)
                b.setButtonText (label);
            const juce::URL url (bodyLinks[i].url);
            if (b.getURL() != url)
                b.setURL (url);
            b.setTooltip (bodyLinks[i].url);
        }
    }

public:
    /** Tests: how many clickable link rows this bubble exposes. */
    int linkRowCountForTest() const noexcept { return (int) bodyLinks.size(); }

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
    static constexpr int linkRowH       = 18;
    static constexpr int corner         = 10;
    static constexpr int chevronSide    = 10;
    static constexpr int pillPad        = 5;

    juce::TextLayout bodyLayout;
    std::vector<BodyLink> bodyLinks;
    std::vector<std::unique_ptr<juce::HyperlinkButton>> linkButtons;
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
