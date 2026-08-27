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

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <deque>

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
        setUsingNativeTitleBar (false);
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
    /** Opaque RAII claim on the ACTIVITY slots. Move-only; destruction
        releases. Any thread (hops to the message thread). A newer
        beginActivity() SUPERSEDES the previous claim: stale handles
        become silent no-ops, so a late finish() can never clear another
        task's slots. */
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

        /** Determinate ratio 0..1; a negative value returns the slot to
            indeterminate. Call only when the task can MEASURE itself -
            the slot stays hidden while nobody feeds it. No textual label
            exists in the slot (consumer feedback 2026-08-27: the log
            line beside it already tells WHAT is running). */
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

    /** Claim the activity slots (ANY thread): spinner + elapsed seconds,
        plus a determinate bar as soon as progress() feeds a ratio >= 0.
        Supersedes the previous claim. */
    ProgressActivity beginActivity();

    /** Queue slot (ANY thread): "done/total" mini-bar; total <= 0 hides.
        The CALLER owns the lifecycle: JobQueue posts the 1-based index of
        the task that is running and posts (0, 0) the moment the burst
        drains, which hides the slot immediately. */
    void setQueue (int done, int total);
    /** JUCE-standard colour hooks. Resolution order: Component::setColour()
     *  override > ebs::LookAndFeel::widgetThemeColour() hook > built-in
     *  palette default. See IconButton::ColourIds for the full contract. */
    enum ColourIds
    {
        backgroundColourId = 0x1e4b0111,  ///< bar surface fill
        dividerColourId    = 0x1e4b0112,  ///< 1 px top separator line
        textColourId       = 0x1e4b0113,  ///< last-log-line text
        counterColourId    = 0x1e4b0114   ///< right-hand "N lines" text
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

        // ACTIVITY slot: spinner + elapsed (+ determinate bar). No textual
        // label: the log line beside the slots already tells WHAT runs.
        if (activityToken != 0)
        {
            const float r = 5.0f;
            juce::Path arc;
            arc.addCentredArc (x + r, cy, r, r, 0.0f,
                               spinnerPhase, spinnerPhase + 3.8f, true);
            g.setColour (resolved (textColourId, textDim()));
            g.strokePath (arc, juce::PathStrokeType (1.6f));
            x += 2.0f * r + 6.0f;

            g.setColour (resolved (textColourId, textDim()));
            const auto secs = (int) ((juce::Time::getCurrentTime()
                                      - activityStart).inSeconds());
            g.drawText (juce::String (secs) + " s", (int) x, 0, 40,
                        getHeight(), juce::Justification::centredLeft, true);
            x += 44.0f;

            if (activityRatio >= 0.0f)
            {
                drawMiniBar (g, x, cy, 90.0f,
                             juce::jlimit (0.0f, 1.0f, activityRatio));
                x += 98.0f;
            }
        }

        // QUEUE slot: "done/total" mini-bar (stays at N/N after a burst).
        if (queueTotal > 0)
        {
            drawMiniBar (g, x, cy, 80.0f,
                         juce::jlimit (0.0f, 1.0f,
                                       (float) queueDone / (float) queueTotal));
            x += 84.0f;
            g.setColour (resolved (counterColourId, panelBorder()));
            g.drawText (juce::String (queueDone) + "/" + juce::String (queueTotal),
                        (int) x, 0, 38, getHeight(),
                        juce::Justification::centredLeft, true);
            x += 42.0f;
        }

        // Last line, LEFT-TRUNCATED (the tail is what matters), in the
        // space the slots left over.
        g.setColour (resolved (textColourId, textDim()));
        const auto avail = (float) getWidth() - x - counterW - 8.0f;
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

        // Discreet counter on the right.
        g.setColour (resolved (counterColourId, panelBorder()));
        g.drawText (juce::String (history.size()) + " lines",
                    getWidth() - counterW, 0, counterW, getHeight(),
                    juce::Justification::centredRight, true);
    }

    void mouseDown (const juce::MouseEvent&) override { showLogWindow(); }

private:
    /** Shared mini progress-bar renderer for the activity/queue slots. */
    void drawMiniBar (juce::Graphics& g, float x, float cy, float width,
                      float ratio)
    {
        const juce::Rectangle<float> r { x, cy - 4.0f, width, 8.0f };
        g.setColour (resolved (dividerColourId, panelBorder()));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (accent().withAlpha (0.85f));
        auto fill = r;                    // removeFromLeft mutates: work on a copy
        g.fillRoundedRectangle (fill.removeFromLeft (juce::jmax (2.0f,
                                    width * ratio)), 4.0f);
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
    static constexpr int counterW  = 70;
    std::deque<juce::String> history;
    juce::String lastLine;
    int totalLogged = 0;

    // Activity + queue slots (message thread only; entries hop).
    std::atomic<juce::uint32> nextToken { 0 };
    juce::uint32 activityToken = 0;       // 0 = no active claim
    juce::Time   activityStart;
    float        activityRatio = -1.0f;   // < 0 = indeterminate
    float        spinnerPhase  = 0.0f;
    int          queueDone = 0, queueTotal = 0;
};

inline StatusBar::ProgressActivity StatusBar::beginActivity()
{
    const auto t = nextToken.fetch_add (1, std::memory_order_relaxed) + 1;
    hop ([t] (StatusBar& s)
    {
        s.activityToken = t;             // supersedes any older claim
        s.activityStart  = juce::Time::getCurrentTime();
        s.activityRatio  = -1.0f;
        s.startTimerHz (10);             // spinner + elapsed repaint
        s.repaint();
    });
    return ProgressActivity (this, t);
}

inline void StatusBar::setQueue (int done, int total)
{
    hop ([done, total] (StatusBar& s)
    {
        s.queueDone  = done;
        s.queueTotal = total;
        s.repaint();
        // Hiding is the CALLER's decision (the queue posts (0, 0) on
        // drain) - no auto-hide here: a determinate-looking "N/N" may
        // legitimately mean "last task still running".
    });
}

inline void StatusBar::ProgressActivity::progress (float ratio01)
{
    if (bar == nullptr || token == 0) return;
    const auto t = token;
    bar->hop ([t, ratio01] (StatusBar& s)
    {
        if (s.activityToken == t) { s.activityRatio = ratio01; s.repaint(); }
    });
}

inline void StatusBar::ProgressActivity::finish()
{
    if (bar == nullptr || token == 0) return;
    const auto t = token;
    token = 0;
    bar->hop ([t] (StatusBar& s)
    {
        if (s.activityToken == t)
        {
            s.activityToken = 0;
            s.activityRatio = -1.0f;
            s.stopTimer();
            s.repaint();
        }
    });
}

inline void StatusBar::ProgressActivity::release()
{
    finish();   // token guard makes double release harmless
}

} // namespace ebs
