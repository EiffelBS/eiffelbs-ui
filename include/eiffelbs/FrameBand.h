// FrameBand.h
// eiffelbs-ui - a horizontal band of frames that keeps its blocks at their
// preferred width, and scrolls when they no longer fit.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY A BAND AND NOT A STRETCH
//
// A row of frames squeezed into a shrinking width dies two ways: the last
// block gets stretched to fill, or every block is compressed until its
// widgets collide. Both are the same failure - the row has no notion of a
// floor. This band keeps each frame at the width its own content asks for:
// spare room is left empty at the right edge and never handed to a block,
// because a stretched frame is a frame whose layout no longer matches its
// content. "Growable" therefore means ONLY "may be shrunk toward its floor
// before the row scrolls". Scroll is the escape valve below the hard floor,
// never the primary mechanism.
//
// SCROLL VOCABULARY, BORROWED FROM THE PILL
//
// The same language as PillTabSwitch, because a user who learned one reads
// the other: eased motion (kSmoothing at 60 Hz), a cue that only earns its
// paint once content is genuinely hidden past 4 px, hover-edge auto-scroll
// at 30 px from the edge and 5 px per tick (~300 px/s), and the chevron
// glyph shared with DisclosureChevron - one tip direction for both.
//
// TOUCH AND TOUCHPAD
//
// A wheel over a frame body pans the band; a wheel over a knob keeps
// changing the knob, which is what a touchpad user wants (two fingers on a
// slider must not move the row). The rule is a walk up the parent chain: if
// anything between the widget that got the event and the band consumes the
// wheel, the band stays put.
//
// A touch drag pans the band ONLY when it lands on a frame's own body. A
// press on a knob, combo, button or label inside a frame belongs to that
// widget.
//
// TWO DISPATCHER QUIRKS THAT SET THE SHAPE OF THE GATE
//
// A deep listener (addMouseListener (this, true)) is called with the event
// retargeted to the nearest ancestor: Component::HierarchyChecker builds the
// event with BOTH eventComponent and originalComponent set to that ancestor,
// so the deepest widget that was pressed is NOT in the event, and the x/y
// inside it are still relative to the original widget. Both facts are used
// here: the press target comes from e.source.getComponentUnderMouse(), the
// only place that still names the deepest widget, and coordinates are taken
// through getEventRelativeTo (this), never read raw.
//
// CLIPPING
//
// Frames are children of the band, so a frame sliding past the edge is cut
// by the band bounds exactly as a viewport cuts its content - no viewport,
// no scrollbar strip stealing height. The cue chevrons are children added
// last and kept toFront: a parent paints before its children, so a cue
// painted by the band itself would be covered by the frames it is supposed
// to sit over.
//
// THE SCROLL STRIP
//
// A mouse user needs to SEE that the row has more in it, and to reach that
// part without hunting with the wheel. So a hairline strip sits at the band's
// bottom edge, painted ONLY while content is hidden: a thumb as long as the
// share of the row the view shows, sitting where the row currently is. It is
// a child component, never a painted region - a painted strip cannot catch the
// press, and a press nobody catches is a press that lands on the frame under
// it. Dragging the thumb maps pointer to row 1:1; a press in the trough away
// from the thumb moves one view width toward that side, the same unit a cue
// tap uses. The strip costs no height: it lies over the frames' own bottom
// padding, so nothing below it has to make room.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <vector>

#include "eiffelbs/Theme.h"
#include "eiffelbs/DisclosureChevron.h"

namespace ebs
{
    // juce::Component IS a juce::MouseListener, so the handlers below are
    // overrides of Component's own virtuals: adding MouseListener as a second
    // base makes `addMouseListener (this)` an ambiguous conversion.
    class FrameBand : public juce::Component,
                      private juce::Timer
    {
    public:
        explicit FrameBand (int gap = 10)
        {
            bandGap = juce::jmax (0, gap);

            // The band owns its clicks so the frames inside get them, and takes
            // no keyboard focus: it is a track, not a tab stop.
            setInterceptsMouseClicks (true, true);
            setWantsKeyboardFocus (false);

            // Events from every nested child, so the pan gate can see who got
            // the press.
            addMouseListener (this, true);

            // Z-order in JUCE is the child index: 0 is the BACK, the last entry is
            // the front. The strip is added first here and pushed to the front by
            // addFrame below, so it always paints over the frames it belongs to.
            addAndMakeVisible (track);

            leftCue.pointsLeft = true;   // the dissolve solidifies at the left edge
            leftCue.expanded = true;   // points LEFT: content is off-screen that way
            addAndMakeVisible (leftCue);
            addAndMakeVisible (rightCue);
            leftCue.toFront (false);
            rightCue.toFront (false);

            leftCue.onToggle  = [this] { glide (-1); };
            rightCue.onToggle = [this] { glide (+1); };

            startTimerHz (60);
        }

        ~FrameBand() override
        {
            stopTimer();
            removeMouseListener (this);
        }

        //==============================================================================
        /** Register a frame, left to right. The frame becomes a child of the band;
         *  every widget already inside it stays where it is.
         *
         *  preferredW is the width its content asks for, minW the floor below which
         *  the widgets collide. A frame is laid out at preferredW. growable says
         *  whether this block may be shrunk toward minW when the row runs out of
         *  room, before it starts to scroll; spare room is never given to a block. */
        void addFrame (juce::Component* frame, int preferredW, int minW, bool growable = true)
        {
            if (frame == nullptr)
                return;

            entries.push_back (Entry { frame, juce::jmax (1, preferredW), juce::jmax (1, minW), growable });
            addAndMakeVisible (*frame);

            // The strip and the cues must stay in front of every frame added after
            // them: the last child index is the front, so re-front in the order the
            // layers should read - strip under the cues.
            track.toFront (false);
            leftCue.toFront (false);
            rightCue.toFront (false);

            relayout();
        }

        void removeFrame (juce::Component* frame)
        {
            for (size_t i = 0; i < entries.size(); ++i)
                if (entries[i].frame == frame)
                {
                    entries.erase (entries.begin() + static_cast<int> (i));
                    break;
                }

            if (frame != nullptr)
                removeChildComponent (frame);

            relayout();
        }

        /** Re-state a frame's widths after its own state changed - an
         *  ExpansibleFrame that just opened is the case this exists for. The band
         *  re-lays out immediately. Pass growable = false for a block whose width
         *  is fixed by its content, so it keeps its preferred width and is never
         *  squeezed below it. */
        void setFrameWidths (juce::Component* frame, int preferredW, int minW, bool growable = true)
        {
            for (auto& e : entries)
                if (e.frame == frame)
                {
                    e.pref     = juce::jmax (1, preferredW);
                    e.minW     = juce::jmax (1, minW);
                    e.growable = growable;
                }

            relayout();
        }

        /** The width at which nothing scrolls: every frame at its preferred width. */
        int preferredWidth() const noexcept { return totalWidth (true); }

        /** The hard floor: every frame at its minimum. Below this the band scrolls. */
        int minWidth() const noexcept { return totalWidth (false); }

        int gap() const noexcept { return bandGap; }
        void setGap (int newGap) { bandGap = juce::jmax (0, newGap); relayout(); }

        //==============================================================================
        int maxScroll() const noexcept { return juce::jmax (0, contentW - viewW); }
        int scrollPosition() const noexcept { return juce::roundToInt (scrollPos); }

        /** The scroll strip: its width, the length of its thumb, and where that thumb
         *  sits. Length and x are zero while the band has nothing to hide. */
        int trackWidth()   const noexcept { return track.getWidth(); }
        int thumbLength()  const noexcept { return computeThumbLength(); }
        int thumbX()       const noexcept { return computeThumbX(); }

        /** The strip, exposed so the host can put a tooltip on it. */
        juce::Component& scrollTrack() noexcept { return track; }

        /** True when content is genuinely hidden past that edge - the same 4 px
         *  rule the cue paint uses. */
        bool canScrollLeft()  const noexcept { return scrollPos > kCueHiddenPx; }
        bool canScrollRight() const noexcept { return scrollPos < static_cast<float> (maxScroll()) - kCueHiddenPx; }

        /** Set the eased target, clamped to what the band can actually show. */
        void scrollTo (int x) { targetScroll = juce::jlimit (0.0f, static_cast<float> (maxScroll()), static_cast<float> (x)); }

        /** The eased DESTINATION, what scrollTo / revealFrame / a cue tap asked for.
         *  scrollPosition() is where the band sits right now; this is where it is
         *  heading. A host that drives the band needs to read both. */
        int targetScrollPosition() const noexcept { return juce::roundToInt (targetScroll); }

        /** Jump straight to a position with NO easing: the case a host needs when it
         *  restores a saved layout, where animating from zero is noise. */
        void snapScroll (int x)
        {
            const int want = juce::roundToInt (juce::jlimit (0.0f, static_cast<float> (maxScroll()),
                                                      static_cast<float> (x)));
            targetScroll = scrollPos = static_cast<float> (want);
            applyScroll();
            updateChrome();
        }

        /** Reveal a frame with the smallest move that brings it fully inside the
         *  view - the same minimal-reveal rule as a tab strip. */
        void revealFrame (juce::Component* frame)
        {
            for (size_t i = 0; i < entries.size(); ++i)
                if (entries[i].frame == frame)
                {
                    const int left = baseX[i], right = baseX[i] + widths[i];
                    float want = targetScroll;

                    if (left < want)
                        want = static_cast<float> (left);
                    else if (right > want + viewW - kEdgePx)
                        want = static_cast<float> (right - viewW + kEdgePx);

                    scrollTo (juce::roundToInt (juce::jlimit (0.0f, static_cast<float> (maxScroll()), want)));
                    return;
                }
        }

        /** The cue strips, exposed so the host can put tooltips on them: a
         *  DisclosureChevron sets no default tooltip. */
        DisclosureChevron& leftCueStrip()  noexcept { return leftCue; }
        DisclosureChevron& rightCueStrip() noexcept { return rightCue; }

        //==============================================================================
        void resized() override { relayout(); }

        void paint (juce::Graphics&) override
        {
            // The band paints nothing. Every block in it is a component that
            // paints its own body, and the cues paint themselves. A painted
            // background here would be the ghost source this design is built to
            // remove.
        }

    private:
        struct Entry
        {
            juce::Component* frame = nullptr;
            int pref = 1, minW = 1;
            bool growable = true;
        };

        /** A cue strip: the dissolve band behind the shared chevron glyph. It is a
         *  DisclosureChevron so hover, the accent fill, the tap target and the tip
         *  direction all come from one place. */
        struct Cue : DisclosureChevron
        {
            bool pointsLeft = false;

            void paint (juce::Graphics& g) override
            {
                const auto b = getLocalBounds().toFloat();

                if (! b.isEmpty())
                {
                    // The band dissolves INTO the track: transparent inside, solid
                    // at the edge. The reverse reads as a wall cutting the frames,
                    // which is the inversion caught on screen in the pill.
                    const auto c = ebs::bgPanel();
                    const float cy = b.getCentreY();
                    const float inner = pointsLeft ? b.getRight()  : b.getX();
                    const float outer = pointsLeft ? b.getX()      : b.getRight();

                    juce::ColourGradient dissolve (c.withAlpha (0.0f),
                                                   juce::Point<float> (inner, cy),
                                                   c,
                                                   juce::Point<float> (outer, cy), false);
                    g.setGradientFill (dissolve);   // JUCE 8 has no fillGradient
                    g.fillRect (b);
                }

                DisclosureChevron::paint (g);   // hover accent + the shared glyph
            }
        };

        /** The scroll strip at the band's bottom edge. A child component, never a
         *  painted region: only a component can catch the press, and a press the
         *  band cannot catch is a press that lands on the frame underneath. It is
         *  a nested class, so it reads the band's own scroll state directly. */
        struct Track : juce::Component
        {
            explicit Track (FrameBand& owner) : band (owner)
            {
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
                setWantsKeyboardFocus (false);
            }

            void paint (juce::Graphics& g) override
            {
                const auto b = getLocalBounds().toFloat();

                if (b.isEmpty() || band.maxScroll() <= 0)
                    return;

                const bool active = isMouseOverOrDragging() || dragging;
                const auto col = active ? ebs::accent() : ebs::textDim();

                // The trough is a hairline: the strip must read as a track the frames
                // sit over, not as a bar laid on top of them.
                g.setColour (col.withAlpha (0.30f));
                g.fillRect (b.getX(), b.getCentreY() - 0.5f, b.getWidth(), 1.0f);

                const int tl = band.computeThumbLength();
                const int tx = band.computeThumbX();

                g.setColour (col.withAlpha (active ? 0.95f : 0.70f));
                g.fillRoundedRectangle (static_cast<float> (tx), b.getCentreY() - 2.0f,
                                        static_cast<float> (tl), 4.0f, 2.0f);
            }

            void mouseEnter (const juce::MouseEvent&) override { repaint(); }
            void mouseExit  (const juce::MouseEvent&) override { repaint(); }

            void mouseDown (const juce::MouseEvent& e) override
            {
                const int x  = e.getEventRelativeTo (this).x;
                const int tx = band.computeThumbX(), tl = band.computeThumbLength();

                dragging        = true;
                dragStartX      = x;
                dragStartScroll = band.scrollPosition();

                if (x >= tx && x <= tx + tl)
                    return;   // the thumb was grabbed: the drag maps 1:1 below

                // A press in the trough away from the thumb moves one view width
                // toward that side - the same unit a cue tap moves by.
                band.scrollTo (band.scrollPosition() + (x < tx ? -band.viewW : band.viewW));
                dragStartScroll = band.targetScrollPosition();
            }

            void mouseDrag (const juce::MouseEvent& e) override
            {
                const int room = band.trackWidth() - band.computeThumbLength();

                if (room <= 0 || band.maxScroll() <= 0)
                    return;

                const float scale = static_cast<float> (band.maxScroll()) / static_cast<float> (room);
                const int   dx    = e.getEventRelativeTo (this).x - dragStartX;

                band.scrollTo (dragStartScroll + juce::roundToInt (static_cast<float> (dx) * scale));
                repaint();
            }

            void mouseUp (const juce::MouseEvent&) override
            {
                dragging = false;
                repaint();
            }

        private:
            FrameBand& band;
            int dragStartX = 0, dragStartScroll = 0;
            bool dragging = false;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Track)
        };

        std::vector<Entry> entries;
        int bandGap = 10;
        int viewW = 0, contentW = 0;
        float scrollPos = 0.0f, targetScroll = 0.0f;
        int hoverX = -1;
        juce::Component* panTarget = nullptr;
        int panStartX = 0, panStartScroll = 0;
        bool panning = false;

        std::vector<int> baseX, widths;

        Cue leftCue, rightCue;
        Track track { *this };

        static constexpr float kSmoothing = 0.18f;   // ~150-200 ms settle at 60 Hz
        static constexpr float kHoverEdgePx = 30.0f;
        static constexpr float kHoverStepPx = 5.0f;  // ~300 px/s
        static constexpr float kCueHiddenPx = 4.0f;  // a cue earns its paint past 4 px hidden
        static constexpr int kEdgePx = 8;            // breathing room in a minimal reveal
        static constexpr int kCueW = 30;
        static constexpr int kMinTapW = 22;          // a cue stays a touch target
        static constexpr int kPanSlopPx = 10;        // a press that barely moves is not a pan
        static constexpr int kTrackH = 8;            // strip height, over the frames' padding
        static constexpr int kMinThumbPx = 24;       // shorter than this is not grabbable

        int totalWidth (bool usePref) const noexcept
        {
            if (entries.empty())
                return 0;

            int sum = 0;
            for (auto& e : entries)
                sum += usePref ? e.pref : e.minW;

            return sum + bandGap * (static_cast<int> (entries.size()) - 1);
        }

        /** Thumb length = the share of the row the view shows, of the strip width.
         *  Zero when there is nothing to hide; never below the grabbable floor. */
        int computeThumbLength() const noexcept
        {
            const int maxS = maxScroll();

            if (maxS <= 0 || track.getWidth() <= 0)
                return 0;

            const double frac = static_cast<double> (viewW) / static_cast<double> (contentW);
            const int len = juce::roundToInt (frac * static_cast<double> (track.getWidth()));

            return juce::jlimit (kMinThumbPx, track.getWidth(), len);
        }

        /** Thumb x inside the strip: 0 at rest, the far end at full scroll. */
        int computeThumbX() const noexcept
        {
            const int maxS = maxScroll();

            if (maxS <= 0)
                return 0;

            const int room = track.getWidth() - computeThumbLength();
            return juce::roundToInt (static_cast<double> (scrollPos) / static_cast<double> (maxS)
                                     * static_cast<double> (room));
        }

        /** Band-local x for an event handed to this band by the dispatcher. The
         *  coordinates inside a retargeted event are NOT converted, so reading e.x
         *  raw gives the x inside whatever widget was under the pointer. */
        int bandLocalX (const juce::MouseEvent& e)
        {
            return e.getEventRelativeTo (this).x;   // takes a non-const Component*
        }

        /** True when anything between the widget that got the event and this band
         *  consumes the wheel. A knob under the pointer must keep its wheel. */
        bool wheelIsConsumedBelow (const juce::Component* deepest) const
        {
            for (auto* c = deepest; c != nullptr && c != this; c = c->getParentComponent())
                if (dynamic_cast<juce::Slider*> (const_cast<juce::Component*> (c)) != nullptr
                 || dynamic_cast<juce::TextEditor*> (const_cast<juce::Component*> (c)) != nullptr)
                    return true;

            return false;
        }

        /** The width of the block nearest one edge of the view: the unit a wheel
         *  notch and a cue tap move by. */
        int stepWidth (bool fromRight) const
        {
            if (widths.empty())
                return 0;

            if (! fromRight)
                for (size_t i = 0; i < widths.size(); ++i)
                    if (baseX[i] + widths[i] > scrollPos)
                        return widths[i];

            for (int i = static_cast<int> (widths.size()) - 1; i >= 0; --i)
                if (baseX[i] < scrollPos + viewW)
                    return widths[i];

            return widths.back();
        }

        void glide (int dir)
        {
            // One tap, one frame width: the block nearest the edge that was tapped.
            const int step = stepWidth (dir > 0);

            if (step > 0)
                scrollTo (scrollPosition() + dir * step);
        }

        void relayout()
        {
            const auto area = getLocalBounds();
            viewW = area.getWidth();

            const int n = static_cast<int> (entries.size());

            if (n == 0)
            {
                contentW = 0;
                baseX.clear();
                widths.clear();
                leftCue.setVisible (false);
                rightCue.setVisible (false);
                return;
            }

            const int gaps = bandGap * (n - 1);
            int prefSum = 0, growRange = 0;

            for (const auto& e : entries)
            {
                prefSum += e.pref;

                if (e.growable && e.pref > e.minW)
                    growRange += e.pref - e.minW;
            }

            widths.assign (static_cast<size_t> (n), 1);

            // A frame gets exactly the width its content asks for. Spare room is left
            // empty at the right edge: stretching a block makes that block's own
            // layout lie about its content, which is the failure this band exists to end.
            for (int i = 0; i < n; ++i)
                widths[i] = entries[i].pref;

            if (viewW < prefSum + gaps)
            {
                // Not enough room: shrink the growable blocks toward their floor, each
                // giving in proportion to how much it can give. Non-growable blocks
                // keep their preferred width - that is all "growable" means, and once
                // every block sits at its floor the row scrolls.
                const int need = prefSum + gaps - viewW;

                for (int i = 0; i < n; ++i)
                {
                    const int range = entries[i].growable ? entries[i].pref - entries[i].minW : 0;
                    const long long give = growRange > 0
                        ? (static_cast<long long> (need) * range) / growRange : 0;
                    widths[i] = entries[i].pref - static_cast<int> (juce::jmin (static_cast<long long> (range), give));
                }
            }

            // The span that can scroll is what the widths actually add up to, not the
            // floor: a non-growable block keeps its preferred width, so the content
            // can be wider than minSum + gaps and the tail would never be reachable.
            int sum = 0;

            for (int w : widths)
                sum += w;

            contentW = juce::jmax (viewW, sum + gaps);

            baseX.assign (static_cast<size_t> (n), 0);
            int x = 0;

            for (int i = 0; i < n; ++i)
            {
                baseX[i] = x;
                x += widths[i] + bandGap;
            }

            targetScroll = juce::jlimit (0.0f, static_cast<float> (maxScroll()), targetScroll);
            scrollPos    = juce::jlimit (0.0f, static_cast<float> (maxScroll()), scrollPos);

            applyScroll();
            placeChrome (area);
        }

        void applyScroll()
        {
            const int h = getHeight();

            for (size_t i = 0; i < entries.size(); ++i)
                if (entries[i].frame != nullptr)
                    entries[i].frame->setBounds (baseX[i] - juce::roundToInt (scrollPos), 0, widths[i], h);
        }

        void placeChrome (juce::Rectangle<int> area)
        {
            const int w = juce::jmax (kMinTapW, juce::jmin (kCueW, static_cast<int> (area.getWidth() * 0.2f)));

            // The band is built with setWidth, never setX: Rectangle::setX keeps the
            // width, so a right cue built that way stretched a full band-width past
            // the edge and its gradient read as solid.
            leftCue.setBounds  (area.withWidth (w).reduced (0, 4));
            rightCue.setBounds (area.withX (area.getRight() - w).withWidth (w).reduced (0, 4));

            // The strip keeps 4 px at each edge so it never runs under a cue, and it
            // lies over the frames' bottom padding: no frame has to give height for it.
            track.setBounds (area.reduced (4, 0).withY (area.getBottom() - kTrackH).withHeight (kTrackH));

            updateChrome();
        }

        void updateChrome()
        {
            const bool showL = canScrollLeft(), showR = canScrollRight();

            if (leftCue.isVisible() != showL)
                leftCue.setVisible (showL);

            if (rightCue.isVisible() != showR)
                rightCue.setVisible (showR);

            const bool showTrack = maxScroll() > 0;

            if (track.isVisible() != showTrack)
                track.setVisible (showTrack);

            if (showTrack)
                track.repaint();   // the thumb moved, so its ink has to move with it
        }

        void timerCallback() override
        {
            const int maxS = maxScroll();

            // Hover-edge auto-scroll, the pill's rule: a pointer parked within 30 px
            // of an edge with room that way keeps pushing.
            if (hoverX >= 0 && maxS > 0)
            {
                if (hoverX < kHoverEdgePx && scrollPos > 0.0f)
                    targetScroll = juce::jmax (0.0f, targetScroll - kHoverStepPx);
                else if (hoverX > viewW - kHoverEdgePx && scrollPos < static_cast<float> (maxS))
                    targetScroll = juce::jmin (static_cast<float> (maxS), targetScroll + kHoverStepPx);
            }

            targetScroll = juce::jlimit (0.0f, static_cast<float> (maxS), targetScroll);

            if (std::abs (targetScroll - scrollPos) < 0.5f)
            {
                if (scrollPos != targetScroll)
                {
                    scrollPos = targetScroll;
                    applyScroll();
                    updateChrome();
                }

                return;
            }

            scrollPos += (targetScroll - scrollPos) * kSmoothing;
            applyScroll();
            updateChrome();
        }

        //==============================================================================
        void mouseDown (const juce::MouseEvent& e) override
        {
            // The press is a pan only when it landed on one of my frames' own
            // bodies. A press on a knob, combo, button or label inside a frame
            // belongs to that widget, and the deep listener cannot see which one:
            // the event handed to an ancestor has its component fields rewritten to
            // the ancestor, so the target is read from the mouse source.
            panning = false;
            panTarget = nullptr;

            const auto* deepest = e.source.getComponentUnderMouse();

            for (const auto& en : entries)
                if (deepest == en.frame)
                {
                    panTarget      = en.frame;
                    panStartX      = bandLocalX (e);
                    panStartScroll = targetScroll;
                    panning        = true;
                    return;
                }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! panning)
                return;

            const int dx = bandLocalX (e) - panStartX;

            if (std::abs (dx) < kPanSlopPx)
                return;   // a press that barely moves is not a pan

            targetScroll = juce::jlimit (0.0f, static_cast<float> (maxScroll()),
                                   static_cast<float> (panStartScroll) - static_cast<float> (dx));
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            panning = false;
            panTarget = nullptr;
        }

        void mouseMove (const juce::MouseEvent& e) override { hoverX = bandLocalX (e); }
        void mouseExit (const juce::MouseEvent&) override   { hoverX = -1; }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
        {
            const int maxS = maxScroll();

            if (maxS <= 0)
                return;

            // A knob under the pointer keeps its wheel: two fingers on a slider must
            // change the slider, not move the row. The wheel is consumed below the
            // band by any Slider or TextEditor in the chain that got the event.
            if (wheelIsConsumedBelow (e.source.getComponentUnderMouse()))
                return;

            const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;

            if (delta == 0.0f)
                return;

            const float dir = wheel.isReversed ? -1.0f : 1.0f;
            targetScroll = juce::jlimit (0.0f, static_cast<float> (maxS),
                                   targetScroll - dir * delta * static_cast<float> (stepWidth (false)));
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrameBand)
    };

} // namespace ebs
