// PillTabSwitch.h
// eiffelbs-ui - animated pill tab strip with scroll, icons and edge cues
// (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A tab bar for a JUCE TabbedComponent whose native strip stays hidden: a
// rounded track with a sliding accent badge behind the active label. Ported
// from an in-house tab switch (itself ported from OpenVoxTuner's
// TabSwitch): the
// animation is NOT a ComponentAnimator but a 60 Hz Timer driving an
// exponentially smoothed position, which settles in ~150-200 ms with a
// natural ease-out.
//
// ONE fixed type size, slots sized to their labels, and a SCROLLING view:
// a slot is exactly as wide as its label (+ icon) needs at that size, and
// when the tab list is wider than the space given, the strip scrolls to
// keep the active tab in view instead of compressing the slots. Scroll
// drivers: click a visible slot, wheel over the pill, linger at an edge
// (also during an OS file drag), 1 s dwell on a slot during a file drag.
// Edge fades + chevrons announce hidden tabs; the tick follows the active
// tab on index CHANGE only, so manual scrolls are never fought.
//
// App-specific chrome stays OUT: short labels and glyphs arrive through
// onTabLabel / onTabIcon providers (default: full name, no icon), so an
// eighth tab reads the day it is added, just less compactly.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <optional>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"

namespace ebs
{

class PillTabSwitch : public juce::Component,
                      private juce::Timer,
                      public juce::FileDragAndDropTarget
{
public:
    explicit PillTabSwitch (juce::TabbedComponent& tabs)
        : tabbed (tabs)
        , pillPos (static_cast<float> (juce::jmax (0, tabs.getCurrentTabIndex())))
        , targetPillPos (pillPos)
    {
        setWantsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (60);          // OVT: 60 Hz animation clock
    }

    ~PillTabSwitch() override { stopTimer(); }

    /** Short display label for tab `i`. Set once, before first paint -
     *  the strip measures it. Default: the full tab name. App chrome
     *  (short names) stays in the app: the component only shortens what
     *  it is told to. */
    std::function<juce::String (int)> onTabLabel;
    /** Glyph for tab `i`, or nullopt for text-only (default: none). */
    std::function<std::optional<IconButton::Shape> (int)> onTabIcon;

    /** Width the pill ASKS for: one slot per tab, every slot as wide as the
     *  WIDEST label at the fixed type size, plus trailing air after the
     *  last slot. That air is not a wider slot: uniform slots are the
     *  badge's geometry, while the strip END needs clearance from the
     *  pill's rounded corner (radius ~ half the pill height) so the last
     *  row never touches the curve. Measured, never a constant: a constant here is what stayed at 520 px
     *  while the tab list went from three slots to seven. */
    int preferredWidth() const
    {
        const int n = numTabs();
        const float slotW = naturalSlotWidth();
        return slotW > 0.0f ? juce::roundToInt (slotW) * n + juce::roundToInt (kEndPad) : 0;
    }

    /** A wider pill shows more of the strip: re-clamp at once, so the eased
     *  position cannot slide back over content that is already visible.
     *  A resize that changed what fits also re-aims at the active tab -
     *  the tick only follows on active-index CHANGE, so this is where a
     *  narrower window keeps its tab in view. */
    void resized() override
    {
        scrollPos = targetScroll = juce::jlimit (0.0f, maxScroll(), scrollPos);
        retargetScroll (juce::jlimit (0, numTabs() - 1,
                                      tabbed.getCurrentTabIndex()));
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = area.getHeight() * 0.5f;

        // Rounded background track ("bgPanel" pill).
        g.setColour (bgPanel());
        g.fillRoundedRectangle (area, radius);
        g.setColour (text().withAlpha (0.18f));
        g.drawRoundedRectangle (area, radius, 1.0f);

        const int n = numTabs();
        const float slotW = naturalSlotWidth();
        if (slotW <= 0.0f)
            return;

        // FIXED type size. A slot is exactly as wide as the widest label at
        // that size, so nothing is ever scaled down to make room; when the
        // tab list is wider than this pill, the strip scrolls under it.
        g.setFont (createFontRaw (kLabelFontH, true));

        // Everything below is drawn in STRIP space (slot 0 at the pill's
        // left edge, then one slot per tab) and shifted by scrollPos, so
        // the track stays put and the TABS move under it. The clip is the
        // ROUNDED SHAPE itself, not its bounding rect: rect-clipping lets
        // scrolling labels, the badge and the square cue bands cross the
        // corner curve - the "dépasse de l'arrondi" report. A half-scrolled
        // label is cut by the pill edge, exactly like a scrolled tab bar,
        // instead of painting over the header and the page frames below.
        juce::Path shape;
        shape.addRoundedRectangle (area, radius);
        g.saveState();
        g.reduceClipRegion (shape, {});

        // Sliding accent badge. pillPos is in SLOT units [0..n-1], so a
        // resize or a tab-count change re-targets it on the very next paint.
        const int active = juce::jlimit (0, n - 1, static_cast<int> (targetPillPos + 0.5f));
        const float slotOriginX = area.getX() - scrollPos;
        auto pillRect = juce::Rectangle<float> (
            slotOriginX + kBadgeInset + pillPos * slotW,
            area.getY() + 3.0f,
            slotW - kBadgeInset * 2.0f, area.getHeight() - 6.0f);
        // Edge hug: the first/last slot's badge stretches to the track end
        // it touches, so no track-colour gap sits between the badge and the
        // curve. Only at scroll rest - mid-scroll the badge rides its slot.
        // NOTE setLeft/setRight, never setX: setX moves the edge and KEEPS
        // the width (the cue-band trap that ate the right chevron), these
        // keep the opposite edge and stretch.
        if (active == 0 && scrollPos <= 0.5f)
            pillRect.setLeft (area.getX() + kBadgeEdgeClear);
        if (active == n - 1 && scrollPos >= maxScroll() - 0.5f)
            pillRect.setRight (area.getRight() - kBadgeEdgeClear);
        g.setColour (accent());
        g.fillRoundedRectangle (pillRect, pillRect.getHeight() * 0.5f);

        for (int i = 0; i < n; ++i)
        {
            auto slot = juce::Rectangle<float> (slotOriginX + static_cast<float> (i) * slotW,
                                                area.getY(), slotW, area.getHeight());
            g.setColour (i == active ? juce::Colours::white : textDim());
            if (const auto glyph = iconFor (i))
            {
                // Icon + label as ONE centred row: the icon owns the left,
                // the text follows, and the pair is centred in the slot -
                // a left-pinned icon with a centred label would drift apart
                // on wide slots.
                const float textW = juce::GlyphArrangement::getStringWidth (
                    g.getCurrentFont(), labelFor (i));
                const float rowW = kIconSide + kIconGap + textW;
                const float rowX = slot.getX()
                                 + (slot.getWidth() - rowW) * 0.5f;
                const float iconY = slot.getCentreY() - kIconSide * 0.5f;
                g.fillPath (IconButton::glyphPath (*glyph,
                    juce::Rectangle<float> (rowX, iconY, kIconSide, kIconSide),
                    false, false, kIconSide));
                g.drawText (labelFor (i),
                            juce::Rectangle<float> (rowX + kIconSide + kIconGap,
                                                    slot.getY(),
                                                    textW + 2.0f,
                                                    slot.getHeight()),
                            juce::Justification::centredLeft);
            }
            else
            {
                g.drawText (labelFor (i), slot.reduced (kSlotPad, 0.0f),
                            juce::Justification::centred);
            }

            // File-drag cue: outline the pill that a 1 s dwell would activate.
            if (draggingOver && i == hoverSlot && hoverSlot != active)
            {
                g.setColour (accent().withAlpha (0.85f));
                g.drawRoundedRectangle (
                    slot.withSizeKeepingCentre (slot.getWidth() - 4.0f,
                                                slot.getHeight() - 4.0f),
                    slot.getHeight() * 0.5f, 1.5f);
            }
        }

        // === Overflow cue ==================================================
        // A scrolling strip that gives no sign of hiding tabs reads as a
        // broken control, so each edge that still has content beyond it
        // fades: the label dissolves INTO the track instead of being cut
        // mid-glyph by the pill. Left edge while anything is scrolled off,
        // right edge while more remains. Painted INSIDE the shape clip
        // above (not here): square cue bands must never cross the corner
        // curve either.
        drawScrollCue (g, area, true);
        drawScrollCue (g, area, false);

        g.restoreState();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // Click selects the slot under the pointer (the old toggle could only
        // ever reach tabs 0 and 1). The pointer x is in PILL space, so
        // scrollPos is added back to get the strip-space slot: without it a
        // scrolled strip would activate the tab one or two places away.
        const int n = numTabs();
        const float slotW = naturalSlotWidth();
        const int idx = slotW > 1.0f
                      ? juce::jlimit (0, n - 1, static_cast<int> ((e.position.getX() + scrollPos) / slotW))
                      : 0;
        tabbed.setCurrentTabIndex (idx);   // fires TabsBar::currentTabChanged
        targetPillPos = static_cast<float> (idx);
        retargetScroll (idx);              // bring the freshly picked tab into view
        repaint();
    }

    bool hitTest (int, int) override { return true; }

    void mouseMove (const juce::MouseEvent& e) override { hoverX = e.position.x; }
    void mouseExit (const juce::MouseEvent&) override { hoverX = -1; }

    /** Wheel scrolls the strip. THE pointer gesture for overflow: a
     *  scrolled-off tab has no visible slot to click and the dwell only
     *  sees visible slots, so without this the edge fades announce tabs
     *  no pointer can reach. The 60 Hz tick keeps minimal-scroll reveal
     *  of the ACTIVE tab, so exploring never loses it - scrolling past
     *  it pulls it back, scrolling beside it stays. */
    static constexpr float kWheelSlotFraction = 1.0f; // one slot per notch
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& wheel) override
    {
        const float maxS = maxScroll();
        if (maxS <= 0.0f)
            return;
        const float slotW = naturalSlotWidth();
        if (slotW <= 0.0f)
            return;
        const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
        const float dir = wheel.isReversed ? -1.0f : 1.0f;
        targetScroll = juce::jlimit (0.0f, maxS,
            targetScroll - dir * delta * slotW * kWheelSlotFraction);
        repaint();
    }

    // === Hover-to-activate during an OS file drag ============================
    // Dragging a take/stem and holding the pointer over a pill for 1 s
    // activates that tab, so a drop can be routed to a panel on ANOTHER tab
    // (e.g. drag a stem from Stems, hover the Text-to-Speech pill, then drop
    // on the TTS panel). The 60 Hz animation timer drives the dwell logic.
    bool isInterestedInFileDrag (const juce::StringArray&) override
    {
        return true;
    }

    void fileDragEnter (const juce::StringArray&, int x, int y) override
    {
        draggingOver = true;
        dragPoint = { x, y };
        hoverSlot = -1;
    }

    void fileDragMove (const juce::StringArray&, int x, int y) override
    {
        draggingOver = true;
        dragPoint = { x, y };
    }

    void fileDragExit (const juce::StringArray&) override
    {
        draggingOver = false;
        hoverSlot = -1;
        repaint();
    }

    void filesDropped (const juce::StringArray&, int, int) override
    {
        draggingOver = false;
        hoverSlot = -1;
        repaint();
    }

private:
    /** Slot count mirrors the real tab list, so adding a tab to the editor
     *  never requires touching this control again. */
    int numTabs() const { return juce::jmax (1, tabbed.getNumTabs()); }

    // === Label metrics =====================================================
    /** ONE size for every label, whatever the tab count or the window
     *  width. The strip is a primary navigation read at a glance, so its
     *  type does not scale: the OLD control stepped the font down from
     *  15 pt to a 10 pt floor to force every name into a pill frozen at
     *  520 px, which is how seven tabs ended up with unreadable labels
     *  that overflowed their own slot. Width is bought with scrolling
     *  instead (scrollPos), which costs the type nothing.
     *
     *  17 pt is half the pill's height, the proportion the original OVT
     *  switch used (font = 50% of the track). Both numbers are constants,
     *  so the pairing cannot drift apart. */
    static constexpr float kLabelFontH = 17.0f;
    /** Breathing room per slot, either side of its label. */
    static constexpr float kSlotPad = 7.0f;
    /** Trailing air after the LAST slot: with the 7 px slot pad alone the
     *  final row ends where the pill's rounded corner begins. */
    static constexpr float kEndPad = 10.0f;
    /** Width of the edge fade that announces hidden tabs. */
    static constexpr float kScrollCueW = 22.0f;

    /** Slot width: the WIDEST label measured at the fixed size, plus its
     *  padding. Equal slots (the pill has one badge, so slots must be
     *  uniform) sized by the worst case, hence no label can overflow the
     *  slot that owns it at any tab count. Tabs carrying an icon measure
     *  icon + gap + label: the worst case may wear an icon, and a slot
     *  sized for a bare label would cut the icon row. */
    float naturalSlotWidth() const
    {
        if (tabbed.getNumTabs() <= 0)
            return 0.0f;

        const int n = numTabs();

        const auto font = createFontRaw (kLabelFontH, true);
        float widest = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float w = juce::GlyphArrangement::getStringWidth (font,
                          labelFor (i));
            if (iconFor (i).has_value())
                w += kIconSide + kIconGap;
            widest = juce::jmax (widest, w);
        }

        return widest + kSlotPad * 2.0f;
    }

    /** How far the strip can scroll: zero when the pill is wide enough to
     *  show every slot, which is the state the 520 px cap used to make
     *  impossible. */
    float maxScroll() const
    {
        return juce::jmax (0.0f, static_cast<float> (preferredWidth()) - getWidth());
    }

    /** Scroll just far enough to bring slot `i` fully into view, and no
     *  further - the "minimal scroll" a tab bar does when the selection
     *  moves off-screen. Called from the timer on every change, so the
     *  strip follows the active tab whether the user clicked here, a
     *  dwell switched it, or a resize narrowed the pill. */
    void retargetScroll (int i)
    {
        const float slotW = naturalSlotWidth();
        const float maxS = maxScroll();
        if (slotW <= 0.0f || maxS <= 0.0f)
        {
            targetScroll = 0.0f;
            return;
        }

        const float slotLeft = static_cast<float> (i) * slotW;
        const float slotRight = slotLeft + slotW;
        const float viewW = static_cast<float> (getWidth());

        // Scroll only when the slot is actually outside the viewport, and
        // keep a small margin so a badge never touches the pill's rounded
        // edge.
        constexpr float kEdge = 2.0f;
        float want = targetScroll;
        if (slotLeft < want + kEdge)
            want = slotLeft - kEdge;
        else if (slotRight > want + viewW - kEdge)
            want = slotRight - viewW + kEdge;

        targetScroll = juce::jlimit (0.0f, maxS, want);
    }

    /** Fade the edge that still has tabs hidden beyond it, with a chevron
     *  naming the scroll direction. */
    void drawScrollCue (juce::Graphics& g, const juce::Rectangle<float>& area,
                        bool leftEdge)
    {
        const float maxS = maxScroll();
        if (maxS <= 0.0f)
            return;

        // A few hidden pixels are overscroll noise, not hidden tabs: the
        // cue earns its paint past 4 px, so easing residue and a nudge
        // never flash a chevron for nothing.
        const bool hasMore = leftEdge ? (scrollPos > 4.0f)
                                      : (scrollPos < maxS - 4.0f);
        if (! hasMore)
            return;

        const float w = juce::jmin (kScrollCueW, area.getWidth() * 0.2f);
        // NOTE: removeFromRight, NOT setX. Rectangle::setX moves the edge
        // and KEEPS the width, so the band would stretch a full pill-width
        // past the edge: its gradient would read solid across the visible
        // slice (the "carré") and the chevron, centred off-screen, would
        // never paint. That is exactly the missing right chevron.
        auto band = area;
        if (leftEdge)
            band.setWidth (w);
        else
            band = area.withX (area.getRight() - w).withWidth (w);

        // Transparent INSIDE (visible labels show through), solid at the
        // EDGE (exiting labels dissolve into the track): a label sliding
        // out travels from seethrough cover to full cover. The reverse -
        // solid inside - hides labels that are fully in view while leaving
        // the exiting ones readable to the curve, which is the inversion
        // the reader caught on screen.
        const auto c = bgPanel();
        const float inner = leftEdge ? band.getRight() : band.getX();
        const float outer = leftEdge ? band.getX() : band.getRight();
        const float cy = band.getCentreY();
        juce::ColourGradient dissolve (c.withAlpha (0.0f),
                                       juce::Point<float> (inner, cy),
                                       c,
                                       juce::Point<float> (outer, cy), false);
        g.setGradientFill (dissolve);
        g.fillRect (band);

        // Chevron: two 2 px strokes pointing toward the hidden tabs, so
        // the edge names its direction instead of merely glowing.
        const float cx = band.getCentreX();
        const float s = 4.0f;
        const float dir = leftEdge ? -1.0f : 1.0f;
        g.setColour (text().withAlpha (0.75f));
        g.drawLine (cx - dir * s, cy - s * 1.4f, cx + dir * s, cy, 2.0f);
        g.drawLine (cx + dir * s, cy, cx - dir * s, cy + s * 1.4f, 2.0f);
    }

    /** Icon side and gap, in px. The icon reads at label height without
     *  owning the slot: 14 px on a ~30 px pill, breathing like the type. */
    static constexpr float kIconSide = 14.0f;
    static constexpr float kIconGap = 6.0f;
    /** Badge inset from its slot, each side. 5 px, not 3: the widest row
     *  ends 4 px from a 3 px-inset badge, and glyph side bearings eat
     *  that - the label touches the badge's rounded end. */
    static constexpr float kBadgeInset = 5.0f;
    /** Badge clearance to the track ends when hugging them: 3 px, like
     *  top and bottom, so the hug reads as nested, not touching. */
    static constexpr float kBadgeEdgeClear = 3.0f;

    /** Resolved label / glyph for tab `i`: the providers above, falling
     *  back to the full name / no icon. Every paint and measure goes
     *  through these, so a label the measure sees is the label the paint
     *  draws. */
    juce::String labelFor (int i) const
    {
        const auto names = tabbed.getTabNames();
        if (i < 0 || i >= names.size())
            return {};
        if (onTabLabel)
            return onTabLabel (i);
        return names[i];
    }

    std::optional<IconButton::Shape> iconFor (int i) const
    {
        if (onTabIcon)
            return onTabIcon (i);
        return std::nullopt;
    }

    void timerCallback() override
    {
        // Keep the badge in sync when the tab changes from elsewhere.
        const float target = static_cast<float> (juce::jlimit (0, numTabs() - 1,
                                                              tabbed.getCurrentTabIndex()));
        if (std::abs (target - targetPillPos) > 0.001f)
        {
            targetPillPos = target;
            repaint();
        }

        // Exponential ease-out, ~150-200 ms settle at 60 Hz (OVT constant).
        constexpr float kSmoothing = 0.18f;
        if (std::abs (pillPos - targetPillPos) > 0.001f)
        {
            pillPos += (targetPillPos - pillPos) * kSmoothing;
            repaint();
        }

        // The strip follows the active tab ON CHANGE, not every tick: a
        // continuous re-aim would fight manual scrolls (wheel, hover
        // edges) by pulling the active tab back the moment it leaves the
        // view. Selecting a tab still reveals it; exploring around it is
        // free, and resized() covers the window that changed what fits.
        const int active = juce::jlimit (0, numTabs() - 1,
                                         tabbed.getCurrentTabIndex());
        if (active != lastActiveIndex)
        {
            lastActiveIndex = active;
            retargetScroll (active);
        }
        if (std::abs (scrollPos - targetScroll) > 0.001f)
        {
            scrollPos += (targetScroll - scrollPos) * kSmoothing;
            repaint();
        }

        // Hover-edge auto-scroll: lingering within a slot of an edge that
        // hides tabs eases the strip that way - the fades and chevrons
        // promise scrollability, the edges perform it. Runs during file
        // drags too (dragPoint), so a needed tab is revealed and then
        // dwell-switched; mouse position comes from the drag there
        // because an OS drag delivers fileDragMove, not mouseMove.
        {
            const int px = draggingOver ? dragPoint.x : hoverX;
            const float maxS = maxScroll();
            if (maxS > 0.0f && px >= 0)
            {
                if (px < kHoverEdgePx && scrollPos > 0.0f)
                    targetScroll = juce::jmax (0.0f,
                        targetScroll - kHoverStepPx);
                else if (px > (float) getWidth() - kHoverEdgePx
                         && scrollPos < maxS)
                    targetScroll = juce::jmin (maxS,
                        targetScroll + kHoverStepPx);
            }
        }

        // Hover-to-activate: 1 s dwell on a pill during a file drag switches
        // to that tab (paint draws the hovered slot as the drop cue).
        if (draggingOver)
        {
            // Same strip-space mapping as the click: the pointer is in pill
            // space, so the scroll offset has to be added back or the dwell
            // highlights the wrong tab once the strip has scrolled.
            const float slotW = naturalSlotWidth();
            const int slot = slotW > 1.0f
                           ? juce::jlimit (0, numTabs() - 1,
                                           static_cast<int> ((static_cast<float> (dragPoint.x)
                                                              + scrollPos) / slotW))
                           : -1;
            const bool inside = getLocalBounds().contains (dragPoint);
            if (! inside || slot != hoverSlot)
            {
                hoverSlot = inside ? slot : -1;
                dwellStartMs = juce::Time::getMillisecondCounter();
                repaint();
            }
           else if (hoverSlot >= 0
                     && hoverSlot != tabbed.getCurrentTabIndex()
                     && juce::Time::getMillisecondCounter() - dwellStartMs >= 1000)
            {
                tabbed.setCurrentTabIndex (hoverSlot); // drop target now live
                dwellStartMs = juce::Time::getMillisecondCounter();
                repaint();
            }
        }
    }

    /** Hover-edge auto-scroll geometry: how close to an edge (px) starts
     *  it, and how far (px) one 60 Hz tick moves. ~300 px/s: fast enough
     *  to cross seven slots in a few seconds, slow enough to stop on one. */
    static constexpr float kHoverEdgePx = 30.0f;
    static constexpr float kHoverStepPx = 5.0f;

    juce::TabbedComponent& tabbed;
    float pillPos = 0.0f;        // current animated slot position [0..n-1]
    float targetPillPos = 0.0f;  // target slot position
    float scrollPos = 0.0f;      // horizontal scroll of the strip, px [0..maxScroll]
    float targetScroll = 0.0f;   // target scroll (minimal scroll to reveal the active slot)
    bool draggingOver = false;   // a file drag is hovering this pill switch
    juce::Point<int> dragPoint;
    int hoverSlot = -1;          // pill under the dragged file (-1 = none)
    juce::uint32 dwellStartMs = 0; // when the current pill hover began
    int hoverX = -1;             // pointer x for hover-scroll (-1 = outside)
    int lastActiveIndex = -1;    // active tab the scroll last followed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PillTabSwitch)
};

} // namespace ebs
