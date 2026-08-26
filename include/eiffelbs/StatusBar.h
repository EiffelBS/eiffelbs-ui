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

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <deque>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"

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
                  public juce::SettableTooltipClient
{
public:
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
        const auto pfx = transientPrefixOf (line);
        bool replaced = false;
        if (pfx.isNotEmpty() && ! history.empty())
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
        g.fillAll (bgPanel());
        g.setColour (panelBorder().withAlpha (0.6f));
        g.drawLine (0.0f, 0.5f, (float) getWidth(), 0.5f, 1.0f);

        // Last line, LEFT-TRUNCATED (the tail is what matters).
        g.setFont (createFont (13.0f, false));
        g.setColour (textDim());
        const auto avail = (float) getWidth() - counterW - 16.0f;
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
        g.drawText (text, 8, 0, (int) avail, getHeight(),
                    juce::Justification::centredLeft, true);

        // Discreet counter on the right.
        g.setColour (panelBorder());
        g.drawText (juce::String (history.size()) + " lines",
                    getWidth() - counterW, 0, counterW, getHeight(),
                    juce::Justification::centredRight, true);
    }

    void mouseDown (const juce::MouseEvent&) override { showLogWindow(); }

private:
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
};

} // namespace ebs
