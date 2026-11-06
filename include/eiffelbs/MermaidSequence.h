// MermaidSequence.h
// eiffelbs-ui - a native renderer for the Mermaid SEQUENCE dialect.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A sequence diagram is not a graph: it is a set of COLUMNS (participants,
// each with a lifeline) and an ORDERED list of rows (messages, notes, and
// frames such as loop/alt/opt). There is no path routing, which is why this
// renderer is much smaller than the flowchart's layered layout: positions are
// one pass down the list.
//
// WHAT THIS SUBSET COVERS - the shapes LLM answers actually write:
//   participant A as Alice      actor B as Bob
//   A->>B: text   A-->>B: text   A->B: text   A-->B: text
//   A-xB: text    A--)B: text   (solid/dashed, arrow/cross/open head)
//   A->>A: text                 (self message)
//   Note over A,B: text   Note left of A: text   Note right of A: text
//   loop text ... end   alt text ... else text ... end   opt text ... end
//   par/and, critical/option, break   (as frames too)
//   autonumber
//
// NOT covered (the source is still shown verbatim by the host if the body is
// not a sequence at all): activation bars, `rect`, styling and the exotic
// message heads. They are additive later; ignoring them would draw a diagram
// that quietly says less than the source.
//
// The size contract is the one every Mermaid diagram uses: natural size laid
// out for the width offered, then a uniform scale down to fit; the host
// reserves getWidth()/getHeight() and paints at the area's top-left.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <vector>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/TextSanitize.h"
#include "eiffelbs/MermaidDiagram.h"

namespace ebs
{

class MermaidSequence final : public MermaidDiagram
{
public:
    bool layout (const juce::String& source, float maxWidth,
                 juce::Font labelFont) override
    {
        reset();
        this->labelFont = labelFont;
        this->maxWidth = juce::jmax (160.0f, maxWidth);

        bool headerSeen = false;
        std::vector<int> frameStack;

        for (auto& rawLine : juce::StringArray::fromLines (
                 sanitizeForShaping (source)))
        {
            auto line = rawLine.trim();
            if (line.isEmpty() || line.startsWith ("%%"))
                continue;

            if (! headerSeen)
            {
                if (! line.startsWithIgnoreCase ("sequenceDiagram"))
                    continue;
                headerSeen = true;
                continue;
            }

            if (line.startsWithIgnoreCase ("participant ")
                || line.startsWithIgnoreCase ("actor "))
            {
                auto rest = line.substring (line.indexOfChar (' ') + 1).trim();
                const int asAt = rest.indexOfIgnoreCase (" as ");
                juce::String id = rest, label = rest;
                if (asAt >= 0)
                {
                    id = rest.substring (0, asAt).trim();
                    label = rest.substring (asAt + 4).trim();
                }
                if (label.length() >= 2 && label.startsWithChar ('"')
                    && label.endsWithChar ('"'))
                    label = label.substring (1, label.length() - 1);
                if (id.isNotEmpty())
                    declareParticipant (id, label.isNotEmpty() ? label : id);
                continue;
            }

            if (line.startsWithIgnoreCase ("autonumber"))
            {
                autonumber = true;
                continue;
            }
            if (line.startsWithIgnoreCase ("activate")
                || line.startsWithIgnoreCase ("deactivate")
                || line.startsWithIgnoreCase ("title ")
                || line.startsWithIgnoreCase ("accTitle")
                || line.startsWithIgnoreCase ("accDescr"))
                continue;

            if (line.startsWithIgnoreCase ("Note "))
            {
                parseNote (line);
                continue;
            }

            // Frames. `end` closes the innermost; else/and/option are
            // separators inside the current frame.
            const auto startsWithWord = [&] (const char* word)
            {
                return line.startsWithIgnoreCase (word)
                       && (line.length() == (int) juce::String (word).length()
                           || juce::CharacterFunctions::isWhitespace (
                                  line[(int) juce::String (word).length()]));
            };

            bool frame = false;
            for (const auto* keyword : { "loop", "alt", "opt", "par",
                                         "critical", "break", "rect" })
            {
                if (startsWithWord (keyword))
                {
                    const auto text = line.substring (
                        (int) juce::String (keyword).length()).trim();
                    Item item;
                    item.kind = ItemKind::frameStart;
                    item.text = keyword + (text.isNotEmpty() ? " " + text : "");
                    item.depth = (int) frameStack.size();
                    items.push_back (std::move (item));
                    ++frameCount;
                    frameStack.push_back ((int) items.size() - 1);
                    frame = true;
                    break;
                }
            }
            if (frame)
                continue;

            if (startsWithWord ("else") || startsWithWord ("and")
                || startsWithWord ("option"))
            {
                const auto word = line.upToFirstOccurrenceOf (" ", false, false);
                const auto text = line.substring (word.length()).trim();
                Item item;
                item.kind = ItemKind::frameElse;
                item.text = word + (text.isNotEmpty() ? " " + text : "");
                item.depth = juce::jmax (0, (int) frameStack.size() - 1);
                items.push_back (std::move (item));
                continue;
            }
            if (line.equalsIgnoreCase ("end"))
            {
                if (frameStack.empty())
                    continue;
                Item item;
                item.kind = ItemKind::frameEnd;
                item.depth = (int) frameStack.size() - 1;
                item.from = frameStack.back();
                frameStack.pop_back();
                items.push_back (std::move (item));
                continue;
            }

            parseMessage (line);
        }

        if (! headerSeen || participants.empty())
            return false;
        if (items.empty() && participants.size() < 2)
            return false;

        layoutSize();
        return true;
    }

    bool isEmpty() const override { return participants.empty(); }
    int getWidth() const override { return juce::roundToInt (width); }
    int getHeight() const override { return juce::roundToInt (height); }
    juce::String kindForTest() const override { return "sequence"; }

    void paint (juce::Graphics& g, juce::Rectangle<float> area) const override
    {
        if (participants.empty() || area.isEmpty())
            return;

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (scale)
                            .translated (area.getX(), area.getY()));

        // Frames behind everything: fills, then borders and labels.
        for (const auto& item : items)
        {
            if (item.kind != ItemKind::frameStart || item.to < 0)
                continue;
            const auto& end = items[(size_t) item.to];
            const auto inset = 6.0f + (float) item.depth * 9.0f;
            const juce::Rectangle<float> box (inset, item.y,
                                              naturalWidth - inset * 2.0f,
                                              end.y - item.y);
            g.setColour (ebs::bgDark().withAlpha (0.30f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (ebs::panelBorder());
            g.drawRoundedRectangle (box, 5.0f, 1.0f);

            g.setFont (labelFont);
            const auto tabWidth = juce::jmin (
                box.getWidth() - 18.0f,
                labelFont.getStringWidthFloat (item.text) + 14.0f);
            const auto tabHeight = labelFont.getHeight() + 4.0f;
            juce::Rectangle<float> tab (box.getX() + 4.0f, box.getY() + 4.0f,
                                        tabWidth, tabHeight);
            g.setColour (ebs::bgPanel());
            g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (ebs::textDim());
            g.drawText (item.text, tab.reduced (5.0f, 0.0f),
                        juce::Justification::centredLeft, true);
        }

        // `else` / `and` separators: a dashed divider with its label, so an
        // alt branch reads as two branches and not as one run of messages.
        for (const auto& item : items)
        {
            if (item.kind != ItemKind::frameElse)
                continue;
            const auto inset = 6.0f + (float) item.depth * 9.0f;
            g.setColour (ebs::panelBorder().brighter (0.1f));
            juce::Path divider;
            divider.startNewSubPath (inset + 2.0f, item.y);
            divider.lineTo (naturalWidth - inset - 2.0f, item.y);
            juce::Path dashed;
            const float dashes[] = { 4.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, divider,
                                                            dashes, 2);
            g.fillPath (dashed);
            g.setColour (ebs::textDim());
            g.setFont (labelFont);
            g.drawText (item.text, inset + 10.0f, item.y + 1.0f,
                        juce::jmax (60.0f, naturalWidth - inset * 2.0f - 20.0f),
                        labelFont.getHeight() + 2.0f,
                        juce::Justification::centredLeft, true);
        }

        // Lifelines.
        const auto headerHeight = participantBoxHeight();
        const auto lifeTop = headerHeight + 16.0f;
        const auto lifeBottom = naturalHeight - 8.0f;
        for (const auto& participant : participants)
        {
            g.setColour (ebs::panelBorder().brighter (0.15f));
            juce::Path life;
            life.startNewSubPath (participant.x, lifeTop);
            life.lineTo (participant.x, lifeBottom);
            juce::Path dashed;
            const float dashes[] = { 5.0f, 5.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, life,
                                                            dashes, 2);
            g.fillPath (dashed);
        }

        // Participant boxes.
        for (const auto& participant : participants)
        {
            const auto boxWidth = participantBoxWidth (participant);
            const juce::Rectangle<float> box (participant.x - boxWidth * 0.5f,
                                              4.0f, boxWidth, headerHeight);
            g.setColour (ebs::bgDark());
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (ebs::panelBorder().brighter (0.2f));
            g.drawRoundedRectangle (box, 5.0f, 1.0f);
            g.setColour (ebs::text());
            g.setFont (labelFont);
            g.drawText (participant.label, box.reduced (6.0f, 0.0f),
                        juce::Justification::centred, true);
        }

        // Notes and messages, in row order.
        for (const auto& item : items)
        {
            if (item.kind == ItemKind::note)
                paintNote (g, item);
            else if (item.kind == ItemKind::message)
                paintMessage (g, item);
        }

        g.restoreState();
    }

    // === test seams =======================================================
    int participantCountForTest() const noexcept
    {
        return (int) participants.size();
    }
    int messageCountForTest() const noexcept { return messageCount; }
    int noteCountForTest() const noexcept { return noteCount; }
    int frameCountForTest() const noexcept { return frameCount; }
    double messageYForTest (int index) const
    {
        int seen = 0;
        for (const auto& item : items)
            if (item.kind == ItemKind::message)
            {
                if (seen++ == index)
                    return item.y;
            }
        return 0.0;
    }

private:
    enum class ItemKind { message, note, frameStart, frameElse, frameEnd };

    struct Item
    {
        ItemKind kind = ItemKind::message;
        int from = -1, to = -1;
        int style = 0;       // 0 solid, 1 dashed
        int side = 0;        // note: 0 over, -1 left, 1 right
        juce::String text;
        int depth = 0;
        float y = 0.0f;
        int number = 0;
    };

    struct Participant
    {
        juce::String id, label;
        float x = 0.0f;
    };

    void reset()
    {
        participants.clear();
        items.clear();
        autonumber = false;
        numberCounter = 0;
        messageCount = noteCount = frameCount = 0;
        labelFont = ebs::fontBody();
        maxWidth = 0.0f;
        naturalWidth = naturalHeight = width = height = 0.0f;
        scale = 1.0f;
    }

    int participantIndex (const juce::String& id)
    {
        for (int i = 0; i < (int) participants.size(); ++i)
            if (participants[(size_t) i].id == id)
                return i;
        return -1;
    }

    void declareParticipant (const juce::String& id, const juce::String& label)
    {
        if (const int existing = participantIndex (id); existing >= 0)
        {
            if (label.isNotEmpty())
                participants[(size_t) existing].label = label;
            return;
        }
        Participant participant;
        participant.id = id;
        participant.label = label.isNotEmpty() ? label : id;
        participants.push_back (std::move (participant));
    }

    int ensureParticipant (const juce::String& id)
    {
        const int existing = participantIndex (id);
        if (existing >= 0)
            return existing;
        declareParticipant (id, id);
        return (int) participants.size() - 1;
    }

    void parseNote (const juce::String& line)
    {
        // Note over A,B: text | Note left of A: text | Note right of A: text
        const auto rest = line.substring (5).trim();
        int side = 0;
        juce::String anchorText;
        if (rest.startsWithIgnoreCase ("over "))
        {
            side = 0;
            anchorText = rest.substring (5);
        }
        else if (rest.startsWithIgnoreCase ("left of "))
        {
            side = -1;
            anchorText = rest.substring (8);
        }
        else if (rest.startsWithIgnoreCase ("right of "))
        {
            side = 1;
            anchorText = rest.substring (9);
        }
        else
        {
            return;
        }

        const int colon = anchorText.indexOfChar (':');
        if (colon < 0)
            return;
        auto anchors = anchorText.substring (0, colon).trim();
        auto text = anchorText.substring (colon + 1).trim();

        juce::StringArray ids;
        for (auto id : juce::StringArray::fromTokens (anchors, ",", {}))
            if (id.trim().isNotEmpty())
                ids.add (id.trim());
        if (ids.isEmpty())
            return;

        Item item;
        item.kind = ItemKind::note;
        item.side = side;
        item.text = text;
        item.from = ensureParticipant (ids[0]);
        item.to = ids.size() > 1 ? ensureParticipant (ids[1]) : item.from;
        items.push_back (std::move (item));
        ++noteCount;
    }

    void parseMessage (const juce::String& line)
    {
        struct Arrow { const char* token; int style; };
        static const Arrow arrows[] = {
            { "-->>", 1 }, { "->>", 0 }, { "--x", 1 }, { "-x", 0 },
            { "--)", 1 }, { "-)", 0 }, { "-->", 1 }, { "->", 0 }
        };

        const Arrow* best = nullptr;
        int bestIndex = -1;
        for (const auto& arrow : arrows)
        {
            const int at = line.indexOf (arrow.token);
            if (at < 0)
                continue;
            if (best == nullptr || at < bestIndex
                || (at == bestIndex
                    && juce::String (arrow.token).length()
                           > juce::String (best->token).length()))
            {
                best = &arrow;
                bestIndex = at;
            }
        }
        if (best == nullptr)
            return;

        auto sender = line.substring (0, bestIndex).trim();
        auto rest = line.substring (bestIndex
                                    + juce::String (best->token).length());
        const int colon = rest.indexOfChar (':');
        auto receiver = (colon >= 0 ? rest.substring (0, colon) : rest).trim();
        auto text = colon >= 0 ? rest.substring (colon + 1).trim()
                               : juce::String();

        // Activation markers (`+`/`-`) ride on the receiver in Mermaid; they
        // are not part of the participant's name.
        while (receiver.startsWithChar ('+') || receiver.startsWithChar ('-'))
            receiver = receiver.substring (1).trim();
        if (sender.isEmpty() || receiver.isEmpty())
            return;

        Item item;
        item.kind = ItemKind::message;
        item.style = best->style;
        item.from = ensureParticipant (sender);
        item.to = ensureParticipant (receiver);
        item.text = text;
        item.number = autonumber ? ++numberCounter : 0;
        items.push_back (std::move (item));
        ++messageCount;
    }

    float participantBoxHeight() const
    {
        return labelFont.getHeight() + 14.0f;
    }

    float participantBoxWidth (const Participant& participant) const
    {
        return labelFont.getStringWidthFloat (participant.label) + 22.0f;
    }

    float requiredSpacing() const
    {
        float widest = 0.0f;
        for (const auto& participant : participants)
            widest = juce::jmax (widest, participantBoxWidth (participant));
        return juce::jmax (96.0f, widest + 26.0f);
    }

    void layoutSize()
    {
        const auto margin = 30.0f;
        auto spacing = requiredSpacing();
        naturalWidth = margin * 2.0f
                       + spacing * (float) juce::jmax (0,
                                                       (int) participants.size() - 1);
        if (participants.size() == 1)
            naturalWidth = juce::jmax (180.0f,
                                       participantBoxWidth (participants[0])
                                           + margin * 2.0f);

        // A wide bubble must not leave a narrow sequence squashed at its left:
        // spread the lifelines across the width offered. A narrow bubble still
        // scales the natural layout down below.
        if (naturalWidth < maxWidth)
        {
            if (participants.size() > 1)
                spacing = (maxWidth - margin * 2.0f)
                          / (float) (participants.size() - 1);
            naturalWidth = maxWidth;
        }

        if (participants.size() > 1)
            for (size_t i = 0; i < participants.size(); ++i)
                participants[i].x = margin + spacing * (float) i;
        else
            participants[0].x = naturalWidth * 0.5f;

        // Rows, with the frame bands taking a little more vertical room.
        float y = participantBoxHeight() + 18.0f;
        for (size_t i = 0; i < items.size(); ++i)
        {
            auto& item = items[i];
            if (item.kind == ItemKind::frameStart)
            {
                item.y = y;
                y += 22.0f;
            }
            else if (item.kind == ItemKind::frameElse)
            {
                item.y = y;
                y += 16.0f;
            }
            else if (item.kind == ItemKind::frameEnd)
            {
                item.y = y;
                y += 10.0f;
                if (item.from >= 0 && item.from < (int) items.size())
                    items[(size_t) item.from].to = (int) i;
            }
            else
            {
                item.y = y + 14.0f;
                y += 34.0f;
            }
        }

        naturalHeight = y + 12.0f;
        scale = juce::jmin (1.0f, maxWidth / juce::jmax (1.0f, naturalWidth));
        width = naturalWidth * scale;
        height = naturalHeight * scale;
    }

    void paintMessage (juce::Graphics& g, const Item& item) const
    {
        if (item.from < 0 || item.from >= (int) participants.size()
            || item.to < 0 || item.to >= (int) participants.size())
            return;

        const auto x1 = participants[(size_t) item.from].x;
        const auto x2 = participants[(size_t) item.to].x;
        const auto label = (item.number > 0
                                ? juce::String (item.number) + ". " : juce::String())
                           + item.text;

        g.setColour (ebs::panelBorder().brighter (0.3f));
        if (item.from == item.to)
        {
            // Self message: a small loop to the right of the lifeline.
            juce::Path loop;
            const auto top = item.y - 8.0f;
            const auto bottom = item.y + 4.0f;
            loop.startNewSubPath (x1, top);
            loop.cubicTo (x1 + 34.0f, top, x1 + 34.0f, bottom,
                          x1, bottom);
            g.strokePath (loop, juce::PathStrokeType (1.0f));
            juce::Path head;
            head.addTriangle (x1, bottom, x1 + 7.0f, bottom - 3.0f,
                              x1 + 7.0f, bottom + 3.0f);
            g.fillPath (head);
            if (label.isNotEmpty())
            {
                g.setColour (ebs::text());
                g.setFont (labelFont);
                g.drawText (label, x1 + 40.0f, top - 6.0f,
                            juce::jmax (60.0f, naturalWidth - x1 - 46.0f),
                            labelFont.getHeight() + 4.0f,
                            juce::Justification::centredLeft, true);
            }
            return;
        }

        g.setColour (ebs::panelBorder().brighter (0.3f));
        if (item.style == 1)
        {
            juce::Path line;
            line.startNewSubPath (x1, item.y);
            line.lineTo (x2, item.y);
            juce::Path dashed;
            const float dashes[] = { 6.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, line,
                                                            dashes, 2);
            g.fillPath (dashed);
        }
        else
        {
            g.drawLine (x1, item.y, x2, item.y, 1.0f);
        }

        // Arrowhead at the receiver.
        const auto direction = x2 >= x1 ? 1.0f : -1.0f;
        juce::Path head;
        head.addTriangle (x2, item.y,
                          x2 - direction * 8.0f, item.y - 4.0f,
                          x2 - direction * 8.0f, item.y + 4.0f);
        g.fillPath (head);

        if (label.isNotEmpty())
        {
            const auto middle = (x1 + x2) * 0.5f;
            const auto widthOfText = labelFont.getStringWidthFloat (label);
            const auto boxWidth = juce::jmin (naturalWidth - 8.0f,
                                              widthOfText + 10.0f);
            const juce::Rectangle<float> box (middle - boxWidth * 0.5f,
                                              item.y - labelFont.getHeight() - 5.0f,
                                              boxWidth, labelFont.getHeight() + 3.0f);
            g.setColour (ebs::bgPanel());
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (ebs::text());
            g.setFont (labelFont);
            g.drawFittedText (label, box.toNearestInt(),
                              juce::Justification::centred, 1);
        }
    }

    void paintNote (juce::Graphics& g, const Item& item) const
    {
        if (item.from < 0 || item.from >= (int) participants.size())
            return;

        const auto leftX = participants[(size_t) item.from].x;
        const auto rightX = item.to >= 0 && item.to < (int) participants.size()
                                ? participants[(size_t) item.to].x
                                : leftX;

        juce::Rectangle<float> box;
        if (item.side == 0)
        {
            // A note OVER one or two participants spans them, so it reads as
            // covering the exchange and not as a floating caption.
            const auto left = juce::jmin (leftX, rightX) - 34.0f;
            const auto right = juce::jmax (leftX, rightX) + 34.0f;
            box = { left, item.y - 16.0f, juce::jmax (80.0f, right - left),
                    32.0f };
        }
        else
        {
            const auto textWidth = juce::jmax (
                60.0f, labelFont.getStringWidthFloat (item.text) + 16.0f);
            const auto boxWidth = juce::jlimit (80.0f, 260.0f, textWidth);
            box = { item.side < 0 ? leftX - boxWidth - 12.0f : leftX + 12.0f,
                    item.y - 16.0f, boxWidth, 32.0f };
        }

        g.setColour (ebs::bgPanel().brighter (0.07f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (ebs::panelBorder());
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (ebs::textDim());
        g.setFont (labelFont);
        g.drawFittedText (item.text, box.reduced (7.0f, 3.0f).toNearestInt(),
                          juce::Justification::centred, 2);
    }

    std::vector<Participant> participants;
    std::vector<Item> items;
    juce::Font labelFont;
    bool autonumber = false;
    int numberCounter = 0;
    int messageCount = 0, noteCount = 0, frameCount = 0;
    float maxWidth = 0.0f;
    float naturalWidth = 0.0f, naturalHeight = 0.0f;
    float width = 0.0f, height = 0.0f;
    float scale = 1.0f;

    JUCE_LEAK_DETECTOR (MermaidSequence)
};

} // namespace ebs
