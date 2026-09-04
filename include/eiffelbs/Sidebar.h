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
        if (onCollapsed != nullptr)
            onCollapsed (collapsed);
        resized();
        relayoutParent();
    }

    bool isCollapsed() const noexcept { return collapsed; }

    /** User width of the EXPANDED body (rail strip excluded). Clamped
        to [minWidth, maxWidth]. Fires onWidthChanged when it changes.
        Also asks the PARENT to re-layout: the host reserves outerWidth()
        in its own resized(), so a drag must propagate upward (otherwise
        the sidebar paints inside stale bounds until the next app resize).
        Same for setCollapsed/setWidths. */
    void setSidebarWidth (int w)
    {
        const int clamped = juce::jlimit (minWidth, maxWidth, w);
        if (bodyWidth == clamped)
            return;
        bodyWidth = clamped;
        if (onWidthChanged != nullptr)
            onWidthChanged (bodyWidth);
        resized();
        relayoutParent();
    }

    int getSidebarWidth() const noexcept { return bodyWidth; }

    void setWidths (int minW, int maxW, int defaultW)
    {
        minWidth = juce::jmax (60, minW);
        maxWidth = juce::jmax (minWidth, maxW);
        bodyWidth = juce::jlimit (minWidth, maxWidth, defaultW);
        resized();
        relayoutParent();
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
        the strip so the chevron sits against the panel it collapses. */
    struct Handle : public juce::Component
    {
        Sidebar* owner = nullptr;
        // collapsedState = "chevron points LEFT". Set by syncChevron().
        bool collapsedState = false;

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (ebs::bgDark());
            g.fillRect (b);
            g.setColour (ebs::textDim());
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
            // Hover affordance: accent chevron under the cursor.
            if (isMouseOver())
            {
                g.setColour (ebs::accent());
                g.fillPath (chev);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragStartX = e.getScreenPosition().x;
            dragStartWidth = owner != nullptr ? owner->bodyWidth : 300;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (owner == nullptr)
                return;
            if (e.getDistanceFromDragStart() < 4)
                return;                            // dead zone: click later
            const int dx = e.getScreenPosition().x - dragStartX;
            // Right-edge sidebar: dragging LEFT grows the body.
            const int delta = (owner->side == Edge::Right) ? -dx : dx;
            owner->setSidebarWidth (dragStartWidth + delta);
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            if (owner == nullptr)
                return;
            // Real click (inside the dead zone): toggle collapse.
            if (e.getDistanceFromDragStart() < 4
                && ! e.mouseWasDraggedSinceMouseDown())
                owner->setCollapsed (! owner->collapsed);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

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

    Edge side = Edge::Right;
    Handle handle;
    juce::Component* content = nullptr;   // host-owned, never deleted here
    bool collapsed = false;
    int bodyWidth = 300;
    int minWidth = 180, maxWidth = 520;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sidebar)
};

} // namespace ebs
