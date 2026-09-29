// BusyIndicator.h
// eiffelbs-ui - reusable indeterminate/determinate progress strip.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Wraps the platform progress control behind the shared theme. Applications
// can keep it running as an indeterminate strip, update its stage text, or
// switch to normalized progress without owning the progress value.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/Theme.h>
#include <eiffelbs/Fonts.h>

namespace ebs
{

class BusyIndicator final : public juce::Component
{
public:
    BusyIndicator()
    {
        progress.setPercentageDisplay (false);
        progress.setTextToDisplay ("Preparing...");
        progress.setColour (juce::ProgressBar::backgroundColourId, bgPanel());
        progress.setColour (juce::ProgressBar::foregroundColourId, accent());
        progress.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (progress);
    }

    /** Keep the strip in indeterminate mode and update its stage text. */
    void setStage (const juce::String& stage)
    {
        progressValue = -1.0;
        progress.setTextToDisplay (stage);
        repaint();
    }

    /** Switch to determinate mode with a value in [0, 1]. */
    void setProgress (double normalized)
    {
        progressValue = juce::jlimit (0.0, 1.0, normalized);
        repaint();
    }

    /** Clear the visual strip and restore the default stage. */
    void clear()
    {
        setStage ("Preparing...");
    }

    bool isIndeterminate() const noexcept { return progressValue < 0.0; }

    double getProgress() const noexcept { return progressValue; }

    void resized() override
    {
        progress.setBounds (getLocalBounds());
    }

    /** juce::ProgressBar starts/stops its 30 ms repaint timer from ITS OWN
        visibility change, and a hidden ANCESTOR never reaches it - so a
        BusyIndicator hidden as an overlay (TTS panel hotspot, status-bar
        claim slot) would keep ticking for the rest of the session. Forward
        ours, and the strip costs nothing while it is not shown. */
    void visibilityChanged() override
    {
        progress.setVisible (isVisible());
    }

    /** Test hook: the inner strip control's own visibility, i.e. whether
        the 30 ms animation timer is running. */
    bool stripControlVisibleForTest() const noexcept { return progress.isVisible(); }

private:
    double progressValue = -1.0;
    juce::ProgressBar progress { progressValue };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BusyIndicator)
};

} // namespace ebs
