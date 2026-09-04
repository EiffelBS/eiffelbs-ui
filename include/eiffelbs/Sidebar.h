// Sidebar.h
// eiffelbs-ui - collapsible/resizable side panel (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Promoted from OpenTimbre's take list column (source/ui/TakeListPanel.h),
// where a fixed 300 px right column needed collapse + width drag.
//
// A host-owned content component fills the body; the library owns only the
// chrome: an edge handle strip (collapse chevron + width drag). Collapsed,
// the body hides and only a slim rail remains; the chevron flips. Width is
// user-driven (mouse drag on the rail), clamped, and reported through
// onWidthChanged so the host can persist it (ui-state, processor state...).
// Selection/state inside the content is the host's business: collapsing
// only hides the component (setVisible), never destroys it.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/Theme.h>
#include <eiffelbs/Fonts.h>

namespace ebs
{

class Sidebar : public juce::Component
{
public:
    /** Which outer edge carries the handle strip. Right = content on the
        right side of the host (collapse pushes right); Left mirrors it. */
    enum class Edge { Right, Left };

    static constexpr int railWidth() noexcept { return 18; }

    explicit Sidebar (Edge edge = Edge::Right)
        : side (edge)
    {
        addAndMakeVisible (handle);
        handle.owner = this;
        // Default cursor is the resize affordance; the handle refines it
        // per zone on mouseMove (hand over the chevron). Collapsed, the
        // whole rail is the toggle -> hand cursor (set in setCollapsed).
        handle.setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        syncChevron();
    }

    /** Host content: fills the body, hidden (not destroyed) on collapse. */
    void setContent (juce::Component* c)
    {
        if (content == c)
            return;
        if (content != nullptr)
            removeChildComponent (content);
        content = c;
        if (content != nullptr)
        {
            addAndMakeVisible (content);
            content->setVisible (! collapsed);
        }
        handle.toFront (false);
        resized();
    }

    void setCollapsed (bool on)
    {
        if (collapsed == on)
            return;
        collapsed = on;
        if (content != nullptr)
            content->setVisible (! collapsed);
        syncChevron();
        // Cursor follows the mode: toggle-only rail when collapsed.
        handle.setMouseCursor (collapsed
            ? juce::MouseCursor::PointingHandCursor
            : juce::MouseCursor::LeftRightResizeCursor);
        if (onCollapsed != nullptr)
            onCollapsed (collapsed);
        resized();
        relayoutParent();
    }

    bool isCollapsed() const noexcept { return collapsed; }

    /** User width of the EXPANDED body (rail strip excluded). Clamped
        to [minWidth, effectiveMaxWidth()]. Fires onWidthChanged when it
        changes. Also asks the PARENT to re-layout: the host reserves
        outerWidth() in its own resized(), so a drag must propagate upward
        (otherwise the sidebar paints inside stale bounds until the next
        app resize). Same for setCollapsed/setWidths.
        notify=false: silent re-clamp (host layout pass) - the host only
        persists USER-initiated widths, never clamp-backs. */
    void setSidebarWidth (int w, bool notify = true)
    {
        const int clamped = juce::jlimit (minWidth, effectiveMaxWidth(), w);
        if (bodyWidth == clamped)
            return;
        bodyWidth = clamped;
        if (notify && onWidthChanged != nullptr)
            onWidthChanged (bodyWidth);
        resized();
        relayoutParent();
    }

    int getSidebarWidth() const noexcept { return bodyWidth; }

    void setWidths (int minW, int maxW, int defaultW)
    {
        minWidth = juce::jmax (60, minW);
        // maxW <= 0 = NO fixed cap: the width is bounded by
        // maxWidthFraction of the parent instead (e.g. half the window).
        fixedMaxWidth = maxW;
        bodyWidth = juce::jlimit (minWidth, effectiveMaxWidth(), defaultW);
        resized();
        relayoutParent();
    }

    /** Fraction of the PARENT width the expanded body may take (0..1,
        default 0.5): the cap follows the window size instead of a fixed
        pixel max. Ignored when a fixed max was set via setWidths(). */
    void setMaxWidthFraction (float f)
    {
        maxWidthFraction = juce::jlimit (0.1f, 0.95f, f);
        // Re-clamp silently: the fraction change is host policy, not a
        // user drag - must not overwrite the persisted user width.
        setSidebarWidth (bodyWidth, false);
    }

    /** Total outer width the host should reserve (body + rail), or just
        the rail when collapsed. The host calls this in resized(). */
    int outerWidth() const noexcept
    {
        return collapsed ? railWidth() : bodyWidth + railWidth();
    }

    /** Host callbacks (persist width / collapsed in ui-state...). */
    std::function<void (int newWidth)> onWidthChanged;
    std::function<void (bool collapsed)> onCollapsed;

    void resized() override
    {
        // The cap follows the window: clamp the body BEFORE laying out
        // so a shrink of the parent pulls an over-wide sidebar back.
        bodyWidth = juce::jlimit (minWidth, effectiveMaxWidth(), bodyWidth);
        auto b = getLocalBounds();
        auto strip = (side == Edge::Right) ? b.removeFromLeft (railWidth())
                                           : b.removeFromRight (railWidth());
        handle.setBounds (strip);
        if (content != nullptr)
            content->setBounds (collapsed ? juce::Rectangle<int>() : b);
    }

    // === Test hooks (smoke test only) ===================================

    juce::Rectangle<int> handleBoundsForTest() const
    {
        return handle.getBounds();
    }

private:
    /** Edge handle rail: a click (no drag) toggles collapse; a press+move
        past the dead zone resizes the body. Lives on the CONTENT side of
        the strip so the chevron sits against the panel it collapses. The
        rail splits into two hit zones: the chevron square (collapse click,
        hand cursor) and the rest of the rail (width drag, resize cursor).
        Collapsed, the whole rail is just the toggle (no resize feedback:
        there is no body to resize). */
    struct Handle : public juce::Component
    {
        Sidebar* owner = nullptr;
        // collapsedState = "chevron points LEFT". Set by syncChevron().
        bool collapsedState = false;

        /** Chevron hit square (centred): the collapse/expand target. */
        juce::Rectangle<int> chevronZone() const
        {
            const auto c = getLocalBounds().getCentre();
            return juce::Rectangle<int> (c.x - 9, c.y - 9, 18, 18);
        }

        bool overChevron (const juce::MouseEvent& e) const
        {
            return chevronZone().contains (e.getPosition());
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (ebs::bgDark());
            g.fillRect (b);
            // Hover/drag separation feedback: a 2 px accent line on the
            // CONTENT side of the rail (the visual split between the
            // sidebar and the rest of the window). EXPANDED ONLY: when
            // collapsed there is no body to resize, the rail is purely
            // the expand toggle.
            const bool expanded = owner != nullptr && ! owner->collapsed;
            const bool active = expanded
                && (isMouseOverOrDragging() || isMouseButtonDown());
            if (active && owner != nullptr)
            {
                g.setColour (ebs::accent());
                const float x = (owner->side == Edge::Right)
                    ? b.getX() + 1.0f : b.getRight() - 1.0f;
                g.fillRect (x - 1.0f, 0.0f, 2.0f, b.getHeight());
            }
            g.setColour (active ? ebs::accent() : ebs::textDim());
            const auto cx = b.getCentreX();
            const auto cy = b.getCentreY();
            juce::Path chev;
            chev.addTriangle (cx - 3.0f, cy - 5.0f,
                              cx + 3.0f, cy,
                              cx - 3.0f, cy + 5.0f);   // ">" glyph
            if (collapsedState)
                chev.applyTransform (juce::AffineTransform::rotation (
                    juce::MathConstants<float>::pi, cx, cy));
            g.fillPath (chev);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragStartX = e.getScreenPosition().x;
            dragStartWidth = owner != nullptr ? owner->bodyWidth : 300;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (owner == nullptr || owner->collapsed)
                return;                            // collapsed: no resize
            if (e.getDistanceFromDragStart() < 4)
                return;                            // dead zone: click later
            repaint();                             // keep the accent line lit
            const int dx = e.getScreenPosition().x - dragStartX;
            // Right-edge sidebar: dragging LEFT grows the body.
            const int delta = (owner->side == Edge::Right) ? -dx : dx;
            owner->setSidebarWidth (dragStartWidth + delta);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            repaint();                             // drop the accent line
            if (owner == nullptr)
                return;
            // Real click (inside the dead zone): toggle collapse.
            if (e.getDistanceFromDragStart() < 4
                && ! e.mouseWasDraggedSinceMouseDown())
                owner->setCollapsed (! owner->collapsed);
        }

        void mouseEnter (const juce::MouseEvent&) override
        {
            updateCursor (getMouseXYRelative());
            repaint();
        }

        void mouseExit (const juce::MouseEvent&) override { repaint(); }

        void mouseMove (const juce::MouseEvent& e) override
        {
            updateCursor (e.getPosition());
        }

        /** Cursor follows the zone: hand over the chevron (collapse
            affordance), resize cursor on the drag rail, plain arrow when
            collapsed (toggle only, no body to resize). */
        void updateCursor (juce::Point<int> pos)
        {
            if (owner != nullptr && owner->collapsed)
            {
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
                return;
            }
            setMouseCursor (chevronZone().contains (pos)
                ? juce::MouseCursor::PointingHandCursor
                : juce::MouseCursor::LeftRightResizeCursor);
        }

        int dragStartX = 0, dragStartWidth = 300;
    };

    // Right edge: expanded = chevron points right (collapse pushes right
    // into the edge); collapsed = points left (expand pulls back). Left
    // edge mirrors.
    void syncChevron()
    {
        const bool expanded = ! collapsed;
        handle.collapsedState = (side == Edge::Right) ? ! expanded
                                                      : expanded;
        handle.repaint();
    }

    /** Ask the parent to re-run its layout: the host positions us with
        outerWidth() in ITS resized(), so our own resized() is not enough
        - without this the new width only shows after an app-level resize.
        Guarded: resizing the parent re-sets our bounds (same size -> the
        host's resized() is cheap and idempotent). */
    void relayoutParent()
    {
        if (auto* p = getParentComponent())
            p->resized();
    }

    /** Upper width bound: fixed max when setWidths() got maxW > 0, else
        maxWidthFraction of the parent width (default = half the host).
        No parent yet (smoke/headless) -> falls back to the fixed max. */
    int effectiveMaxWidth() const
    {
        if (fixedMaxWidth > 0)
            return juce::jmax (minWidth, fixedMaxWidth);
        if (auto* p = getParentComponent())
            return juce::jmax (minWidth,
                (int) ((float) p->getWidth() * maxWidthFraction));
        return juce::jmax (minWidth, fallbackMaxWidth);
    }

    Edge side = Edge::Right;
    Handle handle;
    juce::Component* content = nullptr;   // host-owned, never deleted here
    bool collapsed = false;
    int bodyWidth = 300;
    int minWidth = 180;
    int fixedMaxWidth = 0;                // > 0 = fixed px cap (setWidths)
    int fallbackMaxWidth = 520;          // no-parent (headless) upper bound
    float maxWidthFraction = 0.5f;       // of parent width otherwise

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sidebar)
};

} // namespace ebs
