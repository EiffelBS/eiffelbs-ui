// MermaidDiagram.h
// eiffelbs-ui - the common shape of every native Mermaid renderer.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Mermaid has several diagram dialects, and the chat needs more than one:
// flowcharts (MermaidFlowchart), pie charts (MermaidPie) and sequence
// diagrams (MermaidSequence). They share everything the HOST cares about -
// parse a fence body, lay it out for a width, report the size to reserve, and
// paint into the reserved hole - and nothing about how they do it. That
// shared contract is this interface, so ChatBubble owns ONE block list and
// does not grow a field and a branch per dialect.
//
// The dialect is chosen by the first meaningful line of the fence body
// (`flowchart`, `graph`, `pie`, `sequenceDiagram`), never guessed from the
// content: a renderer that accepts a source it does not understand draws a
// lie, and the host's fallback (show the source) is better than that.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ebs
{

class MermaidDiagram
{
public:
    virtual ~MermaidDiagram() = default;

    /** Parse `source` (the fence body, without the ``` markers) and lay it
        out for `maxWidth` pixels. False = not a diagram this renderer
        understands, so the caller shows the source instead. */
    virtual bool layout (const juce::String& source, float maxWidth,
                         juce::Font labelFont) = 0;

    virtual bool isEmpty() const = 0;

    /** The size the host must reserve, in pixels, after the uniform fitting
        scale applied by layout(). */
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    /** Draw anchored at `area`'s top-left. */
    virtual void paint (juce::Graphics& g, juce::Rectangle<float> area) const = 0;

    /** Tests and host diagnostics: the dialect this renderer draws. */
    virtual juce::String kindForTest() const { return {}; }
};

} // namespace ebs
