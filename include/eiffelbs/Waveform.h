// Waveform.h
// eiffelbs-ui - shared waveform preview with time labels and a selection
// band (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A compact audio preview that answers three questions at a glance: how long
// is it, where am I in it, and which part is selected. It replaces an
// an in-house component whose API could not tell "no audio" from
// "still loading" and could not tell "the transport finished" from "it is
// still at zero" - both were one double with a sentinel, and both had to be
// worked around by the hosts. Here they are explicit.
//
// STATES
//   ReadyState::Empty     no source, or the source has no decodable audio
//   ReadyState::Loading   source set, thumbnail still decoding
//   ReadyState::Ready     thumbnail holds the file
//   PlayheadState         Stopped / Playing / Finished
//
// The playhead is a POSITION plus a STATE, not a position that doubles as one:
// a finished preview and one that never started both sit at 0.0, and a host
// that reads the number alone cannot tell them apart.
//
// TIME LABELS
//   current time top-left, total top-right, m:ss (h:mm:ss past an hour). The
//   current-time label is hidden below a width where "0:32 / 3:07" on one
//   small widget is noise: a mini-player shows the total, a wide take shows
//   both. setCurrentTimeVisible (Auto | On | Off) overrides the rule.
//   A mini showing only the total can show the REMAINING instead
//   (setTotalLabelMode): stopped it reads the total, playing it counts
//   down (total - playhead, "-" prefixed, the DAW convention), so one
//   number answers "how much is left".
//
// SELECTION
//   A band [start, end) in seconds, drawn over the waveform. Drag inside it to
//   MOVE it; drag either EDGE (a few px) to RESIZE that side; double-click to
//   clear it; click outside it to seek. The edges are real affordances, not a
//   decoration: the voice-cloning window needs a 30-second window the user can
//   drag around and shorten from either end.
//   onSelectionChanged fires when the drag ENDS, not on every pixel - a host
//   that needs the live value polls selectionStart()/selectionEnd().
//
// The thumbnail cache is OWNED by the component by default. A host showing
// many rows (a take list) may inject one cache with setCache so the rows share
// a bounded slot pool instead of each keeping its own.

// NOT IN THE UMBRELLA eiffelbs.h, ON PURPOSE. This header pulls
// <juce_audio_utils> for juce::AudioThumbnail, so it compiles the whole
// juce_audio_devices module inside every TU that includes the umbrella. In a
// plugin tree whose TUs include <windows.h> without NOMINMAX (the ARA SDK and
// the VST3 plugin-client chain), that drags in JUCE calls to std::min WITHOUT
// parentheses which the Windows min/max macros expand into std::(...) and MSVC
// fails (C2589 + C2059). Include this header directly and link
// juce::juce_audio_utils. See the note at the top of eiffelbs.h.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"

namespace ebs
{

    class Waveform : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
    {
    public:
        enum class ReadyState { Empty, Loading, Ready };
        enum class PlayheadState { Stopped, Playing, Finished };

        /** What the top-right label shows. Total is the default everywhere;
            Remaining is for the minis that show that number alone: stopped
            it reads the total, otherwise it counts down. */
        enum class TotalLabelMode { Total, Remaining };

        /** Below this width the current-time label is hidden in Auto mode:
         * "0:32" and "3:07" on a 120 px mini-player is noise, the total is the
         * only number that fits and the only one a reader wants there. */
        static constexpr int kAutoCurrentTimeMinWidth = 140;

        /** How close to a selection edge (px) a drag must be to resize it
         * rather than move the band. */
        static constexpr int kEdgeGrabPx = 5;

        /** The top strip the time labels occupy. A host that lays an
         * overlay over this widget - a selection window, a zoom map -
         * reserves this height in its own paint so the numbers stay
         * visible: a parent paints UNDER its children, so the labels
         * cannot be drawn over an overlay the host adds on top. */
        static constexpr int kLabelStripHeight = 16;

        /** `sharedCache` is optional: a host showing many rows passes the one
            cache so the rows share a bounded slot pool. nullptr = this widget
            owns its cache, and the cache's lifetime is then the widget's own
            (which is what makes destruction order safe - a function-local
            static cache dies in whatever order the C++ runtime picks, and
            JUCE's leak detector notices).

            The cache is fixed for the widget's life: AudioThumbnail takes it
            by reference at construction and offers no setter, so a cache that
            could be swapped under a live thumbnail would be a cache whose
            entries outlive the object that owns them. */
        Waveform (juce::AudioThumbnailCache* sharedCache = nullptr)
            : cache (sharedCache != nullptr ? sharedCache : &ownCache),
              thumbnail (kThumbnailSamples, formats(), *cache)
        {
            setOpaque (false);
        }

        ~Waveform() override { stopTimer(); }

        // === Source =========================================================

        /** The audio to show. An empty file, or one with no decodable audio,
            answers ReadyState::Empty - which is NOT Loading: a take with no
            audio will never finish loading, and a component that cannot tell
            the two apart paints a "coming" line forever. */
        void setSource (const juce::File& f)
        {
            file = f;
            playhead = -1.0;
            state = PlayheadState::Stopped;
            selection = {};
            ready = file.existsAsFile() ? ReadyState::Loading : ReadyState::Empty;
            if (file.existsAsFile())
            {
                thumbnail.setSource (new juce::FileInputSource (file));
                if (thumbnail.isFullyLoaded())
                    ready = ReadyState::Ready;
                else
                    startTimerHz (kLoadPollHz);
            }
            repaint();
        }

        juce::File sourceFile() const { return file; }

        ReadyState readyState() const noexcept { return ready; }
        double totalSeconds() const noexcept
        {
            return ready == ReadyState::Ready ? thumbnail.getTotalLength() : 0.0;
        }

        // === Look ===========================================================

        /** Colour of the unplayed waveform. */
        void setAccent (juce::Colour c)
        {
            if (accent == c) return;
            accent = c;
            repaint();
        }

        /** Where the PLAYED fill starts (seconds). 0 is the old behaviour;
            a host that starts playback mid-file (a zone, a seek) sets this so
            the green fill begins at the playhead instead of at the beginning
            (author report 2026-10-28). */
        void setFillStart (double seconds) noexcept
        {
            fillStartSec = juce::jmax (0.0, seconds);
            repaint();
        }

        /** Colour of the already-played part and of the playhead line. */
        void setPlayedColour (juce::Colour c)
        {
            if (played == c) return;
            played = c;
            repaint();
        }

        /** The selection band's fill (translucent) and its edge handles. */
        void setSelectionColours (juce::Colour fill, juce::Colour edge)
        {
            selectionFill = fill;
            selectionEdge = edge;
            repaint();
        }

        /** Auto: hide the current-time label when the widget is narrow.
         * On/Off: force it. The total-time label is always shown - it is the
         * one number every size of widget can afford. */
        void setCurrentTimeVisible (bool autoOn) noexcept
        {
            showCurrent = autoOn ? 1 : 0;
        }
        void setCurrentTimeForced (bool on) noexcept
        {
            showCurrent = on ? 2 : -2;
        }

        /** Total (default): the label reads the file length. Remaining:
            stopped it still reads the length, but a live transport counts
            down (length - playhead, "-" prefixed). One call, at most one
            repaint, only when the mode actually changes. */
        void setTotalLabelMode (TotalLabelMode m) noexcept
        {
            if (totalMode == m)
                return;
            totalMode = m;
            repaint();
        }

        // === Playhead =======================================================

        /** Where the transport is, in SECONDS into the file. -1 hides it.
            Finished is a state, not a position: a host that has no transport
            yet can leave the playhead at -1 and the label at 0:00 without
            lying about the take. */
        void setPlayhead (double seconds, PlayheadState s)
        {
            const double clamped = seconds < 0.0 ? 0.0
                                                 : juce::jmin (seconds,
                                                               totalSeconds());
            if (std::fabs (playhead - clamped) > kPlayheadEpsilon
                || state != s)
            {
                playhead = clamped;
                state = s;
                repaint();
            }
        }

        double playheadSeconds() const noexcept { return playhead; }
        PlayheadState playheadState() const noexcept { return state; }

        // === Selection ======================================================

        /** The band in seconds. Clamped to the file; a reversed or empty range
            clears it. Fires onSelectionChanged when the change comes from the
            user, not from this call - a programmatic set must not look like a
            gesture. */
        void setSelection (double start, double end)
        {
            const double total = totalSeconds();
            if (total <= 0.0 || start < 0.0 || end <= start || end > total)
            {
                if (selection)
                    clearSelection();
                return;
            }
            selection = { start, end };
            updateSelectionBounds();
            repaint();
        }

        void clearSelection()
        {
            if (! selection)
                return;
            selection = {};
            updateSelectionBounds();
            repaint();
            if (onSelectionChanged != nullptr)
                onSelectionChanged (0.0, 0.0);
        }

        bool hasSelection() const noexcept { return selection.has_value(); }
        double selectionStart() const noexcept
        {
            return selection ? selection->first : 0.0;
        }
        double selectionEnd() const noexcept
        {
            return selection ? selection->second : 0.0;
        }

        // === Cache ==========================================================

        // The cache is a CONSTRUCTOR argument, not a setter: see the ctor.
        // A host that wants to share one pool builds the widgets with the same
        // cache object and owns it for as long as they live.

        // === Callbacks ======================================================

        /** A seek request, in seconds. Fires on click and at the end of a drag
            that started outside the band. */
        std::function<void (double seconds)> onSeek;

        /** The band changed by the user, in seconds. (0,0) = cleared. */
        std::function<void (double start, double end)> onSelectionChanged;

        // === Test hooks ======================================================

        juce::Rectangle<int> selectionRectForTest() const noexcept
        {
            return selectionBounds;
        }
        double xToSecondsForTest (int x) const noexcept
        {
            return xToSeconds (x);
        }

    private:
        static constexpr int kThumbnailSamples = 512;
        static constexpr int kLoadPollHz = 8;
        static constexpr double kPlayheadEpsilon = 0.01;

        // A single format registry for every widget, and a default cache the
        // component owns (so destruction order is the component's own, not the
        // C++ runtime's idea of a good time).
        static juce::AudioFormatManager& formats()
        {
            static juce::AudioFormatManager mgr;
            static const bool once = []
                { mgr.registerBasicFormats(); return true; } ();
            juce::ignoreUnused (once);
            return mgr;
        }

        void timerCallback() override
        {
            if (thumbnail.isFullyLoaded())
            {
                stopTimer();
                ready = ReadyState::Ready;
                repaint();
            }
        }

        // === Geometry =======================================================

        juce::Rectangle<int> paintArea() const noexcept
        {
            return getLocalBounds().reduced (1);
        }

        /** x (widget-local) -> seconds into the file. 0 when there is no file. */
        double xToSeconds (int x) const noexcept
        {
            const auto area = paintArea();
            const double total = totalSeconds();
            if (total <= 0.0 || area.getWidth() <= 0)
                return 0.0;
            const double frac = juce::jlimit (0.0, 1.0,
                (double) (x - area.getX()) / (double) area.getWidth());
            return frac * total;
        }

        /** seconds -> x (widget-local). */
        int secondsToX (double seconds) const noexcept
        {
            const auto area = paintArea();
            const double total = totalSeconds();
            if (total <= 0.0)
                return area.getX();
            return area.getX() + (int) std::round (
                area.getWidth() * juce::jlimit (0.0, 1.0, seconds / total));
        }

        // === Paint ==========================================================

        void paint (juce::Graphics& g) override
        {
            g.fillAll (ebs::bgPanel());
            const auto area = paintArea();
            const double total = totalSeconds();

            if (ready == ReadyState::Ready && total > 0.0)
            {
                // The selection band UNDER the waveform, then the waveform,
                // then the played part: three layers, each clipped to its own
                // geometry, so nothing is ever drawn over a region it does not
                // describe.
                if (selection)
                {
                    const auto band = selectionBounds;
                    g.setColour (selectionFill);
                    g.fillRect (band);
                    g.setColour (selectionEdge);
                    g.fillRect (band.getX(), band.getY(), 2, band.getHeight());
                    g.fillRect (band.getRight() - 2, band.getY(), 2,
                                band.getHeight());
                }

                g.setColour (accent);
                thumbnail.drawChannels (g, area, 0.0, total, kThumbnailGain);

                if (state != PlayheadState::Stopped && playhead >= 0.0)
                {
                    const double t = juce::jlimit (0.0, total, playhead);
                    const double from = juce::jlimit (0.0, total,
                                                      fillStartSec);
                    const int startX = secondsToX (from);
                    const int playedW = juce::jmax (0,
                        secondsToX (t) - startX + 1);
                    if (playedW > 0 && t > from)
                    {
                        g.saveState();
                        g.reduceClipRegion (juce::Rectangle<int> (
                            startX, area.getY(), playedW,
                            area.getHeight()));
                        g.setColour (played);
                        thumbnail.drawChannels (g, area, 0.0, total,
                                                kThumbnailGain);
                        g.restoreState();
                    }
                    g.setColour (played);
                    g.fillRect (secondsToX (t), area.getY(), 1,
                                area.getHeight());
                }
            }
            else if (ready == ReadyState::Loading)
            {
                // A median line is the honest "something is coming" - the same
                // shape as a flat take, which is what a partially decoded
                // thumbnail looks like before its first block lands.
                g.setColour (accent.withAlpha (0.4f));
                const int y = (int) (area.getY() + area.getHeight() * 0.5);
                g.drawHorizontalLine (y, 2.0f, (float) area.getRight() - 2.0f);
            }
            // Empty: flat background, on purpose. A line here would promise an
            // audio that does not exist.

            paintTimeLabels (g);
        }

        void paintTimeLabels (juce::Graphics& g)
        {
            const auto area = paintArea();
            const double total = totalSeconds();

            // Current time: hidden in Auto when the widget is too narrow for
            // both numbers to mean anything.
            const bool wantCurrent = showCurrent == 2
                || (showCurrent == 1 && getWidth() >= kAutoCurrentTimeMinWidth);
            if (wantCurrent)
            {
                const double shown = state == PlayheadState::Stopped
                                         && playhead < 0.0 ? 0.0 : playhead;
                paintLabel (g, formatTime (shown), area.getX() + 4, true);
            }
            if (total > 0.0)
            {
                // Remaining mode: a live transport counts down (length -
                // playhead, "-" prefixed so a shrinking number never reads
                // as a shrinking file); stopped it reads the length, which
                // IS the remaining. Total mode always reads the length.
                const bool counting = totalMode == TotalLabelMode::Remaining
                                   && state != PlayheadState::Stopped
                                   && playhead >= 0.0;
                const juce::String text = counting
                    ? juce::String ("-")
                      + formatTime (juce::jmax (0.0, total - playhead))
                    : formatTime (total);
                paintLabel (g, text, area.getRight() - 4, false);
            }
        }

    public:
        /** m:ss, or h:mm:ss past an hour. Public: a host that shows the same
            number in a label wants the same formatting, not a second copy of
            it. */
        static juce::String formatTime (double seconds)
        {
            const int s = (int) std::floor (juce::jmax (0.0, seconds));
            const int h = s / 3600;
            if (h > 0)
                return juce::String (h) + ":"
                     + juce::String (s / 60 % 60).paddedLeft ('0', 2) + ":"
                     + juce::String (s % 60).paddedLeft ('0', 2);
            return juce::String (s / 60) + ":"
                 + juce::String (s % 60).paddedLeft ('0', 2);
        }

    private:
        void paintLabel (juce::Graphics& g, const juce::String& text,
                         int x, bool left) const
        {
            g.setFont (ebs::fontHint());
            const int w = (int) std::ceil (
                ebs::fontHint().getStringWidth (text)) + 10;
            const int h = 14;
            auto box = juce::Rectangle<int> (x, 2, w, h);
            if (! left)
                box.setX (x - w);
            // A translucent panel, so the number stays legible over a loud
            // waveform without claiming a surface of its own.
            g.setColour (ebs::bgDark().withAlpha (0.72f));
            g.fillRoundedRectangle (box.toFloat(), 4.0f);
            g.setColour (ebs::textDim());
            g.drawText (text, box, juce::Justification::centred, true);
        }

        // === Gestures =======================================================

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (ready != ReadyState::Ready || totalSeconds() <= 0.0)
                return;
            const auto edge = edgeAt (e.position);
            if (edge != 0)
            {
                drag = Drag::resize;
                resizeEdge = edge;
                dragStartX = e.position.x;
            }
            else if (selection && selectionBounds.contains (
                                        (int) e.position.x,
                                        (int) e.position.y))
            {
                drag = Drag::move;
                dragStartX = e.position.x;
            }
            else
            {
                drag = Drag::seek;
                if (onSeek != nullptr)
                    onSeek (xToSeconds ((int) e.position.x));
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (drag == Drag::none)
                return;
            if (drag == Drag::seek)
            {
                if (onSeek != nullptr)
                    onSeek (xToSeconds ((int) e.position.x));
                return;
            }
            const double total = totalSeconds();
            if (total <= 0.0 || ! selection)
                return;
            const double secPerPx = total
                                  / (double) juce::jmax (1,
                                                         paintArea().getWidth());
            const double delta = (double) (e.position.x - dragStartX) * secPerPx;
            dragStartX = e.position.x;

            if (drag == Drag::move)
            {
                double start = selection->first + delta;
                double end = selection->second + delta;
                const double span = selection->second - selection->first;
                if (start < 0.0) { start = 0.0; end = span; }
                if (end > total) { end = total; start = total - span; }
                selection = { start, end };
            }
            else
            {
                if (resizeEdge < 0)
                {
                    double start = juce::jlimit (0.0, selection->second,
                                                 selection->first + delta);
                    selection = { start, selection->second };
                }
                else
                {
                    double end = juce::jlimit (selection->first, total,
                                               selection->second + delta);
                    selection = { selection->first, end };
                }
            }
            updateSelectionBounds();
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (drag == Drag::none)
                return;
            const Drag what = drag;
            drag = Drag::none;
            // A selection gesture reports ONCE, at the end: a host rebuilding a
            // take list on every pixel of a drag is a host that drops rows.
            if (what == Drag::move || what == Drag::resize)
            {
                if (onSelectionChanged != nullptr)
                    onSelectionChanged (selection->first, selection->second);
            }
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (selection)
                clearSelection();
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const auto edge = edgeAt (e.position);
            if (edge != 0)
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            else if (selection && selectionBounds.contains ((int) e.position.x,
                                                            (int) e.position.y))
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            else
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        /** -1 = left edge, 1 = right edge, 0 = not on an edge. */
        int edgeAt (juce::Point<float> p) const noexcept
        {
            if (! selection)
                return 0;
            const auto b = selectionBounds;
            const int x = (int) p.x, y = (int) p.y;
            if (y < b.getY() || y >= b.getBottom())
                return 0;
            if (std::abs (x - b.getX()) <= kEdgeGrabPx)
                return -1;
            if (std::abs (x - b.getRight()) <= kEdgeGrabPx)
                return 1;
            return 0;
        }

        void resized() override
        {
            updateSelectionBounds();
            SettableTooltipClient::setTooltip (
                onSeek != nullptr
                    ? "Click to seek; drag the selection to move it, drag an "
                      "edge to resize it, double-click to clear it"
                    : juce::String());
        }

        void updateSelectionBounds()
        {
            selectionBounds = {};
            if (! selection)
                return;
            const int x0 = secondsToX (selection->first);
            const int x1 = secondsToX (selection->second);
            const auto area = paintArea();
            selectionBounds = { x0, area.getY(),
                                juce::jmax (2, x1 - x0), area.getHeight() };
        }

        enum class Drag { none, seek, move, resize };

        // ownCache is declared FIRST and cache SECOND: the thumbnail
        // is built from *cache, so both must exist before it, and
        // reverse declaration order then destroys the thumbnail first
        // and the cache last - the one ordering a thumbnail and its
        // cache may have. Declaring the thumbnail first (as a first
        // draft did) built it against an UNCONSTRUCTED cache, and the
        // first setSource() read it: a crash on load. The ordering is
        // the code, not a comment.
        juce::AudioThumbnailCache ownCache { 8 };
        juce::AudioThumbnailCache* cache = &ownCache;
        juce::AudioThumbnail thumbnail;
        juce::File file;
        ReadyState ready = ReadyState::Empty;
        PlayheadState state = PlayheadState::Stopped;
        double playhead = -1.0;
        juce::Colour accent { ebs::accent() };
        double fillStartSec = 0.0;   // played fill begins here
        /** The played part. A GREEN, not a brighter accent: the accent is a
            host choice (blue for a current take, grey for an old one) and the
            played part has to read as "played" in both, which a brighter grey
            does not. */
        juce::Colour played { juce::Colour::fromRGB (86, 196, 118) };
        juce::Colour selectionFill { ebs::accent().withAlpha (0.20f) };
        juce::Colour selectionEdge { ebs::accent() };
        int showCurrent = 1;              // 1 = Auto, 2 = forced on, <0 = off
        TotalLabelMode totalMode = TotalLabelMode::Total;
        static constexpr float kThumbnailGain = 0.9f;
        std::optional<std::pair<double, double>> selection;
        juce::Rectangle<int> selectionBounds;
        Drag drag = Drag::none;
        int dragStartX = 0;
        int resizeEdge = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Waveform)
    };

} // namespace ebs
