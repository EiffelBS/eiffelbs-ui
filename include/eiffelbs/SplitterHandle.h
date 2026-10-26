// SplitterHandle.h
// eiffelbs-ui - the draggable divider between two panels.
//
// WHY: a form with two big text fields (a system prompt above a template
// prompt) or a list beside a form needs a REAL handle, not a fixed ratio: the
// reader decides what is worth the space (author request 2026-10-26). BandGrip
// resizes a bottom band; this one splits two SIBLINGS, in either direction,
// with the same chrome language.
//
// The host owns the geometry: it places the handle between the two panels and
// supplies the current position (px of the FIRST panel). The handle owns only
// the interaction: drag, clamp, hover chrome, reset on double-click.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <eiffelbs/Theme.h>

namespace ebs
{

class SplitterHandle : public juce::Component
{
public:
    enum class Orientation
    {
        rows,    // a wide horizontal bar between stacked panels; drags VERTICALLY
        columns  // a narrow vertical bar between side-by-side panels; drags HORIZONTALLY
    };

    explicit SplitterHandle (Orientation o = Orientation::rows)
        : orientation (o)
    {
        setMouseCursor (orientation == Orientation::rows
                            ? juce::MouseCursor::UpDownResizeCursor
                            : juce::MouseCursor::LeftRightResizeCursor);
        setWantsKeyboardFocus (false);
    }

    /** The allowed span of the FIRST panel's size (px). */
    void setPositionRange (int minPosition, int maxPosition)
    {
        minPos = juce::jmax (0, minPosition);
        maxPos = juce::jmax (minPos, maxPosition);
        position = juce::jlimit (minPos, maxPos, position);
    }

    void setPositionSilently (int px) noexcept
    {
        position = juce::jlimit (minPos, maxPos, px);
    }

    int getPositionValue() const noexcept { return position; }

    /** Fired while dragging (the host re-lays out). */
    std::function<void (int)> onPositionChanged;
    /** Fired on mouse-up / reset. */
    std::function<void (int)> onDragFinished;

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool active = isMouseOverOrDragging() || isMouseButtonDown();
        g.setColour ((active ? accent() : textDim()).withAlpha (0.9f));
        if (orientation == Orientation::rows)
        {
            // Three short bars, centred: the reader's grip.
            const float cx = b.getCentreX();
            for (int i = -1; i <= 1; ++i)
                g.fillRoundedRectangle (cx + (float) i * 6.0f - 1.5f,
                                        b.getCentreY() - 1.5f, 3.0f, 3.0f,
                                        1.5f);
        }
        else
        {
            const float cy = b.getCentreY();
            for (int i = -1; i <= 1; ++i)
                g.fillRoundedRectangle (b.getCentreX() - 1.5f,
                                        cy + (float) i * 6.0f - 1.5f, 3.0f,
                                        3.0f, 1.5f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragStartPosition = position;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // The HOST re-lays out while we drag, so our own bounds move under the
        // pointer: an e.x/e.y delta is then measured from a moving origin and
        // the handle "fights" the re-layout (jumps - author report
        // 2026-10-26). getDistanceFromDragStart is measured from the MOUSE-DOWN
        // SCREEN position, which no re-layout can move.
        const int delta = orientation == Orientation::rows
                              ? e.getDistanceFromDragStartY()
                              : e.getDistanceFromDragStartX();
        nudge (delta);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onDragFinished != nullptr)
            onDragFinished (position);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        // Double-click restores the host's default (0 means "no opinion": the
        // host decides, we just announce it).
        if (onDragFinished != nullptr)
            onDragFinished (defaultPosition < 0 ? 0 : defaultPosition);
    }

    /** Test seam: the SAME path mouseDrag takes, without synthesising events. */
    void dragByForTest (int delta)
    {
        // Same anchor mouseDown() records: the delta is relative to where the
        // drag STARTED, not to an uninitialised origin.
        dragStartPosition = position;
        nudge (delta);
        if (onDragFinished != nullptr)
            onDragFinished (position);
    }

    int defaultPosition = -1;

private:
    void nudge (int delta)
    {
        const int target = juce::jlimit (minPos, maxPos, dragStartPosition + delta);
        if (target == position)
            return;
        position = target;
        repaint();
        if (onPositionChanged != nullptr)
            onPositionChanged (position);
    }

    Orientation orientation;
    int minPos = 0, maxPos = 0, position = 0;
    int dragStartPosition = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplitterHandle)
};

} // namespace ebs
