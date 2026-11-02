// FileGripButton.h
// eiffelbs-ui - generic external-file drag handle (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Ported from an in-house stems panel StemGripButton (original
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

    /** Tag a receiving zone reads to tell OUR path payload from someone
        else's drag (a text selection, a toolbar item). */
    static constexpr const char* payloadTag = "opentimbre/file:";

    /** The payload this handle starts: ONE path, prefixed. */
    static juce::var payloadFor (const juce::File& f)
    {
        return juce::String (payloadTag) + f.getFullPathName();
    }

    /** The file a payload names, or {} when this is not a handle drag. */
    static juce::File fileOfPayload (const juce::var& description)
    {
        if (! description.isString())
            return {};
        const auto text = description.toString();
        if (! text.startsWith (payloadTag))
            return {};
        return juce::File (text.fromFirstOccurrenceOf (payloadTag, false, false));
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
        // AN INTERNAL drag carrying the path, with external file drops still
        // available through the container's
        // shouldDropFilesWhenDraggedExternally() override.
        //
        // Why not performExternalDragDropOfFiles(): that is an OS-level drag,
        // and the SAME application never receives it back as a file drop - the
        // handle could export to a DAW or the desktop but could not feed
        // another tab's drop zone (author report 2026-11-02, TTS -> Target
        // Voice). A JUCE drag is delivered to every DragAndDropTarget in the
        // app, and JUCE turns it back into a real file drag the moment the
        // pointer leaves the window.
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! f.getFullPathName().isEmpty())
                container->startDragging (payloadFor (f), this,
                                          juce::ScaledImage(),
                                          /*allowDraggingToExternalWindows*/ true);
            return;
        }
        // No container (a bare component in a test, or a host that has none):
        // fall back to the native drag so the handle never becomes inert.
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
