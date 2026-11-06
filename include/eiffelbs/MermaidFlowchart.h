// MermaidFlowchart.h
// eiffelbs-ui - a native renderer for the Mermaid FLOWCHART subset that LLM
// answers actually emit.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY A SUBSET AND NOT MERMAID
//
// Mermaid is a browser library: its full syntax (sequence, class, state, ER,
// gantt, pie, mindmap, timeline...) is a JavaScript parser plus an SVG layout
// engine hundreds of kilobytes wide. A JUCE app cannot embed it without a
// WebView and the JS asset, and a re-implementation of the whole spec is a
// project, not a chat feature. What a model writes in a chat is overwhelmingly
// `flowchart TD/LR` with labelled boxes and arrows, so THAT is what this
// renderer understands:
//
//   flowchart TD | TB | BT | LR | RL     (and the older `graph` spelling)
//   A[Rect] A(Rounded) A{Diamond} A((Circle)) A[with "quotes"]
//   A --> B      A --- B      A -.-> B      A ==> B
//   A -->|edge label| B        A -- edge label --> B
//   A --> B --> C                (chains, one per line or after ';')
//   %% comment                   (ignored)
//   classDef / style / click / subgraph / ...  (ignored, never fatal)
//
// Anything that is NOT a flowchart (a sequenceDiagram, a classDiagram, a
// malformed body) makes layout() return false, and the HOST shows the source
// as a code block instead. Inferring a diagram nobody wrote would be worse
// than showing the text the model wrote.
//
// LAYOUT
//
// Rank = longest path, so arrows read top-to-bottom (or left-to-right) and a
// node sits below every predecessor. Cycles are tolerated: the relaxation runs
// a bounded number of passes, which spreads a cycle across ranks instead of
// looping forever. Inside a rank, nodes are ordered by the average position of
// their predecessors (one barycentre pass), which is the cheap 80 % of the
// crossing reduction a real Sugiyama layout spends its time on.
//
// The drawing is vector on the shared theme: boxes, diamonds, circles, arrows
// and edge labels at the bubble's own font, scaled DOWN uniformly when the
// natural drawing is wider than the space it was offered. Nothing here is a
// juce::Component: the chat bubble paints it over the space it reserved, so a
// diagram can live between two paragraphs without changing the bubble's
// selection, links or copy.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/TextSanitize.h"

namespace ebs
{

class MermaidFlowchart
{
public:
    enum class Shape { rectangle, rounded, diamond, circle };

    MermaidFlowchart() = default;

    /** Parse `source` (the lines of a ```mermaid fence) and lay it out for
        `maxWidth` pixels. Returns false when the source is not a flowchart
        this renderer understands: the caller then shows the source itself.
        `labelFont` is the face the node and edge labels are drawn with. */
    bool layout (const juce::String& source, float maxWidth, juce::Font labelFont)
    {
        reset();
        this->labelFont = labelFont;
        this->maxWidth  = juce::jmax (60.0f, maxWidth);

        const auto text = sanitizeForShaping (source);
        auto lines = juce::StringArray::fromLines (text);

        bool headerSeen = false;
        bool unsupportedKind = false;
        for (auto& rawLine : lines)
        {
            auto line = rawLine.trim();
            if (line.isEmpty() || line.startsWith ("%%"))
                continue;

            // A diagram dialect this renderer does not implement. Refuse the
            // whole source: interpreting a sequence diagram's `A->>B` as
            // flowchart nodes would draw a lie.
            if (line.startsWithIgnoreCase ("sequenceDiagram")
                || line.startsWithIgnoreCase ("classDiagram")
                || line.startsWithIgnoreCase ("stateDiagram")
                || line.startsWithIgnoreCase ("erDiagram")
                || line.startsWithIgnoreCase ("gantt")
                || line.startsWithIgnoreCase ("pie")
                || line.startsWithIgnoreCase ("mindmap")
                || line.startsWithIgnoreCase ("journey")
                || line.startsWithIgnoreCase ("timeline")
                || line.startsWithIgnoreCase ("quadrantChart")
                || line.startsWithIgnoreCase ("requirementDiagram")
                || line.startsWithIgnoreCase ("C4Context")
                || line.startsWithIgnoreCase ("sankey-beta")
                || line.startsWithIgnoreCase ("xychart")
                || line.startsWithIgnoreCase ("block-beta")
                || line.startsWithIgnoreCase ("packet-beta"))
            {
                unsupportedKind = true;
                break;
            }

            if (line.startsWithIgnoreCase ("flowchart")
                || line.startsWithIgnoreCase ("graph"))
            {
                headerSeen = true;
                const auto direction = line.fromFirstOccurrenceOf (
                    " ", false, false).trim().toUpperCase();
                horizontal = direction.startsWith ("LR")
                          || direction.startsWith ("RL");
                reversed   = direction.startsWith ("BT")
                          || direction.startsWith ("RL");
                continue;
            }

            // Directives that carry presentation, not graph structure. A model
            // that adds `style` rows still wrote a diagram; dropping the colour
            // is better than dropping the diagram.
            if (line.startsWithIgnoreCase ("classDef")
                || line.startsWithIgnoreCase ("class ")
                || line.startsWithIgnoreCase ("style ")
                || line.startsWithIgnoreCase ("linkStyle")
                || line.startsWithIgnoreCase ("click ")
                || line.startsWithIgnoreCase ("subgraph")
                || line == "end"
                || line.startsWithIgnoreCase ("direction ")
                || line.startsWithIgnoreCase ("accTitle")
                || line.startsWithIgnoreCase ("accDescr"))
                continue;

            // A statement may chain several arrows; ';' separates statements
            // on one physical line.
            for (auto statement : juce::StringArray::fromTokens (line, ";", {}))
            {
                statement = statement.trim();
                if (statement.isNotEmpty())
                    parseStatement (statement);
            }
        }

        juce::ignoreUnused (headerSeen);
        if (unsupportedKind || nodes.empty())
            return false;

        assignRanksAndOrder();
        measureNodes();
        positionNodes();
        naturalWidth  = juce::jmax (1.0f, naturalWidth);
        naturalHeight = juce::jmax (1.0f, naturalHeight);
        scale = juce::jmin (1.0f, maxWidth / naturalWidth);
        width  = naturalWidth  * scale;
        height = naturalHeight * scale;
        return true;
    }

    bool isEmpty() const noexcept { return nodes.empty(); }

    /** The size the host must reserve, in PIXELS, after the uniform fitting
        scale applied in layout(). */
    int getWidth()  const noexcept { return juce::roundToInt (width); }
    int getHeight() const noexcept { return juce::roundToInt (height); }

    /** Draw into `area`; the drawing is fitted to its width and anchored at
        the area's top-left. */
    void paint (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        if (nodes.empty() || area.isEmpty())
            return;

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (scale)
                            .translated (area.getX(), area.getY()));
        paintEdges (g);
        paintNodes (g);
        g.restoreState();
    }

    // === test seams =======================================================
    int nodeCountForTest() const noexcept { return (int) nodes.size(); }
    int edgeCountForTest() const noexcept { return (int) edges.size(); }
    int rankCountForTest() const noexcept
    {
        int ranks = 0;
        for (const auto& node : nodes)
            ranks = juce::jmax (ranks, node.rank + 1);
        return ranks;
    }
    bool isHorizontalForTest() const noexcept { return horizontal; }
    juce::String nodeLabelForTest (int index) const
    {
        return index >= 0 && index < (int) nodes.size()
                   ? nodes[(size_t) index].label
                   : juce::String();
    }

private:
    struct Node
    {
        juce::String id;
        juce::String label;
        Shape shape = Shape::rectangle;
        int rank = 0;
        int order = 0;
        float width = 40.0f, height = 24.0f;
        juce::Rectangle<float> bounds;
        std::vector<juce::String> labelLines;
    };

    struct Edge
    {
        int from = 0, to = 0;
        int style = 0;   // 0 solid, 1 dashed, 2 thick
        juce::String label;
    };

    void reset()
    {
        nodes.clear();
        edges.clear();
        horizontal = false;
        reversed = false;
        width = height = naturalWidth = naturalHeight = 0.0f;
        scale = 1.0f;
        maxWidth = 0.0f;
    }

    static bool isIdChar (juce::juce_wchar c)
    {
        return juce::CharacterFunctions::isLetterOrDigit (c)
               || c == '_' || c == '-' || c == '.' || c == ':';
    }

    static juce::String unquote (juce::String s)
    {
        s = s.trim();
        if (s.length() >= 2
            && ((s.startsWithChar ('"') && s.endsWithChar ('"'))
                || (s.startsWithChar ('\'') && s.endsWithChar ('\''))))
            s = s.substring (1, s.length() - 1);
        return s.trim().replace ("<br/>", "\n").replace ("<br>", "\n");
    }

    int findOrAddNode (const juce::String& id, Shape shape,
                       const juce::String& label)
    {
        for (int i = 0; i < (int) nodes.size(); ++i)
            if (nodes[(size_t) i].id == id)
            {
                if (label.isNotEmpty())
                {
                    nodes[(size_t) i].label = label;
                    nodes[(size_t) i].shape = shape;
                }
                return i;
            }

        Node node;
        node.id = id;
        node.label = label.isNotEmpty() ? label : id;
        node.shape = shape;
        nodes.push_back (std::move (node));
        return (int) nodes.size() - 1;
    }

    /** Parse one node reference at `pos`; returns its index, or -1. */
    int parseNodeRef (const juce::String& s, int& pos)
    {
        while (pos < s.length() && juce::CharacterFunctions::isWhitespace (s[pos]))
            ++pos;

        const int start = pos;
        while (pos < s.length() && isIdChar (s[pos]))
            ++pos;
        if (pos == start)
            return -1;

        const auto id = s.substring (start, pos);
        auto shape = Shape::rectangle;
        juce::String label;

        if (pos < s.length())
        {
            const auto open = s[pos];
            if (open == '[')
            {
                const int close = s.indexOfChar (pos + 1, ']');
                if (close > pos)
                {
                    label = unquote (s.substring (pos + 1, close));
                    pos = close + 1;
                }
            }
            else if (open == '(')
            {
                if (pos + 1 < s.length() && s[pos + 1] == '(')
                {
                    const int close = s.indexOf (pos + 2, "))");
                    if (close > pos)
                    {
                        shape = Shape::circle;
                        label = unquote (s.substring (pos + 2, close));
                        pos = close + 2;
                    }
                }
                else
                {
                    const int close = s.indexOfChar (pos + 1, ')');
                    if (close > pos)
                    {
                        shape = Shape::rounded;
                        label = unquote (s.substring (pos + 1, close));
                        pos = close + 1;
                    }
                }
            }
            else if (open == '{')
            {
                const int close = s.indexOfChar (pos + 1, '}');
                if (close > pos)
                {
                    shape = Shape::diamond;
                    label = unquote (s.substring (pos + 1, close));
                    pos = close + 1;
                }
            }
        }

        return findOrAddNode (id, shape, label);
    }

    /** Parse one arrow at `pos`; returns true and sets `style`/`label`. */
    static bool parseArrow (const juce::String& s, int& pos, int& style,
                            juce::String& label)
    {
        while (pos < s.length() && juce::CharacterFunctions::isWhitespace (s[pos]))
            ++pos;

        style = 0;
        label.clear();
        const auto rest = s.substring (pos);

        if (rest.startsWith ("-.->"))       { style = 1; pos += 4; }
        else if (rest.startsWith ("-->"))   { style = 0; pos += 3; }
        else if (rest.startsWith ("---"))   { style = 0; pos += 3; }
        else if (rest.startsWith ("==>"))   { style = 2; pos += 3; }
        else if (rest.startsWith ("--"))
        {
            // `-- label -->` and `-- label ---`
            const int open = pos + 2;
            const int closeArrow = s.indexOf (open, "-->");
            const int closeLine  = s.indexOf (open, "---");
            int close = -1;
            if (closeArrow >= 0) close = closeArrow;
            if (closeLine >= 0 && (close < 0 || closeLine < close))
                close = closeLine;
            if (close < 0)
                return false;
            label = unquote (s.substring (open, close));
            pos = close + 3;
        }
        else
        {
            return false;
        }

        while (pos < s.length() && juce::CharacterFunctions::isWhitespace (s[pos]))
            ++pos;
        if (pos < s.length() && s[pos] == '|')
        {
            const int close = s.indexOfChar (pos + 1, '|');
            if (close > pos)
            {
                label = unquote (s.substring (pos + 1, close));
                pos = close + 1;
            }
        }
        return true;
    }

    void parseStatement (const juce::String& s)
    {
        int pos = 0;
        int previous = parseNodeRef (s, pos);
        if (previous < 0)
            return;

        for (;;)
        {
            int style = 0;
            juce::String label;
            if (! parseArrow (s, pos, style, label))
                break;
            const int next = parseNodeRef (s, pos);
            if (next < 0)
                break;

            Edge edge;
            edge.from = previous;
            edge.to = next;
            edge.style = style;
            edge.label = label;
            edges.push_back (std::move (edge));
            previous = next;
        }
    }

    /** Rank = longest path, bounded so a cycle cannot loop forever. */
    void assignRanksAndOrder()
    {
        const int n = (int) nodes.size();
        for (auto& node : nodes)
            node.rank = 0;

        for (int pass = 0; pass < n; ++pass)
            for (const auto& edge : edges)
                if (edge.from != edge.to)
                    nodes[(size_t) edge.to].rank = juce::jmax (
                        nodes[(size_t) edge.to].rank,
                        nodes[(size_t) edge.from].rank + 1);

        int ranks = 0;
        for (const auto& node : nodes)
            ranks = juce::jmax (ranks, node.rank + 1);

        std::vector<std::vector<int>> rankOrder ((size_t) juce::jmax (1, ranks));
        for (int i = 0; i < n; ++i)
            rankOrder[(size_t) nodes[(size_t) i].rank].push_back (i);

        // One barycentre pass: a node follows the average position of its
        // predecessors, which removes the obvious crossings in a chat-scale
        // graph without a full Sugiyama implementation.
        const auto orderOf = [&] (int nodeIndex)
        {
            const auto& row = rankOrder[(size_t) nodes[(size_t) nodeIndex].rank];
            for (int i = 0; i < (int) row.size(); ++i)
                if (row[(size_t) i] == nodeIndex)
                    return (float) i;
            return 0.0f;
        };

        for (int r = 1; r < (int) rankOrder.size(); ++r)
        {
            auto& row = rankOrder[(size_t) r];
            std::stable_sort (row.begin(), row.end(),
                              [&] (int a, int b)
                              {
                                  const auto bary = [&] (int nodeIndex)
                                  {
                                      float sum = 0.0f;
                                      int count = 0;
                                      for (const auto& edge : edges)
                                          if (edge.to == nodeIndex
                                              && nodes[(size_t) edge.from].rank < r)
                                          {
                                              sum += orderOf (edge.from);
                                              ++count;
                                          }
                                      return count > 0 ? sum / (float) count
                                                       : 1.0e9f;
                                  };
                                  return bary (a) < bary (b);
                              });
        }

        for (int r = 0; r < (int) rankOrder.size(); ++r)
            for (int i = 0; i < (int) rankOrder[(size_t) r].size(); ++i)
                nodes[(size_t) rankOrder[(size_t) r][(size_t) i]].order = i;
    }

    std::vector<juce::String> wrapLabel (const juce::String& text,
                                         float maxTextWidth) const
    {
        std::vector<juce::String> lines;
        for (const auto& rawLine : juce::StringArray::fromLines (text))
        {
            juce::String line;
            for (const auto& word : juce::StringArray::fromTokens (
                     rawLine, " \t", {}))
            {
                const auto candidate = line.isEmpty() ? word
                                                      : line + " " + word;
                if (! line.isEmpty()
                    && labelFont.getStringWidthFloat (candidate) > maxTextWidth)
                {
                    lines.push_back (line);
                    line = word;
                }
                else
                {
                    line = candidate;
                }
            }
            lines.push_back (line);
        }
        if (lines.empty())
            lines.push_back (juce::String());
        return lines;
    }

    void measureNodes()
    {
        const auto maxLabelWidth = juce::jlimit (70.0f, 230.0f,
                                                 maxWidth * 0.32f);
        const auto lineHeight = labelFont.getHeight() * 1.16f;
        const auto padX = 9.0f;
        const auto padY = 6.0f;

        for (auto& node : nodes)
        {
            node.labelLines = wrapLabel (node.label, maxLabelWidth);
            float textWidth = 1.0f;
            for (const auto& line : node.labelLines)
                textWidth = juce::jmax (
                    textWidth, labelFont.getStringWidthFloat (line));
            const float textHeight = (float) node.labelLines.size() * lineHeight;

            switch (node.shape)
            {
                case Shape::rectangle:
                case Shape::rounded:
                    node.width  = textWidth + padX * 2.0f;
                    node.height = textHeight + padY * 2.0f;
                    break;
                case Shape::diamond:
                    // A diamond has to hold the text inside its sloped sides:
                    // inflate both axes, not just the box.
                    node.width  = textWidth * 1.65f + padX * 2.0f;
                    node.height = textHeight * 1.65f + padY * 2.0f;
                    break;
                case Shape::circle:
                {
                    const float diameter = juce::jmax (textWidth, textHeight)
                                               * 1.45f;
                    node.width = node.height = diameter + padY * 2.0f;
                    break;
                }
            }

            node.width  = juce::jmax (node.width, 34.0f);
            node.height = juce::jmax (node.height, 24.0f);
        }
    }

    void positionNodes()
    {
        int ranks = 0;
        for (const auto& node : nodes)
            ranks = juce::jmax (ranks, node.rank + 1);

        std::vector<std::vector<int>> rankOrder ((size_t) ranks);
        for (int i = 0; i < (int) nodes.size(); ++i)
            rankOrder[(size_t) nodes[(size_t) i].rank].push_back (i);

        const auto nodeGap = 16.0f;
        const auto rankGap = 34.0f;

        if (! horizontal)
        {
            // Top-down: a rank is a row.
            float widest = 0.0f;
            std::vector<float> rowHeights ((size_t) ranks, 0.0f);
            std::vector<float> rowWidths ((size_t) ranks, 0.0f);
            for (int r = 0; r < ranks; ++r)
            {
                for (const auto index : rankOrder[(size_t) r])
                {
                    rowHeights[(size_t) r] = juce::jmax (
                        rowHeights[(size_t) r], nodes[(size_t) index].height);
                    rowWidths[(size_t) r] += nodes[(size_t) index].width;
                }
                rowWidths[(size_t) r] += nodeGap
                    * (float) juce::jmax (0, (int) rankOrder[(size_t) r].size() - 1);
                widest = juce::jmax (widest, rowWidths[(size_t) r]);
            }

            naturalWidth = widest;
            float y = 0.0f;
            for (int r = 0; r < ranks; ++r)
            {
                float x = (widest - rowWidths[(size_t) r]) * 0.5f;
                for (const auto index : rankOrder[(size_t) r])
                {
                    auto& node = nodes[(size_t) index];
                    node.bounds = { x, y + (rowHeights[(size_t) r]
                                            - node.height) * 0.5f,
                                    node.width, node.height };
                    x += node.width + nodeGap;
                }
                y += rowHeights[(size_t) r] + rankGap;
            }
            naturalHeight = juce::jmax (0.0f, y - rankGap);
        }
        else
        {
            // Left-to-right: a rank is a column.
            float tallest = 0.0f;
            std::vector<float> columnWidths ((size_t) ranks, 0.0f);
            std::vector<float> columnHeights ((size_t) ranks, 0.0f);
            for (int r = 0; r < ranks; ++r)
            {
                for (const auto index : rankOrder[(size_t) r])
                {
                    columnWidths[(size_t) r] = juce::jmax (
                        columnWidths[(size_t) r], nodes[(size_t) index].width);
                    columnHeights[(size_t) r] += nodes[(size_t) index].height;
                }
                columnHeights[(size_t) r] += nodeGap
                    * (float) juce::jmax (0, (int) rankOrder[(size_t) r].size() - 1);
                tallest = juce::jmax (tallest, columnHeights[(size_t) r]);
            }

            naturalHeight = tallest;
            float x = 0.0f;
            for (int r = 0; r < ranks; ++r)
            {
                float y = (tallest - columnHeights[(size_t) r]) * 0.5f;
                for (const auto index : rankOrder[(size_t) r])
                {
                    auto& node = nodes[(size_t) index];
                    node.bounds = { x + (columnWidths[(size_t) r]
                                         - node.width) * 0.5f,
                                    y, node.width, node.height };
                    y += node.height + nodeGap;
                }
                x += columnWidths[(size_t) r] + rankGap;
            }
            naturalWidth = juce::jmax (0.0f, x - rankGap);
        }

        if (reversed)
        {
            for (auto& node : nodes)
            {
                if (horizontal)
                    node.bounds.setX (naturalWidth - node.bounds.getRight());
                else
                    node.bounds.setY (naturalHeight - node.bounds.getBottom());
            }
        }
    }

    /** Where a straight line from the node's centre towards `towards` leaves
        the node's box. */
    static juce::Point<float> borderPoint (juce::Rectangle<float> box,
                                           juce::Point<float> towards)
    {
        const auto centre = box.getCentre();
        auto d = towards - centre;
        if (std::abs (d.x) < 0.001f && std::abs (d.y) < 0.001f)
            return centre;

        // Scale the direction until one axis reaches the box edge.
        const auto hw = box.getWidth() * 0.5f;
        const auto hh = box.getHeight() * 0.5f;
        float t = 1.0f;
        if (std::abs (d.x) > 0.001f) t = juce::jmin (t, hw / std::abs (d.x));
        if (std::abs (d.y) > 0.001f) t = juce::jmin (t, hh / std::abs (d.y));
        return centre + d * t;
    }

    void paintEdges (juce::Graphics& g) const
    {
        for (const auto& edge : edges)
        {
            if (edge.from < 0 || edge.from >= (int) nodes.size()
                || edge.to < 0 || edge.to >= (int) nodes.size())
                continue;

            const auto& a = nodes[(size_t) edge.from];
            const auto& b = nodes[(size_t) edge.to];

            if (edge.from == edge.to)
            {
                paintSelfLoop (g, a, edge);
                continue;
            }

            // An edge that skips a rank - or goes backwards in a cycle - is
            // routed AROUND the boxes on one side: a straight line would run
            // under the nodes in between (the if/else flowchart's `no` branch
            // disappeared behind the `yes` box).
            const int rankDelta = b.rank - a.rank;
            const bool detour = std::abs (rankDelta) > 1 || rankDelta <= 0;

            juce::Path path;
            juce::Point<float> arrowFrom, arrowTo, labelAt;

            if (! detour)
            {
                const auto from = borderPoint (a.bounds, b.bounds.getCentre());
                const auto to   = borderPoint (b.bounds, a.bounds.getCentre());
                path.startNewSubPath (from);
                path.lineTo (to);
                arrowFrom = from;
                arrowTo = to;
                labelAt = (from + to) * 0.5f;
            }
            else if (! horizontal)
            {
                const bool toTheRight = b.bounds.getCentreX()
                                        >= a.bounds.getCentreX();
                const auto routeX = toTheRight
                    ? juce::jmax (a.bounds.getRight(), b.bounds.getRight()) + 16.0f
                    : juce::jmin (a.bounds.getX(), b.bounds.getX()) - 16.0f;
                const auto from = juce::Point<float> (
                    toTheRight ? a.bounds.getRight() : a.bounds.getX(),
                    a.bounds.getCentreY());
                const auto to = juce::Point<float> (
                    toTheRight ? b.bounds.getRight() : b.bounds.getX(),
                    b.bounds.getCentreY());
                const auto elbow1 = juce::Point<float> (routeX, from.y);
                const auto elbow2 = juce::Point<float> (routeX, to.y);
                path.startNewSubPath (from);
                path.lineTo (elbow1);
                path.lineTo (elbow2);
                path.lineTo (to);
                arrowFrom = elbow2;
                arrowTo = to;
                labelAt = { routeX, (from.y + to.y) * 0.5f };
            }
            else
            {
                const bool toTheBottom = b.bounds.getCentreY()
                                         >= a.bounds.getCentreY();
                const auto routeY = toTheBottom
                    ? juce::jmax (a.bounds.getBottom(), b.bounds.getBottom()) + 16.0f
                    : juce::jmin (a.bounds.getY(), b.bounds.getY()) - 16.0f;
                const auto from = juce::Point<float> (
                    a.bounds.getCentreX(),
                    toTheBottom ? a.bounds.getBottom() : a.bounds.getY());
                const auto to = juce::Point<float> (
                    b.bounds.getCentreX(),
                    toTheBottom ? b.bounds.getBottom() : b.bounds.getY());
                const auto elbow1 = juce::Point<float> (from.x, routeY);
                const auto elbow2 = juce::Point<float> (to.x, routeY);
                path.startNewSubPath (from);
                path.lineTo (elbow1);
                path.lineTo (elbow2);
                path.lineTo (to);
                arrowFrom = elbow2;
                arrowTo = to;
                labelAt = { (from.x + to.x) * 0.5f, routeY };
            }

            g.setColour (ebs::panelBorder().brighter (0.25f));
            if (edge.style == 1)
            {
                juce::Path dashed;
                const float dashes[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (
                    dashed, path, dashes, 2);
                g.fillPath (dashed);
            }
            else
            {
                g.strokePath (path, juce::PathStrokeType (
                                  edge.style == 2 ? 2.2f : 1.0f));
            }

            paintArrowHead (g, arrowFrom, arrowTo);

            if (edge.label.isNotEmpty())
            {
                const auto width = labelFont.getStringWidthFloat (edge.label);
                const auto height = labelFont.getHeight();
                juce::Rectangle<float> box (labelAt.x - width * 0.5f - 3.0f,
                                            labelAt.y - height * 0.5f - 1.0f,
                                            width + 6.0f, height + 2.0f);
                g.setColour (ebs::bgPanel());
                g.fillRoundedRectangle (box, 3.0f);
                g.setColour (ebs::textDim());
                g.setFont (labelFont);
                g.drawText (edge.label, box, juce::Justification::centred);
            }
        }
    }

    void paintArrowHead (juce::Graphics& g, juce::Point<float> from,
                         juce::Point<float> to) const
    {
        auto direction = to - from;
        if (direction.getDistanceFromOrigin() < 0.5f)
            return;
        direction = direction / direction.getDistanceFromOrigin();
        const auto normal = juce::Point<float> (-direction.y, direction.x);
        const auto tip = to;
        const auto base = tip - direction * 8.0f;
        juce::Path head;
        head.addTriangle (tip, base + normal * 4.0f, base - normal * 4.0f);
        g.fillPath (head);
    }

    void paintSelfLoop (juce::Graphics& g, const Node& node,
                        const Edge& edge) const
    {
        const auto box = node.bounds;
        const auto start = box.getTopRight().translated (-4.0f, 0.0f);
        const auto end   = box.getBottomRight().translated (-4.0f, 0.0f);
        const auto bulge = juce::Point<float> (box.getRight() + 26.0f,
                                               box.getCentreY());

        juce::Path loop;
        loop.startNewSubPath (start);
        loop.cubicTo (bulge, bulge, end);
        g.setColour (ebs::panelBorder().brighter (0.25f));
        g.strokePath (loop, juce::PathStrokeType (
                          edge.style == 2 ? 2.2f : 1.0f));
        paintArrowHead (g, bulge, end);
    }

    void paintNodes (juce::Graphics& g) const
    {
        for (const auto& node : nodes)
        {
            const auto box = node.bounds;

            g.setColour (ebs::bgDark());
            if (node.shape == Shape::diamond)
                g.fillPath (diamondFor (box));
            else if (node.shape == Shape::circle)
                g.fillEllipse (box);
            else
                g.fillRoundedRectangle (box, cornerFor (node));

            g.setColour (ebs::panelBorder().brighter (0.2f));
            if (node.shape == Shape::diamond)
                g.strokePath (diamondFor (box), juce::PathStrokeType (1.0f));
            else if (node.shape == Shape::circle)
                g.drawEllipse (box, 1.0f);
            else
                g.drawRoundedRectangle (box, cornerFor (node), 1.0f);

            g.setColour (ebs::text());
            g.setFont (labelFont);
            const auto lineHeight = labelFont.getHeight() * 1.16f;
            const auto total = lineHeight * (float) node.labelLines.size();
            auto y = box.getCentreY() - total * 0.5f;
            for (const auto& line : node.labelLines)
            {
                g.drawText (line, box.withY (y).withHeight (lineHeight),
                            juce::Justification::centred);
                y += lineHeight;
            }
        }
    }

    static float cornerFor (const Node& node)
    {
        return node.shape == Shape::rounded
                   ? juce::jmin (10.0f, node.bounds.getHeight() * 0.35f)
                   : 4.0f;
    }

    static juce::Path diamondFor (juce::Rectangle<float> box)
    {
        juce::Path diamond;
        diamond.startNewSubPath (box.getCentreX(), box.getY());
        diamond.lineTo (box.getRight(), box.getCentreY());
        diamond.lineTo (box.getCentreX(), box.getBottom());
        diamond.lineTo (box.getX(), box.getCentreY());
        diamond.closeSubPath();
        return diamond;
    }

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    juce::Font labelFont;
    bool horizontal = false;
    bool reversed = false;
    float maxWidth = 0.0f;
    float width = 0.0f, height = 0.0f;
    float naturalWidth = 0.0f, naturalHeight = 0.0f;
    float scale = 1.0f;

    JUCE_LEAK_DETECTOR (MermaidFlowchart)
};

} // namespace ebs
