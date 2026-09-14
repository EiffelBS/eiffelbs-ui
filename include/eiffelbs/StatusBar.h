// StatusBar.h
// eiffelbs-ui - global bottom status bar + floating log window.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from OpenTimbre source/ui/StatusBar.h (original copyright
// (C) 2026 EiffelBS).
//
// GLOBAL bottom status bar (~24 px, full width, OUTSIDE every frame):
// shows the LAST log line (left-truncated, dim) plus a discreet line
// counter. Clicking it opens a small NON-BLOCKING resizable floating
// "Log" window with the full history (read-only TextEditor), using the
// single-live-instance doctrine: self-delete on close paths, no modal
// state.
//
// Thread safety: logLine() can be called from ANY thread - it hops to the
// message thread through MessageManager::callAsync before touching state.
//
// ACTIVITY SLOTS (v0.5.0 concept, consumer request 2026-08-27): textual
// progress ("Generating... 12 s elapsed") relied on a fragile replace-
// by-prefix heuristic. Structured signals replace it: beginActivity()
// claims a slot rendered as [spinner + label + elapsed s][determinate
// ratio bar when the task can measure itself], and setQueue() renders the
// processing-queue position as a small "done/total" bar. Log lines then
// carry EVENTS only; progress lives in the slots.
//
// MULTIPLE CONCURRENT CLAIMS (v0.7.0, consumer report 2026-08-30): a long
// task (TTS generation) used to lose its indication the moment another
// task claimed the single slot - the new claim SUPERSEDED the old one and
// its finish() cleared the bar while the first task was still running.
// Claims are now independent: each beginActivity() adds a segment
// ([Ns][ratio bar?]), the spinner ensemble stays up while ANY claim is
// live, and a finish()/destruction removes ONLY its own segment. There is
// deliberately NO supersede anymore: ownership is plain RAII per token,
// same doctrine as the queue slot.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <deque>
#include <map>
#include <vector>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/LookAndFeel.h"   // resolved(): theme-level colour hook

namespace ebs
{

class StatusBar;

/** Floating full-history window (single instance, self-deleting). */
class LogWindow : public juce::DialogWindow
{
public:
    LogWindow() : juce::DialogWindow ("Log", bgDark(),
                                      /*escapeCloses*/ true)
    {
        // OS-native title bar, consistent with the rest of the app
        // (SettingsWindow/ModelsManagementWindow and the editor hook set
        // setUsingNativeTitleBar(true)); the JUCE-drawn bar was the odd
        // one out (macOS port wave, 2026-09-14).
        setUsingNativeTitleBar (true);
        // NB: DialogWindow has NO resizable ctor arg (that 4th bool is
        // addToDesktop) - resizability is enabled in showLogWindow().
    }

    ~LogWindow() override
    {
        if (liveWindow == this)
            liveWindow = nullptr;
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<LogWindow> (this)] () mutable
            {
                if (safe != nullptr)
                    safe.deleteAndZero();
            });
    }

    static LogWindow* liveWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LogWindow)
};

inline LogWindow* LogWindow::liveWindow = nullptr;

class StatusBar : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    /** Opaque RAII claim on ONE activity segment. Move-only; destruction
        releases. Any thread (hops to the message thread). Claims are
        INDEPENDENT (v0.7.0): a newer beginActivity() never supersedes an
        older one - each finish() removes only its own segment, so a late
        finish() can never clear another task's slot and two concurrent
        tasks both stay visible. */
    class ProgressActivity
    {
    public:
        ProgressActivity() = default;
        ProgressActivity (ProgressActivity&& o) noexcept
            : bar (std::move (o.bar)), token (o.token) { o.token = 0; }
        ProgressActivity& operator= (ProgressActivity&& o) noexcept
        {
            if (this != &o)
            {
                release();
                bar   = std::move (o.bar);
                token = o.token;
                o.token = 0;
            }
            return *this;
        }
        ~ProgressActivity() { release(); }

        /** Determinate ratio 0..1; a negative value returns the segment to
            indeterminate. Call only when the task can MEASURE itself -
            the segment stays hidden while nobody feeds it. No textual
            label exists in the segment (consumer feedback 2026-08-27:
            the log line beside it already tells WHAT is running). */
        void progress (float ratio01);

        /** Release the slots now (the destructor calls this too). */
        void finish();

        explicit operator bool() const noexcept { return token != 0; }

    private:
        friend class StatusBar;
        ProgressActivity (StatusBar* b, juce::uint32 t) : bar (b), token (t) {}
        void release();
        juce::Component::SafePointer<StatusBar> bar;
        juce::uint32 token = 0;
        JUCE_DECLARE_NON_COPYABLE (ProgressActivity)
    };

    /** Claim an activity segment (ANY thread): spinner + per-claim elapsed
        seconds, plus a determinate bar as soon as progress() feeds a
        ratio >= 0. Concurrent claims stack as independent segments. */
    ProgressActivity beginActivity();

    /** Queue slot (ANY thread): "done/total" mini-bar; total <= 0 hides.
        The CALLER owns the lifecycle: JobQueue posts the 1-based index of
        the task that is running and posts (0, 0) the moment the burst
        drains, which hides the slot immediately.
        burstStart: when valid, the slot also renders the TOTAL elapsed
        time since the queue processing began (second counter, user
        request 2026-08-28) - the per-generation elapsed lives in the
        activity slot. */
    void setQueue (int done, int total, juce::Time burstStart = juce::Time());

    // ------------------------------------------------------------------
    // RESOURCE METERS (idea #7, right-aligned zone): the CONSUMER owns the
    // platform probes and pushes readings; the lib renders label + status
    // fill bar and collapses meters on narrow panels. User decisions
    // (2026-08-28): right side of the bar, VRAM visible by default, a
    // RIGHT-CLICK on any meter opens the add/remove menu (consumer-built,
    // like onQueueClicked - only the consumer knows what its platform can
    // actually probe).
    enum class MeterId { vram, sharedGpu, gpu, cpu, ram };   // priority order

    struct MeterReading
    {
        float ratio01 = -1.0f;   // 0..1 fills the bar; < 0 = no fill
        juce::String valueText;  // e.g. "3.4/8.0 GB" or "12 %"
    };

    /** Show/hide one meter (ANY thread). Default set: VRAM only. The
        consumer persists the choice (ui-state.json) - the lib does not. */
    void setMeterVisible (MeterId id, bool visible)
    {
        hop ([id, visible] (StatusBar& s)
        {
            s.meterVisible[(size_t) id] = visible;
            s.repaint();
        });
    }

    /** Push a sample (ANY thread, ~1 Hz from the consumer's probes).
        Only VISIBLE meters need sampling. Repaints immediately - the
        meters are the only thing ticking while the bar is idle. */
    void updateMeter (MeterId id, const MeterReading& r)
    {
        hop ([id, r] (StatusBar& s)
        {
            s.meterReadings[(int) id] = r;
            s.repaint();
        });
    }

    /** Compact view (user request 2026-08-28): label + bar only, no
        value text. The consumer persists the choice. */
    void setMetersCompact (bool compact)
    {
        hop ([compact] (StatusBar& s)
        {
            s.metersCompact = compact;
            s.repaint();
        });
    }

    /** Right-click on the meter zone (screen position). The consumer
        builds the add/remove menu; unset -> the right-click does
        nothing (left-click anywhere still opens the log). */
    std::function<void (juce::Point<int> screenPos)> onMetersMenu;

    /** Invoked when the user clicks the visible queue slot (passing the
        click's SCREEN position). Consumers may show a menu there (e.g.
        remove a pending job); when unset, every click opens the log. */
    std::function<void (juce::Point<int> screenPos)> onQueueClicked;
    /** JUCE-standard colour hooks. Resolution order: Component::setColour()
     *  override > ebs::LookAndFeel::widgetThemeColour() hook > built-in
     *  palette default. See IconButton::ColourIds for the full contract. */
    enum ColourIds
    {
        backgroundColourId = 0x1e4b0111,  ///< bar surface fill
        dividerColourId    = 0x1e4b0112,  ///< 1 px top separator line
        textColourId       = 0x1e4b0113,  ///< last-log-line + slot text
        counterColourId    = 0x1e4b0114   ///< reserved (counter removed)
    };

    StatusBar()
    {
        setOpaque (true);
        setInterceptsMouseClicks (true, false);
        setTooltip ("Click to open the full log");
    }

    /** ANY thread: hop to the message thread, then record + repaint. */
    void logLine (const juce::String& line)
    {
        if (! juce::MessageManager::existsAndIsCurrentThread())
        {
            juce::MessageManager::callAsync (
                [safe = juce::Component::SafePointer<StatusBar> (this), line]
                {
                    if (safe != nullptr)
                        safe->logLine (line);
                });
            return;
        }

        // TRANSIENT progress lines: when the new line shares the prefix
        // before its first digit with a RECENT entry ("Generating... N s
        // elapsed"), it REPLACES that entry instead of stacking - one
        // evolving line even when stage/notice lines interleave between
        // heartbeats (bounded backward scan).
        //
        // PATH GUARD: a prefix containing a path separator is a file-path
        // fragment, NEVER a transient progress signature. Sibling files of
        // one run ("...\run-5132a8ae\fc9c3307-vocals.wav" vs
        // "...\bass.wav") share everything before their first digit (the
        // run directory name) and would otherwise collapse into ONE
        // history line (consumer report 2026-08-27: "Done: 4 stems"
        // listed a single path - the four sibling paths replaced each
        // other in place).
        const auto pfx = transientPrefixOf (line);
        bool replaced = false;
        if (pfx.isNotEmpty() && ! pfx.contains ("/") && ! pfx.contains ("\\")
            && ! history.empty())
        {
            int scanned = 0;
            for (auto it = history.rbegin();
                 it != history.rend() && scanned < 12; ++it, ++scanned)
            {
                if (transientPrefixOf (*it) == pfx)
                {
                    *it = line;
                    replaced = true;
                    break;
                }
            }
        }
        if (! replaced)
        {
            history.push_back (line);
            while (history.size() > kMaxLines)
                history.pop_front();
            totalLogged++;
        }
        lastLine = line;
        repaint();
    }

    /** Substring before the FIRST digit - the transient-progress
        signature of a line ("Generating... " for the heartbeat). Empty
        when the line starts with a digit or holds none at all. */
    static juce::String transientPrefixOf (const juce::String& s)
    {
        for (int i = 0; i < s.length(); ++i)
            if (s[i] >= '0' && s[i] <= '9')
                return s.substring (0, i);
        return {};
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (resolved (backgroundColourId, bgPanel()));
        g.setColour (resolved (dividerColourId,
                               panelBorder().withAlpha (0.6f)));
        g.drawLine (0.0f, 0.5f, (float) getWidth(), 0.5f, 1.0f);

        g.setFont (createFont (13.0f, false));
        const float cy = getHeight() * 0.5f;
        auto x = 8.0f;

        // ACTIVITY block: spinner + per-generation elapsed, and - when a
        // queue burst is live - a small separator + the TOTAL burst
        // elapsed (user request 2026-08-28: both timers side by side).
        // The block is ONE ensemble (user feedback 2026-08-28): the
        // spinner keeps spinning in the gap between two tasks of a live
        // burst (a task just ended, the next is about to start) and the
        // whole block vanishes together when the burst drains. The
        // per-task seconds only show while a task is actually claimed.
        const bool burstLive = queueTotal > 0 && queueBurstStart != juce::Time();
        const bool taskLive  = ! claims.empty();
        if (taskLive || burstLive)
        {
            const float r = 5.0f;
            juce::Path arc;
            arc.addCentredArc (x + r, cy, r, r, 0.0f,
                               spinnerPhase, spinnerPhase + 3.8f, true);
            g.setColour (resolved (textColourId, textDim()));
            g.strokePath (arc, juce::PathStrokeType (1.6f));
            x += 2.0f * r + 6.0f;

            // ONE SEGMENT PER LIVE CLAIM (v0.7.0): [Ns][ratio bar?], with
            // a thin divider between segments. The spinner ensemble keeps
            // ticking while ANY claim is live; each segment vanishes when
            // its own task finishes.
            for (size_t ci = 0; ci < claims.size(); ++ci)
            {
                const auto& claim = claims[ci];
                if (ci > 0)
                {
                    g.setColour (resolved (textColourId, textDim())
                                     .withAlpha (0.35f));
                    g.fillRect (x, cy - 6.0f, 1.5f, 12.0f);
                    x += 9.0f;
                }
                const auto secs = (int) ((juce::Time::getCurrentTime()
                                          - claim.start).inSeconds());
                g.drawText (juce::String (secs) + " s", (int) x, 0, 40,
                            getHeight(), juce::Justification::centredLeft, true);
                x += 44.0f;
                if (claim.ratio >= 0.0f)
                {
                    drawMiniBar (g, x, cy, 45.0f,
                                 juce::jlimit (0.0f, 1.0f, claim.ratio));
                    x += 53.0f;
                }
            }

            if (burstLive)
            {
                // Small separator between the task segments and the
                // burst-total timer.
                g.setColour (resolved (textColourId, textDim())
                                 .withAlpha (0.35f));
                g.fillRect (x, cy - 6.0f, 1.5f, 12.0f);
                x += 9.0f;
                const auto total = (int) (juce::Time::getCurrentTime()
                                          - queueBurstStart).inSeconds();
                g.setColour (resolved (textColourId, textDim()));
                g.drawText (formatElapsed (total), (int) x, 0, 56,
                            getHeight(), juce::Justification::centredLeft, true);
                x += 60.0f;
            }
        }

        // QUEUE slot: "done/total" mini-bar; visible only while the
        // caller feeds it (the queue posts (0, 0) the moment the burst
        // drains, which hides the slot immediately). The TOTAL burst
        // elapsed lives in the activity block beside the spinner.
        queueSlotBounds = {};
        if (queueTotal > 0)
        {
            const float slotX = x;
            drawMiniBar (g, x, cy, 40.0f,
                         juce::jlimit (0.0f, 1.0f,
                                       (float) queueDone / (float) queueTotal));
            x += 44.0f;
            g.setColour (resolved (textColourId, textDim()));
            g.drawText (juce::String (queueDone) + "/" + juce::String (queueTotal),
                        (int) x, 0, 38, getHeight(),
                        juce::Justification::centredLeft, true);
            x += 42.0f;
            queueSlotBounds = { slotX, 0.0f, x - slotX, (float) getHeight() };
        }

        // RESOURCE METERS zone (right-aligned, before the right edge):
        // [label][bar][value] per meter, priority-collapsed on narrow
        // panels, separated by a thin divider. Widths adapt to CONTENT
        // (user feedback 2026-08-28: GPU/CPU had lots of dead space);
        // compact view drops the value text entirely. Drawn BEFORE the
        // log line so the text shrinks.
        meterZoneBounds = {};
        const float meterSep = 12.0f;           // divider line + margins
        int maxMeters = getWidth() >= 700 ? 5 : getWidth() >= 480 ? 2 : 1;
        // Percentages flip between 1 and 3 digits ("5 %" / "100 %") -
        // reserve the wide form so the cell does not jitter every tick
        // (user feedback 2026-08-28).
        auto valueWidth = [this, &g] (const juce::String& value)
        {
            float w = juce::GlyphArrangement::getStringWidth (
                g.getCurrentFont(), value);
            if (value.endsWith ("%"))
                w = std::max (w, juce::GlyphArrangement::getStringWidth (
                                     g.getCurrentFont(), "100 %"));
            return std::max (w, 20.0f);
        };
        const auto grid4 = [] (float w)
        { return ((int) std::ceil (w / 4.0f)) * 4.0f; };
        // Pass 1 - which meters, and how wide is each (4 px grid keeps
        // the zone from jittering as the numbers tick).
        std::vector<int> shownIds;
        float meterZoneW = 10.0f;               // leading gap
        for (int mi = 0; mi < 5 && (int) shownIds.size() < maxMeters; ++mi)
        {
            if (! meterVisible[(size_t) mi]) continue;
            const auto label = mi == 0 ? "VRAM"
                             : mi == 1 ? "GPU-S"
                             : mi == 2 ? "GPU"
                             : mi == 3 ? "CPU" : "RAM";
            float labelW = juce::GlyphArrangement::getStringWidth (
                g.getCurrentFont(), label);
            float valueW = 0.0f;
            if (! metersCompact)
            {
                const auto reading = meterReadings.find (mi);
                const auto value = reading != meterReadings.end()
                                       ? reading->second.valueText
                                       : juce::String();
                valueW = valueWidth (value);
            }
            // Exact 5 px gaps on BOTH sides of the bar (the label box
            // carries the grid slack so rounding cannot eat the gap -
            // "GPU-S" used to end up glued).
            const float cell = grid4 (labelW) + 4.0f + 5.0f + 34.0f + 5.0f
                               + (metersCompact ? 0.0f : grid4 (valueW) + 4.0f);
            shownIds.push_back (mi);
            meterZoneW += cell + meterSep;
        }
        if (! shownIds.empty())
            meterZoneW -= meterSep;             // no trailing divider
        if (meterZoneW > 10.0f)
        {
            const float mx = (float) getWidth() - 8.0f - meterZoneW + 10.0f;
            meterZoneBounds = { mx, 0.0f,
                                (float) getWidth() - 8.0f - mx,
                                (float) getHeight() };
            float cx = mx;
            for (size_t idx = 0; idx < shownIds.size(); ++idx)
            {
                const auto mi = shownIds[idx];
                if (idx > 0)
                {
                    // Divider between two meter blocks.
                    g.setColour (resolved (textColourId, textDim())
                                     .withAlpha (0.35f));
                    g.fillRect (cx + 5.0f, cy - 6.0f, 1.5f, 12.0f);
                    cx += meterSep;
                }
                const auto reading = meterReadings.find (mi);
                const auto label = mi == 0 ? "VRAM"
                                 : mi == 1 ? "GPU-S"
                                 : mi == 2 ? "GPU"
                                 : mi == 3 ? "CPU" : "RAM";
                // Re-measure for the draw pass (pass 1 sized the zone).
                const float labelW = juce::GlyphArrangement::getStringWidth (
                    g.getCurrentFont(), label);
                // Label RIGHT-ALIGNED inside its box: the tail sits at
                // the box's right edge, exactly 5 px before the bar.
                g.setColour (resolved (textColourId, textDim()));
                g.drawText (label, (int) cx, 0,
                            (int) grid4 (labelW) + 4, getHeight(),
                            juce::Justification::centredRight, true);
                cx += grid4 (labelW) + 4.0f + 5.0f;
                // Status fill: green -> amber (>75 %) -> red (>90 %).
                const float ratio = reading != meterReadings.end()
                                        ? reading->second.ratio01 : -1.0f;
                juce::Colour fill { 0xff4d9e5f };      // green
                if (ratio > 0.90f)      fill = juce::Colour (0xffb5433c);
                else if (ratio > 0.75f) fill = juce::Colour (0xffd19b3c);
                const juce::Rectangle<float> br { cx, cy - 4.0f, 34.0f, 8.0f };
                g.setColour (resolved (textColourId, textDim()));
                g.drawRoundedRectangle (br, 4.0f, 1.0f);
                if (ratio >= 0.0f)
                {
                    g.setColour (fill.withAlpha (0.85f));
                    auto fr = br;
                    g.fillRoundedRectangle (fr.removeFromLeft (juce::jmax (
                        2.0f, 34.0f * juce::jlimit (0.0f, 1.0f, ratio))), 4.0f);
                }
                cx += 34.0f;
                if (! metersCompact)
                {
                    cx += 5.0f;
                    const auto value = reading != meterReadings.end()
                                           ? reading->second.valueText
                                           : juce::String();
                    const float valueW = valueWidth (value);
                    // Value LEFT-ALIGNED at its box's left edge: exactly
                    // 5 px after the bar.
                    g.setColour (resolved (textColourId, textDim()));
                    g.drawText (value, (int) cx, 0, (int) grid4 (valueW) + 4,
                                getHeight(), juce::Justification::centredLeft,
                                true);
                    cx += grid4 (valueW) + 4.0f;
                }
            }
        }

        // Last line, LEFT-TRUNCATED (the tail is what matters), in the
        // space the slots and the meters left over.
        g.setColour (resolved (textColourId, textDim()));
        const auto avail = (float) getWidth() - x - meterZoneW - 8.0f;
        auto text   = lastLine;
        const float w = juce::GlyphArrangement::getStringWidth (
                            g.getCurrentFont(), text);
        if (w > avail && text.length() > 4)
        {
            while (text.length() > 4
                   && juce::GlyphArrangement::getStringWidth (
                          g.getCurrentFont(), "... " + text) > avail)
                text = text.substring (1);
            text = "... " + text;
        }
        g.drawText (text, (int) x, 0, (int) avail, getHeight(),
                    juce::Justification::centredLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // The meter zone owns ANY click (user feedback 2026-08-28: the
        // add/remove menu opens on a PLAIN click too - a left click on
        // a meter must never fall through to the log window).
        if (onMetersMenu != nullptr && meterZoneBounds.contains (e.position))
        {
            onMetersMenu (e.getScreenPosition());
            return;
        }
        if (onQueueClicked != nullptr && queueTotal > 0
            && queueSlotBounds.contains (e.position))
            onQueueClicked (e.getScreenPosition());
        else
            showLogWindow();
    }

private:
    /** Shared mini progress-bar renderer for the activity/queue slots.
        Border + outline use the TEXT colour (user feedback 2026-08-28:
        the dim divider made the slot hard to read). */
    void drawMiniBar (juce::Graphics& g, float x, float cy, float width,
                      float ratio)
    {
        const juce::Rectangle<float> r { x, cy - 4.0f, width, 8.0f };
        g.setColour (resolved (textColourId, textDim()));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (accent().withAlpha (0.85f));
        auto fill = r;                    // removeFromLeft mutates: work on a copy
        g.fillRoundedRectangle (fill.removeFromLeft (juce::jmax (2.0f,
                                    width * ratio)), 4.0f);
    }

    /** M:SS under an hour, H:MM:SS beyond (queue/burst elapsed). */
    static juce::String formatElapsed (int secs)
    {
        if (secs < 0) secs = 0;
        const auto two = [] (int v)
            { return juce::String (v).paddedLeft ('0', 2); };
        if (secs < 3600)
            return juce::String (secs / 60) + ":" + two (secs % 60);
        return juce::String (secs / 3600) + ":" + two ((secs / 60) % 60)
               + ":" + two (secs % 60);
    }

    void timerCallback() override
    {
        spinnerPhase += 0.4f;
        repaint();
    }

    /** Message-thread hop shared by every mutating entry point. */
    void hop (std::function<void (StatusBar&)>&& fn)
    {
        if (juce::MessageManager::existsAndIsCurrentThread())
            fn (*this);
        else
            juce::MessageManager::callAsync (
                [safe = juce::Component::SafePointer<StatusBar> (this),
                 f = std::move (fn)] () mutable
                { if (safe != nullptr) f (*safe); });
    }
    /** Colour resolution order for every widget ColourId:
        1. per-instance setColour() override (standard JUCE),
        2. theme-level hook ebs::LookAndFeel::widgetThemeColour(),
        3. built-in default (palette constant). */
    juce::Colour resolved (int id, juce::Colour builtin) const
    {
        if (isColourSpecified (id))
            return findColour (id);
        if (auto* l = dynamic_cast<LookAndFeel*> (&getLookAndFeel()))
            if (juce::Colour themed = l->widgetThemeColour (id);
                ! themed.isTransparent())
                return themed;
        return builtin;
    }

    void showLogWindow()
    {
        if (LogWindow::liveWindow != nullptr)
        {
            refreshLogText();
            LogWindow::liveWindow->toFront (true);
            return;
        }

        auto* box = new juce::TextEditor ("log");
        box->setMultiLine (true, true);
        box->setReadOnly (true);
        box->setCaretVisible (false);
        box->setWantsKeyboardFocus (true);
        box->setFont (createMonospaceFont (13.0f));
        box->setColour (juce::TextEditor::backgroundColourId, bgDark());
        box->setColour (juce::TextEditor::outlineColourId, accent());
        fillLogText (*box);

        auto* window = new LogWindow();
        window->setContentOwned (box, true);
        // DialogWindow has NO resizable ctor arg (that 4th bool is
        // addToDesktop): resizability must be turned on EXPLICITLY here.
        window->setResizable (true, true);            // + corner grip handle
        window->setResizeLimits (420, 300, 1600, 1200);
        window->centreAroundComponent (getParentComponent(), 560, 420);
        LogWindow::liveWindow = window;
        window->setVisible (true);
    }

    void refreshLogText()
    {
        if (auto* box = dynamic_cast<juce::TextEditor*> (
                LogWindow::liveWindow->getContentComponent()))
            fillLogText (*box);
    }

    void fillLogText (juce::TextEditor& box)
    {
        juce::String all;
        for (const auto& l : history)
            all << l << "\r\n";
        box.setText (all, false);
        box.moveCaretToEnd();
    }

    static constexpr int kMaxLines = 500;
    std::deque<juce::String> history;
    juce::String lastLine;
    int totalLogged = 0;

    // Activity segments + queue slot (message thread only; entries hop).
    std::atomic<juce::uint32> nextToken { 0 };
    struct ActivityClaim
    {
        juce::uint32 token = 0;
        juce::Time   start;
        float        ratio = -1.0f;       // < 0 = indeterminate
    };
    std::vector<ActivityClaim> claims;     // independent, RAII per token
    float        spinnerPhase  = 0.0f;
    int          queueDone = 0, queueTotal = 0;
    juce::Time   queueBurstStart;                 // valid -> total elapsed shown
    juce::Rectangle<float> queueSlotBounds;   // click target (set in paint)

    // Resource meters (message thread only; entries hop). Default set =
    // VRAM only (user decision 2026-08-28); the consumer persists the
    // visible set. Priority/collapse order follows the MeterId order.
    std::array<bool, 5> meterVisible { true, false, false, false, false };
    std::map<int, MeterReading> meterReadings;
    bool metersCompact = false;              // label + bar, no value text
    juce::Rectangle<float> meterZoneBounds;  // click target (paint)
};

inline StatusBar::ProgressActivity StatusBar::beginActivity()
{
    const auto t = nextToken.fetch_add (1, std::memory_order_relaxed) + 1;
    hop ([t] (StatusBar& s)
    {
        s.claims.push_back ({ t, juce::Time::getCurrentTime(), -1.0f });
        s.startTimerHz (10);             // spinner + elapsed repaint
        s.repaint();
    });
    return ProgressActivity (this, t);
}

inline void StatusBar::setQueue (int done, int total, juce::Time burstStart)
{
    hop ([done, total, burstStart] (StatusBar& s)
    {
        s.queueDone  = done;
        s.queueTotal = total;
        s.queueBurstStart = burstStart;
        s.repaint();
        // Hiding is the CALLER's decision (the queue posts (0, 0) on
        // drain) - no auto-hide here: a determinate-looking "N/N" may
        // legitimately mean "last task still running".
        // A visible slot with a burst counter needs the 10 Hz repaint
        // even without an activity claim (gap between two jobs); a
        // hidden slot lets the activity segments decide (the timer stops
        // when nothing needs it - setQueue must too, else the drain
        // arriving after the last finish() would leave it running).
        if (total > 0 && burstStart != juce::Time())
            s.startTimerHz (10);
        else if (total <= 0 && s.claims.empty())
            s.stopTimer();
    });
}

inline void StatusBar::ProgressActivity::progress (float ratio01)
{
    if (bar == nullptr || token == 0) return;
    const auto t = token;
    bar->hop ([t, ratio01] (StatusBar& s)
    {
        for (auto& c : s.claims)
            if (c.token == t) { c.ratio = ratio01; s.repaint(); }
    });
}

inline void StatusBar::ProgressActivity::finish()
{
    if (bar == nullptr || token == 0) return;
    const auto t = token;
    token = 0;
    bar->hop ([t] (StatusBar& s)
    {
        // Remove ONLY this claim's segment (v0.7.0: independent claims).
        for (size_t i = 0; i < s.claims.size(); ++i)
            if (s.claims[i].token == t)
            {
                s.claims.erase (s.claims.begin() + (long) i);
                break;
            }
        // The 10 Hz repaint serves BOTH slots: keep it while the queue
        // slot is visible (its burst elapsed must tick in the gap between
        // two generations) or while other claims are still live.
        if (s.claims.empty() && s.queueTotal <= 0)
            s.stopTimer();
        s.repaint();
    });
}

inline void StatusBar::ProgressActivity::release()
{
    finish();   // token guard makes double release harmless
}

} // namespace ebs
