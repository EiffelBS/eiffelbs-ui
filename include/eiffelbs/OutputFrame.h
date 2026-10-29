// OutputFrame.h
// The shared OUTPUT block (TTS / Voice / SFX), following the SFX frame exactly:
// a FramedBody + FrameTitleHeader band, a single-channel band window carrying
// the waveform with the FREE ZONE selector, the zone readout, a Name row, an
// Enhance row (chain + Use Enhanced + receipt) and ONE transport row
// (play, hand, Add).
//
// The host owns the audio chain: it supplies the current wav, the enhanced wav
// and the export (name + zone + enhanced). Play, Add and the hand all ask the
// SAME export, so what sounds and what is filed can never disagree.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/Waveform.h"
#include "eiffelbs/FramedBody.h"
#include "eiffelbs/FrameTitleHeader.h"
#include "eiffelbs/ClearableEditor.h"
#include "eiffelbs/PillButton.h"
#include "eiffelbs/IconButton.h"
#include "eiffelbs/FileGripButton.h"

#include "eiffelbs/WaveformRegionSelector.h"

namespace ebs
{

class OutputFrame : public juce::Component,
                    private juce::Timer
{
public:
    struct Bindings
    {
        std::function<juce::File()> currentWav;
        std::function<juce::File()> enhancedWav;
        std::function<void()> onEnhance;
        /** Play a file FROM a position (seconds). */
        std::function<void (const juce::File&, double)> onPlayFile;
        /** Transport position in FILE seconds (-1 = stopped). Kept for hosts
            that want it; the frame now runs its own clock. */
        std::function<double()> position;
        /** Length of the CHOSEN file in seconds (0 = unknown). */
        std::function<double()> duration;
        std::function<void()> onStop;
        std::function<juce::File (const juce::String& name, double start,
                                  double end, bool useEnhanced)> exportFile;
        std::function<void (const juce::File&)> onAddToTakes;
        std::function<juce::String()> defaultName;
        std::function<void (const juce::String&)> log;
    };

    explicit OutputFrame (Bindings b);

    void setAudio (const juce::File& wav);
    void setEnhancedFile (const juce::File& wav);
    /** The primary action's label ("Add to Takes" / "Add to Sample Library"). */
    void setActionLabel (const juce::String& text)
    {
        addButton.setButtonText (text);
        resized();
    }

    /** Optional host widget shown between the drag handle and the action
        (SFX's tag chips). Not owned: the host keeps its lifetime; a null
        pointer removes the slot. */
    void setExtraComponent (juce::Component* component, int preferredWidth = 0,
                            int preferredRowHeight = 28)
    {
        if (extra == component)
        {
            extraWidth = preferredWidth;
            extraHeight = preferredRowHeight;
            resized();
            return;
        }
        if (extra != nullptr && extra->getParentComponent() == &frame)
            frame.removeChildComponent (extra);
        extra = component;
        extraWidth = preferredWidth;
        extraHeight = preferredRowHeight;
        if (extra != nullptr)
        {
            frame.addAndMakeVisible (*extra);
            extra->setVisible (true);
        }
        resized();
    }

    void setPlaying (bool playing);
    /** Auto-play a fresh result through the frame (so the green fill shows). */
    void playNow() { if (! playingNow) playFrom (0.0); }
    void playFrom (double secondsSec);
    void timerCallback() override;
    void setPlayhead (double seconds, bool playing)
    {
        waveform.setFillStart (0.0);
        waveform.setPlayhead (seconds, playing
                                          ? ebs::Waveform::PlayheadState::Playing
                                          : ebs::Waveform::PlayheadState::Stopped);
    }
    void refreshFromHost();
    juce::File chosenFile() const;
    juce::File exportForAction();
    juce::String zoneText() const;

    /** The name the export will carry (the field's current text). */
    juce::String nameText() const { return nameEditor.getText().trim(); }
    bool useEnhancedForTest() const { return useEnhanced.getToggleState(); }
    bool hasZoneForTest() const { return selector.hasSelection(); }

    void paint (juce::Graphics& g) override { g.fillAll (ebs::bgPanel()); }
    void resized() override;

public:
    /** Exact height of band + body at the CURRENT settings (extras included):
        a host stacking the frame in a page must not guess it. */
    int preferredHeight() const
    {
        // MIRRORS applyFlow() row by row: zone(16) gap2 clip(62) gap6
        // enhance(26) gap6 name(26), plus gap6 + the extra row when present.
        // The old sum carried a phantom 32 px, which is exactly the empty
        // band the author reported under the tag row (2026-10-29).
        int body = 16 + 2 + 62 + 6 + 26 + 6 + 26;
        if (extra != nullptr)
            body += 6 + juce::jmax (28, extraHeight);
        return ebs::bandHeaderHeight() + 10 + 6 + body;   // top 10, bottom 6
    }

private:
    void applyFlow (const juce::Rectangle<int>& body, bool apply);
    void updateTransport();
    void updateZone();
    void reloadWaveform();

    Bindings bindings;
    ebs::FramedBody frame;
    ebs::FrameTitleHeader band;
    juce::Label zoneLabel;
    juce::Component clip;
    ebs::Waveform waveform;
    WaveformRegionSelector selector;
    juce::Label nameCaption;
    ebs::ClearableEditor nameEditor;
    // SPARKLE, not the wand glyph: the wand reads as three menu lines at
    // 14 px (author report 2026-10-29). Sparkle is this design system's "AI /
    // magic" mark.
    ebs::PillButton enhanceButton { ebs::IconButton::Shape::sparkle, "Enhance" };
    juce::ToggleButton useEnhanced { "Use Enhanced" };
    juce::Label enhanceStatus;
    ebs::IconButton playButton { ebs::IconButton::Shape::play };
    ebs::FileGripButton hand { [this] { return exportForAction(); } };
    ebs::PillButton addButton { ebs::IconButton::Shape::plus, "Add to Takes" };
    juce::Component* extra = nullptr;   // optional host widget (tag chips)
    int extraWidth = 0;
    int extraHeight = 30;
    bool playingNow = false;
    double sourceBase = 0.0;   // where the PLAYING file starts in the SOURCE
    double playEnd = 0.0;      // source seconds where playback must stop
    int graceTicks = 0;
    int waveChannels = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputFrame)
};

inline OutputFrame::OutputFrame (Bindings b) : bindings (std::move (b))
{
    addAndMakeVisible (frame);
    frame.addAndMakeVisible (band);
    band.setTitle ("Output");

    frame.addAndMakeVisible (zoneLabel);
    zoneLabel.setFont (ebs::fontHint());
    zoneLabel.setColour (juce::Label::textColourId, ebs::textDim());
    zoneLabel.setJustificationType (juce::Justification::centredRight);

    frame.addAndMakeVisible (clip);
    clip.setInterceptsMouseClicks (false, true);
    clip.addAndMakeVisible (waveform);
    waveform.setInterceptsMouseClicks (false, false);
    clip.addAndMakeVisible (selector);
    selector.setLabelStripVisible (false);   // the frame owns the zone readout
    selector.onSelectionChanged = [this] (double, double)
    { setPlaying (false); updateZone(); };
    selector.onSelectionCleared = [this] { setPlaying (false); updateZone(); };
    selector.onSeek = [this] (double seconds)
    {
        // A click sets the playhead and plays the CHOSEN version (enhanced when
        // selected) FROM THAT POSITION - a zone does not hijack a seek.
        if (playingNow)
            setPlaying (false);
        const auto file = chosenFile();
        if (file.existsAsFile() && bindings.onPlayFile)
        {
            sourceBase = 0.0;
            playEnd = bindings.duration ? bindings.duration() : 0.0;
            const auto from = juce::jmax (0.0, seconds);
            bindings.onPlayFile (file, from);
            playingNow = true;
            startTimerHz (30);
            updateTransport();
        }
    };

    frame.addAndMakeVisible (nameCaption);
    nameCaption.setText ("Name", juce::dontSendNotification);
    nameCaption.setFont (ebs::fontHint());
    nameCaption.setColour (juce::Label::textColourId, ebs::textDim());
    nameCaption.setJustificationType (juce::Justification::centredRight);
    frame.addAndMakeVisible (nameEditor);
    nameEditor.setFont (ebs::fontHint());
    nameEditor.setTextToShowWhenEmpty ("output name", ebs::textDim());

    frame.addAndMakeVisible (enhanceButton);
    enhanceButton.setFont (ebs::fontBody());
    enhanceButton.onClick = [this]
    { if (bindings.onEnhance) bindings.onEnhance(); };
    frame.addAndMakeVisible (useEnhanced);
    useEnhanced.setEnabled (false);   // no enhanced copy yet
    useEnhanced.setColour (juce::ToggleButton::textColourId, ebs::textDim());
    useEnhanced.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    useEnhanced.onClick = [this]
    { setPlaying (false); reloadWaveform(); updateZone(); };
    frame.addAndMakeVisible (enhanceStatus);
    enhanceStatus.setFont (ebs::fontHint());
    enhanceStatus.setColour (juce::Label::textColourId, ebs::textDim());
    enhanceStatus.setJustificationType (juce::Justification::centredLeft);
    enhanceStatus.setText ("not enhanced yet", juce::dontSendNotification);

    frame.addAndMakeVisible (playButton);
    playButton.onClick = [this]
    {
        if (playingNow)
            setPlaying (false);
        else
            playFrom (0.0);
    };
    frame.addAndMakeVisible (hand);
    hand.setTooltip ("Drag the selected zone (or the enhanced file) into "
                     "your DAW");
    frame.addAndMakeVisible (addButton);
    addButton.setFont (ebs::fontBody());
    addButton.onClick = [this]
    {
        const auto file = exportForAction();
        if (file.existsAsFile() && bindings.onAddToTakes)
            bindings.onAddToTakes (file);
    };

    setAudio ({});
}

inline juce::String OutputFrame::zoneText() const
{
    if (! selector.hasSelection())
        return "zone: whole file";
    const auto fmt = [] (double s)
    {
        const int t = (int) s;
        return juce::String (t / 60) + ":"
             + juce::String (t % 60).paddedLeft ('0', 2);
    };
    return "zone: " + fmt (selector.selectionStartSeconds()) + " - "
         + fmt (selector.selectionEndSeconds());
}

inline juce::File OutputFrame::chosenFile() const
{
    const auto enhanced = bindings.enhancedWav ? bindings.enhancedWav()
                                               : juce::File();
    return useEnhanced.getToggleState() && enhanced.existsAsFile()
               ? enhanced
               : (bindings.currentWav ? bindings.currentWav() : juce::File());
}

inline void OutputFrame::reloadWaveform()
{
    const auto wav = chosenFile();
    waveform.setSource (wav);
    selector.setTotalSeconds (0.0);
    selector.clearSelection (false);
    if (wav.existsAsFile())
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                formats.createReaderFor (wav)))
        {
            waveChannels = juce::jmax (1, (int) reader->numChannels);
            selector.setTotalSeconds (reader->lengthInSamples
                                          / juce::jmax (1.0, reader->sampleRate));
        }
    }
    updateZone();
    resized ();   // the mono/stereo band choice lives in the flow
}

inline void OutputFrame::setAudio (const juce::File&)
{
    // Reset the ENHANCE choice FIRST: reloadWaveform() displays chosenFile(),
    // and a tick left from the previous result made the NEW render show the
    // PREVIOUS enhanced waveform while the transport played the new file -
    // clicking Enhance was the only thing that refreshed it (author report
    // 2026-10-29).
    useEnhanced.setToggleState (false, juce::dontSendNotification);
    useEnhanced.setEnabled (false);
    reloadWaveform();
    const auto base = chosenFile();
    if (base.existsAsFile())
        nameEditor.setText (bindings.defaultName ? bindings.defaultName()
                                                 : base.getFileNameWithoutExtension(),
                            juce::dontSendNotification);
    enhanceStatus.setText (base.existsAsFile() ? base.getFileName()
                                               : "no audio yet",
                           juce::dontSendNotification);
}

inline void OutputFrame::setEnhancedFile (const juce::File& wav)
{
    if (wav.existsAsFile())
    {
        useEnhanced.setEnabled (true);
        useEnhanced.setToggleState (true, juce::dontSendNotification);
        enhanceStatus.setText ("enhanced: " + wav.getFileName(),
                               juce::dontSendNotification);
        reloadWaveform();
    }
    else
        enhanceStatus.setText ("enhance failed", juce::dontSendNotification);
}

inline void OutputFrame::refreshFromHost() { reloadWaveform(); }

inline void OutputFrame::setPlaying (bool playing)
{
    if (playing == playingNow)
        return;
    if (! playing)
    {
        playingNow = false;
        sourceBase = 0.0;
        playEnd = 0.0;
        waveform.setFillStart (0.0);
        stopTimer();
        if (bindings.onStop)
            bindings.onStop();
        updateTransport();
        return;
    }
    playFrom (0.0);
}

inline void OutputFrame::playFrom (double secondsSec)
{
    const auto file = chosenFile();
    if (! file.existsAsFile() || ! bindings.onPlayFile)
        return;
    if (selector.hasSelection())
    {
        // SFX's proven route: cut the zone ONCE to a scratch wav, play THAT
        // (offset 0) and map the playhead back with sourceBase - which is why
        // SFX never had a playback/visual mismatch.
        const auto zoneStart = selector.selectionStartSeconds();
        const auto zoneEnd = selector.selectionEndSeconds();
        if (zoneEnd <= zoneStart)
            return;
        const auto cut = bindings.exportFile
                             ? bindings.exportFile (nameEditor.getText().trim(),
                                                    zoneStart, zoneEnd,
                                                    useEnhanced.getToggleState())
                             : file;
        if (! cut.existsAsFile())
            return;
        bindings.onPlayFile (cut, 0.0);
        sourceBase = zoneStart;
        playEnd = zoneEnd;
    }
    else
    {
        const auto from = juce::jmax (0.0, secondsSec);
        bindings.onPlayFile (file, from);
        sourceBase = 0.0;
        playEnd = bindings.duration ? bindings.duration() : 0.0;
    }
    playingNow = true;
    graceTicks = 0;
    updateTransport();
    startTimerHz (30);
}

inline void OutputFrame::timerCallback()
{
    if (! playingNow)
    {
        stopTimer();
        return;
    }
    const auto pos = bindings.position ? bindings.position() : -1.0;
    if (pos < 0.0)
    {
        if (++graceTicks < 4)
            return;
        playingNow = false;
        stopTimer();
        updateTransport();
        return;
    }
    graceTicks = 0;
    // `pos` is the position in the FILE ON THE TRANSPORT; the waveform shows
    // the SOURCE, so a zone cut is shifted by its start (SFX's rule).
    const auto shown = sourceBase + pos;
    if (playEnd > 0.0 && shown >= playEnd - 1.0e-4)
    {
        setPlaying (false);
        return;
    }
    waveform.setFillStart (0.0);
    waveform.setPlayhead (shown, ebs::Waveform::PlayheadState::Playing);
}

inline void OutputFrame::updateTransport()
{
    playButton.setShape (playingNow ? ebs::IconButton::Shape::stop
                                    : ebs::IconButton::Shape::play);
    playButton.repaint();
}

inline void OutputFrame::updateZone()
{
    zoneLabel.setText (zoneText(), juce::dontSendNotification);
}

inline juce::File OutputFrame::exportForAction()
{
    const auto chosen = chosenFile();
    if (! chosen.existsAsFile())
        return {};
    const auto start = selector.hasSelection()
                           ? selector.selectionStartSeconds() : 0.0;
    const auto end = selector.hasSelection()
                         ? selector.selectionEndSeconds() : 0.0;
    auto name = nameEditor.getText().trim();
    if (name.isEmpty())
        name = chosen.getFileNameWithoutExtension();
    return bindings.exportFile
               ? bindings.exportFile (name, start, end,
                                      useEnhanced.getToggleState())
               : chosen;
}

inline void OutputFrame::resized()
{
    frame.setBounds (getLocalBounds());
    band.setBounds (0, 0, frame.getWidth(), ebs::bandHeaderHeight());
    applyFlow (frame.getLocalBounds()
                   .withTrimmedTop (ebs::bandHeaderHeight())
                   .reduced (10)
                   .withTrimmedBottom (4), true);   // bottom pad 6, not 10
}

inline void OutputFrame::applyFlow (const juce::Rectangle<int>& area, bool apply)
{
    int y = area.getY();
    const int x = area.getX();
    const int w = area.getWidth();
    auto placeIn = [&] (juce::Component& c, juce::Rectangle<int> r)
    { if (apply) c.setBounds (r); };
    auto gap = [&] (int h) { y += h; };

    placeIn (zoneLabel, { x, y, w, 16 });
    y += 16;
    gap (2);

    const int bandH = 62;
    placeIn (clip, { x, y, w, bandH });
    if (apply)
    {
        clip.setVisible (true);
        if (waveChannels <= 1)
        {
            // A MONO file drawn through the stereo one-channel window only
            // showed its bottom half (author report 2026-10-28): give mono the
            // full band.
            waveform.setBounds (0, 0, w, bandH);
        }
        else
        {
            const int widgetH = 2 * bandH + 2;
            waveform.setBounds (0, -(1 + bandH), w, widgetH);
        }
        selector.setBounds (0, -ebs::Waveform::kLabelStripHeight, w,
                            bandH + ebs::Waveform::kLabelStripHeight);
        selector.toFront (false);
    }
    y += bandH;
    gap (6);

    auto enhanceRow = juce::Rectangle<int> (x, y, w, 26);
    placeIn (enhanceButton, enhanceRow.removeFromLeft (104));
    enhanceRow.removeFromLeft (8);
    placeIn (useEnhanced, enhanceRow.removeFromLeft (132));
    enhanceRow.removeFromLeft (8);
    placeIn (enhanceStatus, enhanceRow);
    y += 26;
    gap (6);

    // NAME row: play, hand, NAME (and Add when there is no host extra).
    auto actionRow = juce::Rectangle<int> (x, y, w, 26);
    placeIn (playButton, actionRow.removeFromLeft (26).withHeight (26));
    actionRow.removeFromLeft (6);
    placeIn (hand, actionRow.removeFromLeft (26).withHeight (26));
    actionRow.removeFromLeft (8);
    if (extra == nullptr)
        placeIn (addButton, actionRow.removeFromRight (150).withHeight (26));
    if (extra == nullptr)   // keep the gap before Add; with an extra row the
        actionRow.removeFromRight (8);   // NAME ends exactly where Add ends
    placeIn (nameCaption, actionRow.removeFromLeft (44));
    placeIn (nameEditor, actionRow.withTrimmedLeft (4)
                                .withHeight (24)
                                .withY (actionRow.getY() + 1));
    y += 26;
    if (extra != nullptr)
    {
        // The host extra (SFX's tag chips) sits on the row BELOW Name, with
        // the action at its right (author request 2026-10-29).
        gap (6);
        auto extraRow = juce::Rectangle<int> (x, y, w,
                                              juce::jmax (28, extraHeight));
        placeIn (addButton, extraRow.removeFromRight (150).withHeight (26));
        extraRow.removeFromRight (8);
        placeIn (*extra, extraRow);
        y += juce::jmax (28, extraHeight);
    }
}

} // namespace ebs
