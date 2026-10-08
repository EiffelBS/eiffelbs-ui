// BandGrip.h
// eiffelbs-ui - reusable vertical resize strip for a bottom band.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The host owns the band geometry and supplies the current height. The grip
// only owns the interaction chrome: drag, clamping, reset, and relayout.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <limits>
#include <eiffelbs/Theme.h>

namespace ebs
{

class BandGrip : public juce::Component
{
public:
    static constexpr int defaultGripHeight() noexcept { return 14; }

    BandGrip()
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (false);
    }

    void setHeightRange (int minHeight, int maxHeight)
    {
        minH = juce::jmax (1, minHeight);
        maxH = juce::jmax (minH, maxHeight);
        currentH = juce::jlimit (minH, maxH, currentH);
    }

    void setHeightSilently (int height) noexcept
    {
        currentH = juce::jlimit (minH, maxH, height);
    }

    int getHeightValue() const noexcept { return currentH; }

    std::function<void (int)> onHeightChanged;
    std::function<void ()> onDragFinished;
    std::function<void ()> onReset;

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto colour = (isMouseOverOrDragging() || isMouseButtonDown())
            ? accent() : textDim();
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (b.getCentreX() - 20.0f,
                                b.getCentreY() - 2.0f,
                                40.0f, 4.0f, 2.0f);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStartHeight = currentH;
        dragStartY = e.getScreenY();
        dragged = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 2)
            return;

        // The grip is on the band's top edge: moving upward grows the band.
        const int requested = dragStartHeight + dragStartY - e.getScreenY();
        dragged = true;
        setHeight (requested, true);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragged && onDragFinished != nullptr)
            onDragFinished();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (onReset != nullptr)
            onReset();
    }

    int clampForTest (int height) const noexcept
    {
        return juce::jlimit (minH, maxH, height);
    }

private:
    void setHeight (int height, bool notify)
    {
        const int clamped = juce::jlimit (minH, maxH, height);
        if (currentH == clamped)
            return;
        currentH = clamped;
        if (notify && onHeightChanged != nullptr)
            onHeightChanged (currentH);
        relayoutParent();
    }

    void relayoutParent()
    {
        if (auto* p = getParentComponent())
            p->resized();
    }

    int minH = 1;
    int maxH = (std::numeric_limits<int>::max)();
    int currentH = 1;
    int dragStartHeight = 1;
    int dragStartY = 0;
    bool dragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandGrip)
};

} // namespace ebs
