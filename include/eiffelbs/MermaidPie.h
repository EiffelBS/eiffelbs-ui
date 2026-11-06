// MermaidPie.h
// eiffelbs-ui - a native renderer for the Mermaid PIE dialect.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The pie dialect is the simplest diagram an LLM emits: a header, an optional
// title, and one `"label" : value` per slice. There is no layout problem to
// solve, so this renderer is deliberately small: angles from the values, a
// disc or a ring, and a legend with the percentage (and the raw value when
// the source asked for it with `showData`).
//
// The same theme and the same width-fitting contract as every other Mermaid
// diagram: the host reserves the reported size, and the drawing is anchored at
// the area's top-left. A source with no usable slices is refused (false), so
// the caller shows the source instead of an empty pie.

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

class MermaidPie final : public MermaidDiagram
{
public:
    bool layout (const juce::String& source, float maxWidth,
                 juce::Font labelFont) override
    {
        reset();
        this->labelFont = labelFont;
        this->maxWidth = juce::jmax (120.0f, maxWidth);

        bool headerSeen = false;
        for (auto& rawLine : juce::StringArray::fromLines (
                 sanitizeForShaping (source)))
        {
            auto line = rawLine.trim();
            if (line.isEmpty() || line.startsWith ("%%"))
                continue;

            if (! headerSeen)
            {
                if (! line.startsWithIgnoreCase ("pie"))
                    continue;   // a dialect this renderer does not own

                headerSeen = true;
                auto rest = line.substring (3).trim();
                if (rest.startsWithIgnoreCase ("showData"))
                {
                    showData = true;
                    rest = rest.substring (8).trim();
                }
                if (rest.startsWithIgnoreCase ("title"))
                    title = rest.substring (5).trim();
                continue;
            }

            if (line.startsWithIgnoreCase ("title"))
            {
                title = line.substring (5).trim();
                continue;
            }
            if (line.startsWithIgnoreCase ("showData"))
            {
                showData = true;
                continue;
            }

            const int colon = line.indexOfChar (':');
            if (colon <= 0)
                continue;

            auto label = line.substring (0, colon).trim();
            if (label.length() >= 2 && label.startsWithChar ('"')
                && label.endsWithChar ('"'))
                label = label.substring (1, label.length() - 1);
            if (label.isEmpty())
                continue;

            const auto value = line.substring (colon + 1).trim()
                                   .getDoubleValue();
            if (value <= 0.0)
                continue;
            slices.push_back ({ label, value });
        }

        if (! headerSeen || slices.empty())
            return false;

        total = 0.0;
        for (const auto& slice : slices)
            total += slice.value;
        if (total <= 0.0)
            return false;

        layoutSize();
        return true;
    }

    bool isEmpty() const override { return slices.empty(); }
    int getWidth() const override { return juce::roundToInt (width); }
    int getHeight() const override { return juce::roundToInt (height); }
    juce::String kindForTest() const override { return "pie"; }

    void paint (juce::Graphics& g, juce::Rectangle<float> area) const override
    {
        if (slices.empty() || area.isEmpty())
            return;

        g.saveState();
        g.addTransform (juce::AffineTransform::translation (area.getX(),
                                                            area.getY()));

        const auto titleHeight = title.isNotEmpty()
                                     ? labelFont.getHeight() * 1.7f : 0.0f;
        if (title.isNotEmpty())
        {
            auto titleFont = labelFont;
            titleFont.setStyleFlags (juce::Font::bold);
            g.setColour (ebs::text());
            g.setFont (titleFont);
            g.drawText (title, juce::Rectangle<float> (0.0f, 0.0f, width,
                                                       titleHeight),
                        juce::Justification::centredLeft, true);
        }

        const auto contentTop = titleHeight + 6.0f;
        const auto stacked = width < 380.0f;
        const auto diameter = pieDiameter();
        juce::Rectangle<float> pieArea;
        juce::Rectangle<float> legendArea;

        if (stacked)
        {
            pieArea = { (width - diameter) * 0.5f, contentTop,
                        diameter, diameter };
            legendArea = { 16.0f, pieArea.getBottom() + 14.0f,
                           width - 32.0f, height - pieArea.getBottom() - 14.0f };
        }
        else
        {
            pieArea = { 18.0f, contentTop, diameter, diameter };
            legendArea = { pieArea.getRight() + 24.0f, contentTop,
                           width - pieArea.getRight() - 24.0f - 16.0f,
                           height - contentTop - 12.0f };
        }

        // The pie itself. A single slice is a full disc (addPieSegment with a
        // 2pi span is not reliable across shapes); everything else uses arcs.
        if (slices.size() == 1)
        {
            g.setColour (sliceColour (0));
            g.fillEllipse (pieArea);
        }
        else
        {
            constexpr float twoPi = juce::MathConstants<float>::twoPi;
            float angle = -juce::MathConstants<float>::halfPi;
            for (size_t i = 0; i < slices.size(); ++i)
            {
                const auto span = twoPi
                    * (float) (slices[i].value / total);
                const auto end = angle + span;
                juce::Path sector;
                sector.addPieSegment (pieArea, angle, end, 0.0f);
                g.setColour (sliceColour ((int) i));
                g.fillPath (sector);
                angle = end;
            }
        }

        g.setColour (ebs::panelBorder());
        g.drawEllipse (pieArea.reduced (0.5f), 1.0f);

        // The legend: a colour chip, the label, the percentage and, when the
        // source asked for it, the raw value.
        const auto rowHeight = juce::jmax (18.0f, labelFont.getHeight() * 1.45f);
        auto y = legendArea.getY();
        for (size_t i = 0; i < slices.size(); ++i)
        {
            const auto ratio = slices[i].value / total;
            auto row = juce::Rectangle<float> (legendArea.getX(), y,
                                               legendArea.getWidth(),
                                               rowHeight);
            const auto chip = row.removeFromLeft (rowHeight * 0.66f)
                                 .withSizeKeepingCentre (rowHeight * 0.66f,
                                                         rowHeight * 0.66f);
            g.setColour (sliceColour ((int) i));
            g.fillRoundedRectangle (chip, 3.0f);

            if (showData)
            {
                const auto valueText = juce::String (
                    juce::roundToInt (slices[i].value));
                const auto valueWidth = labelFont.getStringWidthFloat (valueText);
                g.setColour (ebs::textDim());
                g.setFont (labelFont);
                g.drawText (valueText,
                            row.removeFromRight (valueWidth + 2.0f),
                            juce::Justification::centredRight, false);
            }

            const auto percent = juce::String (ratio * 100.0, 1) + "%";
            const auto percentWidth = labelFont.getStringWidthFloat (percent);
            g.setColour (ebs::textDim());
            g.setFont (labelFont);
            g.drawText (percent, row.removeFromRight (percentWidth + 2.0f),
                        juce::Justification::centredRight, false);

            g.setColour (ebs::text());
            g.setFont (labelFont);
            g.drawText (slices[i].label, row.reduced (6.0f, 0.0f),
                        juce::Justification::centredLeft, true);

            y += rowHeight;
        }

        g.restoreState();
    }

    // === test seams =======================================================
    int sliceCountForTest() const noexcept { return (int) slices.size(); }
    juce::String titleForTest() const { return title; }
    double valueForTest (int index) const
    {
        return index >= 0 && index < (int) slices.size()
                   ? slices[(size_t) index].value : 0.0;
    }
    double percentForTest (int index) const
    {
        return total > 0.0 && index >= 0 && index < (int) slices.size()
                   ? slices[(size_t) index].value / total * 100.0 : 0.0;
    }
    bool showDataForTest() const noexcept { return showData; }

private:
    struct Slice
    {
        juce::String label;
        double value = 0.0;
    };

    void reset()
    {
        slices.clear();
        title.clear();
        total = 0.0;
        showData = false;
        width = height = 0.0f;
        labelFont = ebs::fontBody();
        maxWidth = 0.0f;
    }

    static juce::Colour sliceColour (int index)
    {
        static const juce::uint32 palette[] = {
            0xff4f8cff, 0xff3ecf8e, 0xffffb84d, 0xffe05a6d,
            0xff9b6dff, 0xff4dd7e8, 0xfff2e14c, 0xffb0b8c4
        };
        return juce::Colour (palette[(size_t) juce::jmax (0, index) % 8]);
    }

    float pieDiameter() const
    {
        const auto stacked = maxWidth < 380.0f;
        return juce::jlimit (100.0f, 180.0f,
                             stacked ? maxWidth - 32.0f
                                     : maxWidth * 0.42f);
    }

    void layoutSize()
    {
        const auto stacked = maxWidth < 380.0f;
        const auto titleHeight = title.isNotEmpty()
                                     ? labelFont.getHeight() * 1.7f : 0.0f;
        const auto rowHeight = juce::jmax (18.0f,
                                           labelFont.getHeight() * 1.45f);
        const auto legendHeight = rowHeight * (float) slices.size() + 4.0f;

        const auto content = stacked
            ? pieDiameter() + 14.0f + legendHeight
            : juce::jmax (pieDiameter(), legendHeight);

        width = maxWidth;
        height = titleHeight + 6.0f + content + 12.0f;
    }

    std::vector<Slice> slices;
    juce::String title;
    double total = 0.0;
    bool showData = false;
    juce::Font labelFont;
    float maxWidth = 0.0f;
    float width = 0.0f, height = 0.0f;

    JUCE_LEAK_DETECTOR (MermaidPie)
};

} // namespace ebs
