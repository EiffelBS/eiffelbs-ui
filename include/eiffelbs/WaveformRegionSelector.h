#pragma once
/*
 * WaveformRegionSelector - a FREE range selection laid over the shared
 * ebs::Waveform.
 *
 * Why not reuse the voice-reference overlay: that one is a fixed-duration
 * WINDOW (10-30 s) that re-centres on a click outside it - a voice reference
 * is a window the engine wants, while a sample is an ARBITRARY zone of the
 * render. Here a drag defines both edges freely, a drag inside the band
 * moves it, a drag on an edge resizes that edge, a plain click seeks (the
 * TTS waveform's gesture) and a double click clears.
 *
 * The waveform stays passive (`setInterceptsMouseClicks(false, true)`); this
 * overlay owns the gesture and paints the band, so a host that only wants
 * click-to-seek simply does not attach a selector. The total length comes
 * from the host (`setTotalSeconds`), never from the thumbnail's load state:
 * the gesture must work the frame the render lands, not one block later.
 */
#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/Waveform.h>

#include <cmath>
#include <functional>

namespace ebs
{

class WaveformRegionSelector : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    WaveformRegionSelector()
    {
        setTooltip ("Drag to select the zone that will be added to the"
                    " library. Drag inside to move it, an edge to resize it,"
                    " click to seek, double-click to clear.");
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    }

    ~WaveformRegionSelector() override { detach(); }

    /** Become a child of `wave`, covering it. The waveform stops taking the
        mouse: this component owns the gesture. */
    void attach (ebs::Waveform& wave)
    {
        detach();
        waveRef = &wave;
        wave.setInterceptsMouseClicks (false, true);
        wave.addAndMakeVisible (*this);
        setBounds (wave.getLocalBounds());
        toFront (false);
    }

    void detach()
    {
        if (waveRef != nullptr)
        {
            waveRef->removeChildComponent (this);
            waveRef = nullptr;
        }
    }

    /** The source length the gesture maps against. 0 = no selection
        possible (nothing loaded): every gesture is ignored. */
    /** Hide the selector's OWN label strip (the zone readout above the band).
        An Output frame already writes the zone on its own row, so the strip's
        "Drag to select a zone" hint would sit over the waveform (SFX render
        proof, 2026-10-29). The band and the gestures are untouched. */
    void setLabelStripVisible (bool visible) noexcept
    {
        if (labelStripVisible == visible)
            return;
        labelStripVisible = visible;
        repaint();
    }

    void setTotalSeconds (double seconds) noexcept
    {
        const double next = (std::isfinite (seconds) && seconds > 0.0)
                                ? seconds : 0.0;
        if (juce::exactlyEqual (next, totalSec))
            return;
        totalSec = next;
        if (totalSec <= 0.0)
        {
            hasSel = false;
            startSel = endSel = 0.0;
        }
        else
        {
            startSel = juce::jlimit (0.0, totalSec, startSel);
            endSel = juce::jlimit (startSel, totalSec, endSel);
            hasSel = endSel - startSel > minimumSelectionSeconds();
        }
        repaint();
    }

    double totalSeconds() const noexcept { return totalSec; }
    bool hasSelection() const noexcept { return hasSel; }
    double selectionStartSeconds() const noexcept { return startSel; }
    double selectionEndSeconds() const noexcept { return endSel; }
    double selectionDurationSeconds() const noexcept
    {
        return hasSel ? juce::jmax (0.0, endSel - startSel) : 0.0;
    }

    /** Programmatic selection (an import preview, a fresh render). Never
        notifies: the host already knows what it set. */
    void setSelection (double startSeconds, double endSeconds)
    {
        if (totalSec <= 0.0)
            return;
        const double lo = juce::jlimit (0.0, totalSec, juce::jmin (startSeconds, endSeconds));
        const double hi = juce::jlimit (0.0, totalSec, juce::jmax (startSeconds, endSeconds));
        if (hi - lo <= minimumSelectionSeconds())
        {
            clearSelection (false);
            return;
        }
        startSel = lo;
        endSel = hi;
        hasSel = true;
        repaint();
    }

    void clearSelection (bool notify = true)
    {
        if (! hasSel && startSel == 0.0 && endSel == 0.0)
            return;
        hasSel = false;
        startSel = endSel = 0.0;
        repaint();
        if (notify && onSelectionCleared != nullptr)
            onSelectionCleared();
    }

    /** A drag finalised a range (start/end in seconds). */
    std::function<void (double, double)> onSelectionChanged;
    /** A plain click: seek the transport there (TTS waveform behaviour). */
    std::function<void (double)> onSeek;
    bool labelStripVisible = true;
    std::function<void()> onSelectionCleared;
    /** Optional veto: return false and the gesture is ignored. */
    std::function<bool()> selectionAllowed;

    // === Component =========================================================
    void paint (juce::Graphics& g) override
    {
        if (totalSec <= 0.0)
            return;
        const auto b = getLocalBounds().toFloat();
        const float top = (float) ebs::Waveform::kLabelStripHeight;
        const float h = b.getHeight() - top;
        if (h <= 0.0f)
            return;

        const float x0 = xFor (startSel);
        const float x1 = xFor (endSel);
        if (hasSel)
        {
            g.setColour (ebs::bgDark().withAlpha (0.55f));
            g.fillRect (0.0f, top, x0, h);
            g.fillRect (x1, top, b.getWidth() - x1, h);
            g.setColour (ebs::accent().withAlpha (0.12f));
            g.fillRect (x0, top, x1 - x0, h);
            g.setColour (ebs::accent());
            g.drawRect (juce::Rectangle<float> (x0, top, x1 - x0, h), 1.5f);
            g.fillRect (juce::Rectangle<float> (x0 - 1.5f, top, 3.0f, h));
            g.fillRect (juce::Rectangle<float> (x1 - 1.5f, top, 3.0f, h));

            if (labelStripVisible)
            {
            auto label = juce::String (startSel, 2) + " - " + juce::String (endSel, 2)
                       + " s  (" + juce::String (endSel - startSel, 2) + " s)";
            const auto font = ebs::fontHint();
            const int tw = (int) std::ceil (font.getStringWidthFloat (label)) + 10;
            int x = (int) x0 + 4;
            if (x + tw > getWidth() - 2)
                x = juce::jmax (2, getWidth() - tw - 2);
            const juce::Rectangle<int> tag (x, (int) top + 2, tw, 14);
            g.setColour (ebs::bgPanel().withAlpha (0.92f));
            g.fillRoundedRectangle (tag.toFloat(), 2.0f);
            g.setColour (ebs::accent());
            g.setFont (font);
            g.drawText (label, tag.reduced (4, 0),
                        juce::Justification::centredLeft, true);
            }
        }
        else
        {
            if (labelStripVisible)
            {
                g.setColour (ebs::accent().withAlpha (0.55f));
                g.setFont (ebs::fontHint());
                g.drawText ("Drag on the waveform to select a zone", 6,
                            (int) top + 2,
                        getWidth() - 12, 14,
                        juce::Justification::centredLeft, true);
            }
        }
    }

    void resized() override
    {
        // The overlay is a child of the waveform: follow it.
        if (waveRef != nullptr)
            setBounds (waveRef->getLocalBounds());
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = Drag::none;
        if (! allowed() || getWidth() <= 0)
            return;
        const double sec = secondsAt (e.position.x);
        downX = e.position.x;
        downSec = sec;
        savedStart = startSel;
        savedEnd = endSel;
        savedHadSelection = hasSel;
        dragDistance = 0.0f;

        const float edge = 6.0f;
        if (hasSel && std::abs (e.position.x - xFor (startSel)) <= edge)
        {
            drag = Drag::resizeStart;
            return;
        }
        if (hasSel && std::abs (e.position.x - xFor (endSel)) <= edge)
        {
            drag = Drag::resizeEnd;
            return;
        }
        if (hasSel && sec > startSel && sec < endSel)
        {
            drag = Drag::move;
            moveStart = startSel;
            moveEnd = endSel;
            return;
        }
        drag = Drag::create;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == Drag::none || ! allowed())
            return;
        dragDistance = juce::jmax (dragDistance,
                                   (float) e.getDistanceFromDragStart());
        const double sec = secondsAt (e.position.x);
        const double minLen = minimumSelectionSeconds();

        if (drag == Drag::create)
        {
            const double a = juce::jmin (downSec, sec);
            const double b = juce::jmax (downSec, sec);
            setSelection (a, b);
            return;
        }
        if (drag == Drag::resizeStart)
        {
            const double next = juce::jlimit (0.0, endSel - minLen, sec);
            startSel = juce::jmin (next, endSel - minLen);
            hasSel = true;
            repaint();
            return;
        }
        if (drag == Drag::resizeEnd)
        {
            const double next = juce::jlimit (startSel + minLen, totalSec, sec);
            endSel = juce::jmax (next, startSel + minLen);
            hasSel = true;
            repaint();
            return;
        }
        // move
        const double span = moveEnd - moveStart;
        const double delta = sec - downSec;
        double nextStart = juce::jlimit (0.0, juce::jmax (0.0, totalSec - span),
                                         moveStart + delta);
        startSel = nextStart;
        endSel = nextStart + span;
        hasSel = true;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (drag == Drag::none)
            return;
        const auto wasDrag = drag;
        drag = Drag::none;
        if (! allowed())
            return;

        if (wasDrag == Drag::create && dragDistance < 3.0f)
        {
            // A click seeks and leaves the previous zone alone (a selection
            // lost to a playhead move would be a nasty surprise).
            startSel = savedStart;
            endSel = savedEnd;
            hasSel = savedHadSelection;
            repaint();
            if (onSeek != nullptr)
                onSeek (secondsAt (e.position.x));
            return;
        }
        if (! hasSel || endSel - startSel <= minimumSelectionSeconds())
        {
            clearSelection (false);
            if (onSelectionCleared != nullptr)
                onSelectionCleared();
            return;
        }
        if (onSelectionChanged != nullptr)
            onSelectionChanged (startSel, endSel);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        clearSelection (true);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto cursor = juce::MouseCursor::CrosshairCursor;
        if (hasSel && getWidth() > 0)
        {
            const float edge = 6.0f;
            if (std::abs (e.position.x - xFor (startSel)) <= edge
                || std::abs (e.position.x - xFor (endSel)) <= edge)
                cursor = juce::MouseCursor::LeftRightResizeCursor;
            else if (secondsAt (e.position.x) > startSel
                     && secondsAt (e.position.x) < endSel)
                cursor = juce::MouseCursor::DraggingHandCursor;
        }
        if (getMouseCursor() != cursor)
            setMouseCursor (cursor);
    }

private:
    enum class Drag { none, create, move, resizeStart, resizeEnd };

    bool allowed() const noexcept
    {
        if (totalSec <= 0.0 || (selectionAllowed && ! selectionAllowed()))
            return false;
        return true;
    }

    static constexpr double minimumSelectionSeconds() noexcept { return 0.05; }

    double secondsAt (float x) const noexcept
    {
        if (getWidth() <= 0)
            return 0.0;
        return juce::jlimit (0.0, totalSec,
                             (double) x / (double) getWidth() * totalSec);
    }

    float xFor (double seconds) const noexcept
    {
        if (totalSec <= 0.0)
            return 0.0f;
        return (float) ((double) getWidth() * juce::jlimit (0.0, totalSec, seconds)
                        / totalSec);
    }

    ebs::Waveform* waveRef = nullptr;
    double totalSec = 0.0;
    double startSel = 0.0;
    double endSel = 0.0;
    bool hasSel = false;

    Drag drag = Drag::none;
    float downX = 0.0f;
    float dragDistance = 0.0f;
    double downSec = 0.0;
    double savedStart = 0.0;
    double savedEnd = 0.0;
    bool savedHadSelection = false;
    double moveStart = 0.0;
    double moveEnd = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformRegionSelector)
};

} // namespace ebs
