// LookAndFeel.h
// eiffelbs-ui - application-wide LookAndFeel and frame helpers
// (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Adapted from OpenTimbre source/ui/OTLookAndFeel.h (class OTLookAndFeel),
// itself adapted from OpenVoxTuner Source/ui/LookAndFeel.{h,cpp} and
// Source/ui/OVTFonts.h (original copyright (C) 2026 EiffelBS).
//
// Painting coverage: global colour table, buttons (incl. the "primary"
// ComponentID contract), rotary knobs (pivot-centred fill support),
// toggle checkbox, combo box + text positioning, popup menu background/
// font, tooltip rendering/bounds, tab bar painting, slim scrollbars,
// TextEditor fill/outline and Label font fallback.
//
// Colour resolution for eiffelbs widgets follows the three-level contract:
// per-instance setColour() > widgetThemeColour() hook > built-in palette
// default (documented on the widgets; see Knob.h for an example).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"

namespace ebs
{
    // Free functions shared with the widget headers; defined at the bottom
    // of this file, AFTER the LookAndFeel class they depend on.
    juce::Colour widgetColour (const juce::Component& origin, int colourId,
                               juce::Colour builtin);
    void drawKnob (juce::Graphics& g, int x, int y, int width, int height,
                   float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                   juce::Slider& slider);

    // === Shared group-box frame ==========================================
    //
    // Visual language from OpenVoxTuner's bottom blocks (bgPanel fill +
    // soft accent-tinted rounded outline, 6 px corners).
    //
    // The frame itself is a CONTINUOUS rounded panel: no notch, no tab
    // punched into the border. Titles are drawn by each panel as a plain
    // label fully INSIDE the frame on the internal top band (left padding
    // 10 px), on the same bgPanel surface as the body - so text never
    // overlaps the accent border.
    inline void drawGroupFrame (juce::Graphics& g,
                                const juce::Rectangle<float>& bounds)
    {
        constexpr float corner = 6.0f;

        // Raised body, clearly distinct from the window background layer.
        g.setColour (bgPanel());
        g.fillRoundedRectangle (bounds, corner);

        // Continuous rounded border with a soft accent trim (25 % alpha).
        g.setColour (accent().withAlpha (0.25f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

        // TODO: optional horizontal expand chevron at the right end of the
        // title band (OVT-like collapsible frame) - deliberately deferred.
    }

    /** Title-less variant wrapping whole tab pages: identical continuous
     *  rounded bgPanel body and accent border, no text band. The area
     *  around it stays on the window background (bgDark). */
    inline void drawFramePlain (juce::Graphics& g,
                                const juce::Rectangle<float>& bounds)
    {
        drawGroupFrame (g, bounds);
    }

    /** Rounded framed panel component used by apps as a drop-in group
     *  body (see FramedBody.h). Declared after the class below. */

    class LookAndFeel : public juce::LookAndFeel_V4, public ThemeSubscriber
    {
    public:
        LookAndFeel() { subscribeTheme (this); refreshThemeColours(); }
        ~LookAndFeel() override { unsubscribeTheme (this); }

        void themeChanged() override { refreshThemeColours(); }

        /** Refresh all LookAndFeel colours to match the current theme.
            Called automatically on ebs::setTheme(); manual calls remain
            valid for legacy currentTheme() assignments. */
        void refreshThemeColours()
        {
            // --- Buttons -------------------------------------------------
            setColour (juce::TextButton::buttonColourId,     bgPanel());
            setColour (juce::TextButton::buttonOnColourId,   accentSoft());
            setColour (juce::TextButton::textColourOffId,    text());
            setColour (juce::TextButton::textColourOnId,     juce::Colours::white);

            // --- ComboBox ------------------------------------------------
            setColour (juce::ComboBox::backgroundColourId,        bgDark());
            setColour (juce::ComboBox::outlineColourId,           bgPanel());
            setColour (juce::ComboBox::textColourId,              text());
            setColour (juce::ComboBox::arrowColourId,             accent());
            setColour (juce::ComboBox::focusedOutlineColourId,    accent().withAlpha (0.6f));

            // --- Labels --------------------------------------------------
            setColour (juce::Label::textColourId,            text());
            setColour (juce::Label::outlineColourId,         juce::Colours::transparentBlack);

            // --- TextEditor ----------------------------------------------
            setColour (juce::TextEditor::backgroundColourId,       bgDark());
            setColour (juce::TextEditor::textColourId,             text());
            setColour (juce::TextEditor::highlightColourId,        accentSoft());
            setColour (juce::TextEditor::highlightedTextColourId,  text());
            setColour (juce::TextEditor::outlineColourId,          panelBorder());
            setColour (juce::TextEditor::focusedOutlineColourId,   accent().withAlpha (0.65f));
            setColour (juce::CaretComponent::caretColourId,        accent());

            // --- ToggleButton --------------------------------------------
            setColour (juce::ToggleButton::textColourId,     text());
            setColour (juce::ToggleButton::tickColourId,     accent());
            setColour (juce::ToggleButton::tickDisabledColourId, panelBorder());

            // --- ListBox / ScrollBar --------------------------------------
            setColour (juce::ListBox::backgroundColourId,    bgDark());
            setColour (juce::ListBox::outlineColourId,       panelBorder());
            setColour (juce::ListBox::textColourId,          text());
            setColour (juce::ScrollBar::backgroundColourId,  juce::Colours::transparentBlack);
            setColour (juce::ScrollBar::thumbColourId,       panelBorder());
            setColour (juce::ScrollBar::trackColourId,       bgDark());

            // --- Tabs -----------------------------------------------------
            setColour (juce::TabbedComponent::backgroundColourId, bgDark());
            setColour (juce::TabbedComponent::outlineColourId,    panelBorder());
            setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::TabbedButtonBar::tabTextColourId,    textDim());

            // Tooltip: transparent background so our drawTooltip rounded
            // rect is the only fill (as in OpenVoxTuner).
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::TooltipWindow::textColourId,       text());

            // Popup menus (combo dropdowns).
            setColour (juce::PopupMenu::backgroundColourId,            bgDark());
            setColour (juce::PopupMenu::textColourId,                  text());
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accentSoft());
            setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

            // Alert windows (delete confirmations).
            setColour (juce::AlertWindow::backgroundColourId, bgPanel());
            setColour (juce::AlertWindow::outlineColourId,    panelBorder());
            setColour (juce::AlertWindow::textColourId,       text());

            // Standalone window background behind the editor.
            setColour (juce::ResizableWindow::backgroundColourId, bgDark());
        }

        /** Theme-level restyling hook for eiffelbs widget ColourIds
            (IconButton / StatusBar). Return a NON-transparent colour to
            override a widget's built-in default; per-instance
            Component::setColour() calls still win over this hook.
            Default implementation overrides nothing, so widgets keep their
            exact pre-v0.2 palette behaviour unless a host opts in. */
        virtual juce::Colour widgetThemeColour ([[maybe_unused]] int colourId)
        {
            return {};
        }

        /** Shared chrome colour hooks (three-level contract, THEME level).

            These ids restyle LookAndFeel-painted surfaces shared across
            many components (tab bars, checkbox wells). Per-instance
            setColour() is deliberately NOT part of chrome lookups: those
            surfaces have no single owner component; override
            widgetThemeColour() in an app-side LookAndFeel subclass to
            restyle them app-wide (see the OpenVoxTuner cutover note).

            Built-ins below return the exact pre-v0.3 colours, so existing
            consumers keep identical pixels unless they opt in. */
        enum ThemeChromeIds
        {
            tabActiveFillColourId   = 0x3a5c9101,  ///< active tab pill fill
            tabActiveTextColourId   = 0x3a5c9102,  ///< active tab label text
            tabBarLineColourId      = 0x3a5c9103,  ///< bar bottom accent rule
            checkboxFillColourId    = 0x3a5c9104,  ///< checkbox well fill
            checkboxOutlineColourId = 0x3a5c9105   ///< checkbox well border
        };

        /** Chrome lookup: theme hook first, built-in default otherwise.
            NB: intentionally NON-const - it invokes the non-const virtual
            widgetThemeColour() hook, matching its v0.2.0 signature so
            existing subclasses stay source-compatible. */
        juce::Colour chromeColour (int id, juce::Colour builtin)
        {
            if (juce::Colour themed = widgetThemeColour (id);
                ! themed.isTransparent())
                return themed;
            return builtin;
        }

        // === Buttons ===============================================

        /** Rounded theme buttons: panel fill at rest, accent when pressed,
            accentSoft when toggled on. PRIMARY actions: a button whose
            ComponentID is "primary" is filled with the accent colour at
            rest (white text, hover brighter, down darker) so key actions
            read at a glance.

            Per-instance fills: a button with an EXPLICITLY SET
            TextButton::buttonColourId keeps that colour as its body tint
            across rest / hover / down / toggled states (brighter/darker
            variations on hover/down) while inheriting the shared accent
            hover rule (the 1 px "liseret"). Buttons without an explicit
            colour get the built-in form-style treatment. This is how apps
            carry translucent "chip" buttons into this theme without losing
            their tinted look. */
        void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                   const juce::Colour& /*backgroundColour*/,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
            const float corner = 4.0f;

            const bool explicitFill =
                button.isColourSpecified (juce::TextButton::buttonColourId);
            const auto instFill = explicitFill
                ? button.findColour (juce::TextButton::buttonColourId)
                : juce::Colours::black;

            juce::Colour fill, outline;
            if (button.getComponentID() == "primary")
            {
                fill    = shouldDrawButtonAsDown
                              ? accent().darker (0.10f)
                          : shouldDrawButtonAsHighlighted
                              ? accent().brighter (0.08f)
                              : accent();
                outline = juce::Colours::transparentBlack;
            }
            else if (shouldDrawButtonAsDown)
            {
                fill = explicitFill ? instFill.darker (0.12f) : accent();
                // GENERAL hover accent rule, pressed variant: stronger.
                if (shouldDrawButtonAsHighlighted)
                    outline = accent().withAlpha (0.55f);
                else
                    outline = juce::Colours::transparentBlack;
            }
            else if (button.getToggleState())
            {
                // Toggle state must be unmistakable. An instance
                // buttonOnColourId wins outright; otherwise a chip that
                // only set buttonColourId gets its fill visibly lifted
                // from the rest state, and bare buttons switch to the
                // solid accent. Explicit ON fills carry no outline - the
                // fill itself is the active-state signal.
                const bool explicitOn =
                    button.isColourSpecified (juce::TextButton::buttonOnColourId);
                if (explicitOn)
                {
                    fill    = button.findColour (juce::TextButton::buttonOnColourId);
                    outline = juce::Colours::transparentBlack;
                }
                else
                {
                    fill    = explicitFill ? instFill.brighter (0.35f) : accent();
                    outline = accent().withAlpha (0.55f);
                }
                if (shouldDrawButtonAsHighlighted)
                    fill = fill.brighter (0.08f);
            }
            else if (shouldDrawButtonAsHighlighted)
            {
                fill = explicitFill ? instFill.brighter (0.07f)
                                    : bgDark().brighter (0.07f);
                // GENERAL hover accent rule (every non-primary button):
                // accent-tinted 1 px outline over the standard hover fill.
                outline = accent().withAlpha (0.45f);
            }
            else
            {
                // Text buttons rest on the SAME fill as the input fields
                // (TextEditors/ComboBoxes), so they read as part of the
                // form instead of floating light blocks.
                fill = explicitFill ? instFill : bgDark();
                outline = panelBorder();
            }

            if (! button.isEnabled())
                outline = outline.withAlpha (0.35f);

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, corner);
            g.setColour (outline);
            g.drawRoundedRectangle (bounds, corner, 1.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                             bool /*shouldDrawButtonAsHighlighted*/,
                             bool shouldDrawButtonAsDown) override
        {
            g.setFont (getTextButtonFont (button, button.getHeight()));
            // Colour contract: instance-level textColourOffId wins; the
            // palette token is only the LookAndFeel-level default. This
            // lets canvas-overlay buttons (drawn on theme-invariant dark
            // islands) opt into a fixed bright label under both themes.
            auto colour = button.findColour (juce::TextButton::textColourOffId);
            if (colour.isTransparent())
                colour = findColour (juce::TextButton::textColourOffId);
            // White stays readable over the pressed/toggled accent fills.
            if (! button.isEnabled())
                colour = textDim();
            else if (shouldDrawButtonAsDown || button.getToggleState())
                colour = juce::Colours::white;
            g.setColour (colour);
            g.drawText (button.getButtonText(), button.getLocalBounds(),
                        juce::Justification::centred, false);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int /*buttonHeight*/) override
        {
            return fontComboBox();
        }

        // === Rotary knob (ported from OpenVoxTuner) ==================

        /** EiffelBS knob: dark arc track, accent value arc with optional
            pivot-centred fill ("centred" property / Knob::setCentredFill),
            gradient cap and thin pointer. Colour resolution for the arcs:
            instance setColour > widgetThemeColour() hook > live palette
            (accent()/accentSoft()). Applies to every rotary Slider drawn
            through this LookAndFeel - ebs::Knob is the typed flavour. */
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider& slider) override
        {
            ebs::drawKnob (g, x, y, width, height, sliderPos,
                           rotaryStartAngle, rotaryEndAngle, slider);
        }

        // === ToggleButton (checkbox branch from OpenVoxTuner) ========

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                               bool /*shouldDrawButtonAsHighlighted*/,
                               bool /*shouldDrawButtonAsDown*/) override
        {
            auto bounds = button.getLocalBounds().toFloat();
            const bool isOn = button.getToggleState();

            auto size = juce::jmin (16.0f, bounds.getHeight() * 0.7f);
            auto rect = juce::Rectangle<float> (0.0f, (bounds.getHeight() - size) * 0.5f,
                                                size, size);

            // Well default is theme-aware (v0.4.1): canvas tone on Dark,
            // white well under Light so the accent check keeps contrast.
            g.setColour (chromeColour (checkboxFillColourId,
                                       isDark() ? vizBg() : juce::Colours::white));
            g.fillRoundedRectangle (rect, 3.0f);
            // Chrome hook applies to the well border UNIFORMLY over both
            // states (default keeps the state-dependent pair unchanged).
            g.setColour (chromeColour (checkboxOutlineColourId,
                                       isOn ? accent() : panelBorder()));
            g.drawRoundedRectangle (rect, 3.0f, 1.0f);

            if (isOn)
            {
                juce::Path check;
                check.startNewSubPath (rect.getX() + size * 0.2f, rect.getY() + size * 0.5f);
                check.lineTo (rect.getX() + size * 0.4f, rect.getY() + size * 0.7f);
                check.lineTo (rect.getX() + size * 0.8f, rect.getY() + size * 0.3f);
                g.strokePath (check, juce::PathStrokeType (2.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded));
            }

            g.setColour (button.findColour (juce::ToggleButton::textColourId));
            g.setFont (fontToggleButton());
            g.drawText (button.getButtonText(), bounds.withTrimmedLeft (size + 6.0f),
                        juce::Justification::centredLeft);
        }

        // === ComboBox (ported from OpenVoxTuner) =====================

        void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox& box) override
        {
            juce::ignoreUnused (isButtonDown);
            auto cornerSize = 4.0f;
            juce::Rectangle<int> boxBounds (0, 0, width, height);

            g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
            g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

            g.setColour (box.findColour (box.hasKeyboardFocus (true)
                                             ? juce::ComboBox::focusedOutlineColourId
                                             : juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

            juce::Path path;
            auto x = buttonX + buttonW * 0.5f;
            auto y = buttonY + buttonH * 0.5f;
            auto w = juce::jmin (buttonW, buttonH) * 0.25f;

            path.addTriangle (x - w, y - w * 0.5f,
                              x + w, y - w * 0.5f,
                              x, y + w * 0.5f);

            g.setColour (box.findColour (juce::ComboBox::arrowColourId)
                              .withAlpha (box.isEnabled() ? 1.0f : 0.3f));
            g.fillPath (path);
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (1, 1, box.getWidth() - 30, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override
        {
            return fontComboBox();
        }

        /** Labels keep any explicitly assigned font; labels left on the
            JUCE default receive the themed combo font (OpenVoxTuner returns
            it unconditionally, but several app labels carry their own
            sizes). */
        juce::Font getLabelFont (juce::Label& label) override
        {
            static const float defaultHeight = []
                { juce::Label probe; return probe.getFont().getHeight(); }();

            const auto& f = label.getFont();
            const bool untouched = std::abs (f.getHeight() - defaultHeight) < 0.01f
                                   && f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName();
            return untouched ? fontComboBox() : f;
        }

        juce::Font getPopupMenuFont() override
        {
            // Route through the scaled font helper so dropdown menus match
            // the other text elements (see OpenVoxTuner note).
            return fontPopupMenu();
        }

        // === TextEditor (theme-language addition) ====================

        void fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                       juce::TextEditor&) override
        {
            g.setColour (bgDark());
            g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
        }

        void drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                    juce::TextEditor& editor) override
        {
            if (editor.isEnabled())
            {
                const auto b = juce::Rectangle<float> (0.5f, 0.5f,
                                                       (float) width - 1.0f,
                                                       (float) height - 1.0f);
                g.setColour (editor.hasKeyboardFocus (true)
                                 ? accent().withAlpha (0.65f)
                                 : panelBorder());
                g.drawRoundedRectangle (b, 4.0f, 1.0f);
            }
        }

        // === Tooltip (ported from OpenVoxTuner) ======================

        void drawTooltip (juce::Graphics& g, const juce::String& text,
                          int width, int height) override
        {
            g.setColour (bgDark());
            g.fillRect (0, 0, width, height);

            g.setColour (accentSoft());
            g.drawRect (0, 0, width, height, 1);

            juce::AttributedString s;
            s.setJustification (juce::Justification::centredLeft);
            // NB: qualified ebs::text() - the parameter shadows the
            // namespace-scope colour function.
            s.append (text, fontTooltip(), ebs::text());

            juce::TextLayout layout;
            layout.createLayout (s, (float) width - 16.0f);
            layout.draw (g, juce::Rectangle<float> (8.0f, 6.0f,
                                                    (float) width - 16.0f,
                                                    (float) height - 12.0f));
        }

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText,
                                               juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override
        {
            const int maxWidth = 360;
            const int minWidth = 120;

            int widestLine = 0;
            juce::StringArray lines;
            lines.addLines (tipText);
            for (const auto& line : lines)
                widestLine = juce::jmax (widestLine,
                    static_cast<int> (juce::GlyphArrangement::getStringWidth (
                        fontTooltip(), line)));

            int width = juce::jlimit (minWidth, maxWidth, widestLine + 16);

            juce::AttributedString s;
            s.setJustification (juce::Justification::centredLeft);
            s.append (tipText, fontTooltip(), text());

            juce::TextLayout layout;
            layout.createLayout (s, (float) width - 16.0f);
            const int height = (int) std::ceil (layout.getHeight() + 12.0f);

            int x = screenPos.x - width / 2;
            int y = screenPos.y - height - 14;

            if (y < parentArea.getY())
                y = screenPos.y + 24;

            x = juce::jlimit (parentArea.getX(), parentArea.getRight() - width, x);
            y = juce::jlimit (parentArea.getY(), parentArea.getBottom() - height, y);

            return { x, y, width, height };
        }

        // === Popup menus (ported from OpenVoxTuner) ==================

        void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
        {
            // Force the app background for all popup menus.
            g.setColour (bgDark());
            g.fillRect (0, 0, width, height);

            // Subtle border.
            g.setColour (bgPanel());
            g.drawRect (0, 0, width, height, 1);
        }

        // === Tabs (ported from OpenVoxTuner, OpenTimbre variant) =====

        void drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar,
                                            juce::Graphics& g) override
        {
            auto barBounds = bar.getLocalBounds().toFloat();

            // Tab bar background matching the app background.
            g.setColour (bgDark());
            g.fillRect (barBounds);

            // Subtle bottom line in the header accent tint (chrome hookable).
            g.setColour (chromeColour (tabBarLineColourId, headerAccent()));
            g.drawHorizontalLine ((int) barBounds.getBottom() - 1, 0.0f,
                                  barBounds.getWidth());
        }

        void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                            bool isMouseOver, bool isMouseDown) override
        {
            auto tabBounds = button.getLocalBounds().toFloat();
            const float indent = 2.0f;
            const float tabHeight = tabBounds.getHeight();
            const bool isFrontTab = (button.getToggleState());

            if (isFrontTab)
            {
                // Active tab pill fill (chrome-hookable, built-in unchanged).
                g.setColour (chromeColour (tabActiveFillColourId, accentSoft()));
                auto activeTab = tabBounds.reduced (indent, 0.0f)
                                     .removeFromTop (tabHeight - 1.0f);
                g.fillRoundedRectangle (activeTab, 4.0f);
            }
            else if (isMouseOver || isMouseDown)
            {
                // Inactive tab hover: subtle highlight.
                g.setColour (accentSoft().withAlpha (0.15f));
                auto hoverTab = tabBounds.reduced (indent, 0.0f)
                                      .removeFromTop (tabHeight - 1.0f);
                g.fillRoundedRectangle (hoverTab, 4.0f);
            }

            // IMPORTANT: Draw the tab text (delegates to drawTabButtonText).
            drawTabButtonText (button, g, isMouseOver, isMouseDown);
        }

        void drawTabButtonText (juce::TabBarButton& button, juce::Graphics& g,
                                bool /*isMouseOver*/, bool /*isMouseDown*/) override
        {
            auto tabBounds = button.getLocalBounds();
            const bool isFrontTab = (button.getToggleState());

            g.setFont (fontComboBox());
            g.setColour (isFrontTab ? chromeColour (tabActiveTextColourId, accent())
                                    : textDim());
            g.drawText (button.getButtonText(), tabBounds,
                        juce::Justification::centred, false);
        }

        // === ScrollBar (modern slim thumb, theme colours) ============

        void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y,
                            int width, int height, bool isVertical,
                            int thumbStartPosition, int thumbSize,
                            bool isMouseOver, bool isMouseDown) override
        {
            if (thumbSize <= 0)
                return;

            juce::Rectangle<float> thumb = isVertical
                ? juce::Rectangle<float> ((float) x, (float) (y + thumbStartPosition),
                                          (float) width, (float) thumbSize)
                : juce::Rectangle<float> ((float) (x + thumbStartPosition), (float) y,
                                          (float) thumbSize, (float) height);

            auto c = isMouseDown      ? accent()
                     : isMouseOver    ? accent().withAlpha (0.60f)
                                      : panelBorder();
            g.setColour (c);
            g.fillRoundedRectangle (thumb.reduced (2.0f), 3.0f);
        }
    };

    // === Free helper implementations =====================================

    /** Shared three-level colour resolution for eiffelbs widgets:
        per-instance setColour() > ebs::LookAndFeel::widgetThemeColour()
        hook > built-in default. Every new widget routes through this so
        the contract stays single-sourced. */
    inline juce::Colour widgetColour (const juce::Component& origin, int colourId,
                                      juce::Colour builtin)
    {
        if (origin.isColourSpecified (colourId))
            return origin.findColour (colourId);
        // Same call shape as StatusBar::resolved(): Component::getLookAndFeel()
        // hands back a non-const reference even from read-only contexts.
        if (auto* l = dynamic_cast<LookAndFeel*> (&origin.getLookAndFeel()))
            if (juce::Colour themed = l->widgetThemeColour (colourId);
                ! themed.isTransparent())
                return themed;
        return builtin;
    }

    inline void drawKnob (juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider)
    {
        auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - 4.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto rx      = centreX - radius;
        auto ry      = centreY - radius;
        auto rw      = radius * 2.0f;
        auto angle   = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Arc track background.
        g.setColour (bgPanel().darker (0.5f));
        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                                     rotaryStartAngle, rotaryEndAngle, true);
        g.strokePath (backgroundArc, juce::PathStrokeType (6.0f,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded));

        if (slider.isEnabled())
        {
            // Pivot-aware fill: knobs flagged "centred" grow their filled
            // arc symmetrically from the central range value instead of
            // starting at the minimum.
            const bool centred = slider.getProperties().contains ("centred")
                                 && (bool) slider.getProperties()["centred"];
            g.setColour (widgetColour (slider, juce::Slider::rotarySliderFillColourId,
                                       accent()));
            juce::Path valueArc;
            if (centred)
            {
                const auto range = slider.getNormalisableRange();
                const double pivotValue = (range.start + range.end) * 0.5;
                const double pivotPos  = range.convertTo0to1 (pivotValue);
                const float pivotAngle = rotaryStartAngle
                                       + (float) pivotPos * (rotaryEndAngle - rotaryStartAngle);
                // Always draw the SHORT arc between pivot and current value,
                // using clockwise=true with ordered angles. Passing
                // clockwise=false with start>end makes addCentredArc go the
                // long way around, drawing a spurious arc on the other side.
                const float arcStart = juce::jmin (angle, pivotAngle);
                const float arcEnd   = juce::jmax (angle, pivotAngle);
                valueArc.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                                        arcStart, arcEnd, true);
            }
            else
            {
                valueArc.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                                        rotaryStartAngle, angle, true);
            }
            g.strokePath (valueArc, juce::PathStrokeType (6.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));
        }

        // Knob cap: dark vertical gradient + soft black outline.
        const auto capLight = isDark() ? juce::Colour::fromString ("#FF303030")
                                       : juce::Colour::fromString ("#FF505050");
        const auto capDark  = isDark() ? juce::Colour::fromString ("#FF151515")
                                       : juce::Colour::fromString ("#FF383838");
        juce::ColourGradient grad (capLight, centreX, centreY - radius,
                                   capDark, centreX, centreY + radius, false);
        g.setGradientFill (grad);
        g.fillEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f);

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f, 2.0f);

        // Thin pointer line (modern look).
        juce::Path p;
        const float pointerLength    = radius - 6.0f;
        const float pointerThickness = 2.5f;
        p.startNewSubPath (0.0f, -radius + 8.0f);
        p.lineTo (0.0f, -radius + 8.0f + pointerLength * 0.4f);
        p.applyTransform (juce::AffineTransform::rotation (angle)
                              .translated (centreX, centreY));
        g.setColour (slider.isEnabled() ? juce::Colours::white
                                        : juce::Colours::grey.withAlpha (0.5f));
        g.strokePath (p, juce::PathStrokeType (pointerThickness,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded));
    }
}
