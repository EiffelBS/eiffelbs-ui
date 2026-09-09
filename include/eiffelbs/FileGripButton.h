// FileGripButton.h
// eiffelbs-ui - generic external-file drag handle (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Ported from OpenTimbre source/ui/StemsPanel.h StemGripButton (original
// copyright (C) 2026 EiffelBS): open-hand cursor; once the drag passes
// ~8 px from the mouse-down point it starts the NATIVE external file
// drag of the resolved file (towards DAW/desktop). The path is resolved
// AT DRAG TIME through the caller-provided resolver (never captured at
// build time), so a recycled row component can never drag a stale file.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "eiffelbs/IconButton.h"

namespace ebs
{

class FileGripButton : public IconButton
{
public:
    explicit FileGripButton (std::function<juce::File()> fileResolver)
        : IconButton (Shape::grip), resolveFile (std::move (fileResolver))
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStarted || e.getDistanceFromDragStart() < 8)
            return;                      // dead zone before the real drag
        dragStarted = true;
        const auto f = (resolveFile != nullptr) ? resolveFile() : juce::File();
        if (! f.existsAsFile())
            return;
        // allowMove = true: the DAW may import/move the file natively.
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            { f.getFullPathName() }, true, this);
    }

    void mouseUp (const juce::MouseEvent&) override { dragStarted = false; }

private:
    std::function<juce::File()> resolveFile;
    bool dragStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileGripButton)
};

} // namespace ebs
