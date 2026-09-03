// Smoke.cpp
// Standalone offscreen proof for eiffelbs-ui.
//
// Exercises every public header: instantiates the LookAndFeel and the
// widgets, renders through the REAL painting code into software images and
// asserts a few pixel-level outcomes (primary button accent fill, popup
// background, group frame, toggle checkbox). Exit code 0 = PASS. A composed
// PNG receipt is written to %TEMP%\eiffelbs-ui-smoke.png for eyeballing.
//
// Headless by design: no window is created (DialogWindow is deliberately
// NOT instantiated); the ScopedJuceInitialiser_GUI only wires the message
// thread so StatusBar::logLine can take its synchronous path.

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/eiffelbs.h>

#include <cstdio>

namespace
{
    int failures = 0;

    // juce::Button::paint is PROTECTED (unlike Component::paint), so direct
    // offscreen rendering needs these minimal test wrappers: using-
    // declarations lift paint AND inherit the base constructors without
    // touching the library.
    struct PaintableTextButton : juce::TextButton
    {
        using juce::TextButton::TextButton;
        using juce::TextButton::paint;
    };
    struct PaintableToggleButton : juce::ToggleButton
    {
        using juce::ToggleButton::ToggleButton;
        using juce::ToggleButton::paint;
    };
    struct PaintableIconButton : ebs::IconButton
    {
        using ebs::IconButton::IconButton;
        using ebs::IconButton::paint;
    };

    void check (bool ok, const char* what)
    {
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok)
            ++failures;
    }

    /** Pixel-level check with a colour diff on failure. */
    void checkPixel (const juce::Image& img, juce::Point<int> p,
                     const juce::Colour& expected, const char* what)
    {
        const auto actual = img.getPixelAt (p.x, p.y);
        const bool ok = (actual == expected);
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok)
        {
            ++failures;
            std::printf ("       at (%d,%d) actual=%s expected=%s\n",
                         p.x, p.y,
                         actual.toString().toRawUTF8(),
                         expected.toString().toRawUTF8());
        }
    }

    /** Render `draw` into a fresh opaque ARGB image (pre-filled black). */
    template <typename Fn>
    juce::Image renderToImage (int w, int h, Fn&& draw)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colours::black);
        draw (g);
        return img;
    }

    /** Count pixels with meaningful alpha (did anything actually paint?). */
    int inkPixels (const juce::Image& img)
    {
        int n = 0;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
                if (img.getPixelAt (x, y).getAlpha() > 10)
                    ++n;
        return n;
    }

    void writePngReceipt (const juce::Image& img)
    {
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("eiffelbs-ui-smoke.png");
        if (auto out = file.createOutputStream())
        {
            juce::PNGImageFormat().writeImageToStream (img, *out);
            out->flush();
        }
        std::printf ("[INFO] PNG receipt: %s (%d bytes)\n",
                     file.getFullPathName().toRawUTF8(),
                     (int) file.getSize());
    }
}

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    // === Palette ============================================================
    // NB: direct Colour equality - Colour::toString() renders lowercase
    // AARRGGBB WITHOUT a '#' prefix, so string comparison is brittle.
    check (ebs::isDark(),                                   "default theme is Dark");
    check (ebs::bgDark() == juce::Colour (0xff26282b),      "dark main background hex");
    check (ebs::accent() == juce::Colour (0xff1a9af0),      "dark accent hex");
    check (ebs::panelBorder() == ebs::bgPanel().brighter (0.08f),
                                                            "panel border derives from panel colour");

    ebs::currentTheme() = ebs::Theme::Light;
    check (! ebs::isDark(),                                 "theme switches to Light");
    check (ebs::bgDark() == juce::Colour (0xfff0f1f5),      "light main background hex");
    check (ebs::accent() == juce::Colour (0xff1565c0),      "light accent hex");
    ebs::currentTheme() = ebs::Theme::Dark;

    // === Fonts ==============================================================
    check (ebs::createFont (13.0f).getHeight() > 0.0f,      "createFont returns a valid font");
    check (ebs::fontPopupMenu().getHeight() == 17.0f,       "popup menu font bypasses platform scale");
    check (ebs::createMonospaceFont (12.0f).getTypefaceName()
               == juce::Font::getDefaultMonospacedFontName(),
                                                            "monospace family");

    // === Widgets + LookAndFeel (default L&F scoped to this block) ===========
    {
        ebs::LookAndFeel lnf;
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

        // Primary TextButton: rests filled with the accent colour.
        // NB: components are painted AT ORIGIN (a real parent would
        // translate the Graphics context by the child's position; calling
        // paint() directly does not, so local bounds == image coords here).
        PaintableTextButton primary ("Go");
        primary.setComponentID ("primary");
        const auto primaryImg = renderToImage (110, 60, [&] (juce::Graphics& g)
            { primary.setBounds (0, 0, 70, 28); primary.paint (g); });
        checkPixel (primaryImg, { 6, 14 }, ebs::accent(),
                    "primary button rests filled with the accent colour");

        // Plain TextButton: rests on the input-field fill (bgDark).
        PaintableTextButton plain ("Edit");
        const auto plainImg = renderToImage (110, 60, [&] (juce::Graphics& g)
            { plain.setBounds (0, 0, 70, 28); plain.paint (g); });
        checkPixel (plainImg, { 6, 14 }, ebs::bgDark(),
                    "plain button rests on the shared form fill");

        // "Chip" contract: a TextButton with an EXPLICIT buttonColourId
        // keeps that translucent tint at rest AND when toggled, instead of
        // falling back to the built-in form/accentSoft fills. (The accent
        // hover rule itself stays shared and is not sampled here - hover
        // is mouse-driven.)
        PaintableTextButton chip ("");
        chip.setSize (60, 24);
        const auto chipTint = juce::Colour (0x331A9AF0);
        chip.setColour (juce::TextButton::buttonColourId, chipTint);
        {
            const uint8_t fa = chipTint.getAlpha();
            // Translucent tint composed over this canvas's black base
            // (round-to-nearest like JUCE's software blender).
            const auto composeCh = [fa] (uint8_t ch)
                { return (uint8_t) ((ch * fa + 127) / 255); };
            const auto composed = juce::Colour::fromRGBA (
                composeCh (chipTint.getRed()),
                composeCh (chipTint.getGreen()),
                composeCh (chipTint.getBlue()), 255);
            const auto restImg = renderToImage (60, 24, [&] (juce::Graphics& g)
                { chip.setBounds (0, 0, 60, 24); chip.paint (g); });
            checkPixel (restImg, { 4, 12 }, composed,
                        "explicit TextButton fill carries the instance tint at rest");
            chip.setToggleState (true, juce::dontSendNotification);
            const auto onImg = renderToImage (60, 24, [&] (juce::Graphics& g)
                { chip.paint (g); });
            checkPixel (onImg, { 4, 12 }, composed,
                        "explicit TextButton fill survives the toggled state");
        }

        // ToggleButton (checked): under Dark the interior keeps the visualizer bg.
        PaintableToggleButton tog ("Option");
        tog.setToggleState (true, juce::dontSendNotification);
        const auto togImg = renderToImage (260, 60, [&] (juce::Graphics& g)
            { tog.setBounds (0, 0, 220, 24); tog.paint (g); });
        // Checkbox rect = local (0,4,16,16): sample the top-right INTERIOR,
        // clear of both the 1 px border and the check-mark stroke band.
        checkPixel (togImg, { 12, 6 }, ebs::vizBg(),
                    "toggle checkbox interior uses the visualizer background");

        // v0.4.1: the well default is theme-aware - white under Light so the
        // accent tick keeps contrast, then restore Dark for the rest of the run.
        ebs::setTheme (ebs::Theme::Light);
        const auto togImgLight = renderToImage (260, 60, [&] (juce::Graphics& g)
            { tog.setBounds (0, 0, 220, 24); tog.paint (g); });
        checkPixel (togImgLight, { 12, 6 }, juce::Colours::white,
                    "light theme turns the checkbox well white by default");
        ebs::setTheme (ebs::Theme::Dark);

        // Popup menu background: app background + subtle border.
        const auto popImg = renderToImage (80, 60, [&] (juce::Graphics& g)
            { lnf.drawPopupMenuBackground (g, 80, 60); });
        check (popImg.getPixelAt (40, 30) == ebs::bgDark(),
               "popup menu background uses the app background");

        // Shared group-frame helper: continuous bgPanel body.
        const auto frameImg = renderToImage (100, 80, [] (juce::Graphics& g)
            { ebs::drawFramePlain (g, juce::Rectangle<float> (10.f, 10.f, 80.f, 60.f)); });
        check (frameImg.getPixelAt (50, 40) == ebs::bgPanel(),
               "group frame fills with the panel colour");

        // Every IconButton shape must construct AND leave ink on screen
        // (catches broken SVG parsing as well as painting regressions).
        const ebs::IconButton::Shape shapes[] =
        {
            ebs::IconButton::Shape::play,    ebs::IconButton::Shape::stop,
            ebs::IconButton::Shape::cross,   ebs::IconButton::Shape::refresh,
            ebs::IconButton::Shape::folder,  ebs::IconButton::Shape::grip,
            ebs::IconButton::Shape::undo,    ebs::IconButton::Shape::redo,
            ebs::IconButton::Shape::settings, ebs::IconButton::Shape::star,
            ebs::IconButton::Shape::wand,    ebs::IconButton::Shape::tool,
            ebs::IconButton::Shape::search,  ebs::IconButton::Shape::lock
        };
        bool allShapesInk = true;
        for (const auto s : shapes)
        {
            PaintableIconButton ib (s);
            ib.setSize (28, 28);
            const auto img = renderToImage (28, 28, [&] (juce::Graphics& g)
                { ib.paint (g); });
            if (inkPixels (img) < 5)
                allShapesInk = false;
        }
        check (allShapesInk, "every IconButton shape paints visible ink");

        // FramedBody: paints the shared frame language.
        ebs::FramedBody body;
        const auto bodyImg = renderToImage (100, 80, [&] (juce::Graphics& g)
            { body.setBounds (0, 0, 80, 60); body.paint (g); });
        checkPixel (bodyImg, { 40, 30 }, ebs::bgPanel(),
                    "FramedBody paints the shared frame");

        // StatusBar: thread-safe logLine (we ARE the message thread here)
        // followed by a real paint pass.
        ebs::StatusBar bar;        bar.logLine ("smoke test line");
        bar.logLine ("Generating... 42 s elapsed");
        const auto barImg = renderToImage (300, 24, [&] (juce::Graphics& g)
            { bar.setSize (300, 24); bar.paint (g); });
        check (barImg.getPixelAt (150, 12) == ebs::bgPanel(),
               "status bar paints the panel surface");

        // v0.7.1 HelpBubble: accent ring + "?" glyph actually paint, and the
        // tooltip plumbing (SettableTooltipClient) is present.
        ebs::HelpBubble bubble;
        bubble.setTooltip ("help bubble smoke tooltip");
        const auto bubbleImg = renderToImage (14, 14, [&] (juce::Graphics& g)
            { bubble.paint (g); });
        check (inkPixels (bubbleImg) > 10,
               "help bubble paints its accent ring + glyph");

        // v0.7.3 PathRow: editor + reveal + browse all paint visible ink.
        ebs::PathRow pathRow (ebs::PathRow::Mode::File, "path placeholder");
        pathRow.setText ("C:/some/path.wav");
        const auto pathImg = renderToImage (300, 26, [&] (juce::Graphics& g)
            { pathRow.setBounds (0, 0, 300, 26); pathRow.paint (g);
              pathRow.resized(); });
        check (inkPixels (pathImg) > 40,
               "path row paints editor + buttons");

        // v0.8.0 DataList: proxy pipeline (views + search + sort) over
        // painted cells with a progress bar and an action column. The
        // action glyphs must stay pixel-identical to IconButton widgets.
        {
            // Shared-renderer parity: every action shape paints through
            // IconButton::glyphPath, so paint a widget and a DataList cell
            // side by side and require identical ink.
            const ebs::IconButton::Shape actionShapes[] =
            {
                ebs::IconButton::Shape::play, ebs::IconButton::Shape::stop,
                ebs::IconButton::Shape::star, ebs::IconButton::Shape::lock,
                ebs::IconButton::Shape::grip, ebs::IconButton::Shape::wand,
                ebs::IconButton::Shape::cross,
                ebs::IconButton::Shape::refresh
            };
            {
                ebs::DataList dl;
                dl.setShowSearch (false);
                dl.setShowViews (false);
                dl.setColumns ({ { 1, "Name", 200 } });
                std::vector<ebs::DataList::Action> acts;
                for (const auto s : actionShapes)
                    acts.push_back ({ (int) s + 100, s, "" });
                dl.setRowActions (acts);
                ebs::DataList::Row r;
                r.id = "glyphs";
                r.cells = { { 1, "" } };
                dl.setRows ({ r });
                dl.setSize (320, 60);
                dl.resized();
                // One ink check per slot: each glyph leaves visible paint.
                // Threshold: mean brightness must RISE above the panel
                // background (dim glyphs like the hand still count).
                const auto glyphImg = renderToImage (320, 60,
                    [&] (juce::Graphics& g)
                    { dl.paintEntireComponent (g, false); });
                const auto bgLum = [] (juce::Colour c)
                    { return 0.3f * c.getFloatRed()
                             + 0.6f * c.getFloatGreen()
                             + 0.1f * c.getFloatBlue(); };
                const float bg = bgLum (ebs::bgPanel());
                // Action column: 8 slots x 24 px from x=0; sample each.
                bool allGlyphInk = true;
                for (int i = 0; i < 8; ++i)
                {
                    float lum = 0.0f;
                    int n = 0;
                    for (int y = 34; y < 54; ++y)
                        for (int x = i * 24 + 4; x < i * 24 + 20; ++x)
                        {
                            lum += bgLum (glyphImg.getPixelAt (x, y));
                            ++n;
                        }
                    if (n == 0 || lum / (float) n < bg + 0.004f)
                        allGlyphInk = false;
                }
                check (allGlyphInk,
                       "datalist paints every action glyph (parity with IconButton)");
            }
            ebs::DataList dl;
            dl.setShowSearch (false);
            dl.setShowViews (false);
            dl.setColumns ({ { 1, "Name", 140 }, { 2, "Dur", 60 },
                             { 3, "Origin", 90 } });
            dl.setViews ({ { "All", {} },
                            { "Music", [] (const ebs::DataList::Row& r)
                               { auto it = r.cells.find (3);
                                 return it != r.cells.end()
                                     && it->second == "Music"; } } });
            ebs::DataList::Row r1, r2, r3;
            r1.id = "t-1"; r1.cells = { { 1, "alpha" }, { 2, "30" }, { 3, "Music" } };
            r2.id = "t-2"; r2.cells = { { 1, "beta" }, { 2, "8" }, { 3, "TTS" } };
            r2.progress = 0.5; r2.progressColumnId = 1;
            r3.id = "t-3"; r3.cells = { { 1, "gamma" }, { 2, "120" }, { 3, "Music" } };
            dl.setRows ({ r1, r2, r3 });
            check (dl.visibleRowCount() == 3, "datalist shows all rows by default");
            dl.selectView (1);
            check (dl.visibleRowCount() == 2
                       && dl.visibleRowId (0) == "t-1"
                       && dl.visibleRowId (1) == "t-3",
                   "datalist view filters rows by predicate");
            dl.selectView (0);
            dl.setSearchText ("beta");
            check (dl.visibleRowCount() == 1
                       && dl.visibleRowId (0) == "t-2",
                   "datalist search filters rows by text");
            dl.setSearchText ("");
            dl.sortBy (2, true);   // numeric-aware: 8 < 30 < 120
            check (dl.visibleRowCount() == 3
                       && dl.visibleRowId (0) == "t-2"
                       && dl.visibleRowId (1) == "t-1"
                       && dl.visibleRowId (2) == "t-3",
                   "datalist sorts numeric columns numerically");
            dl.sortBy (1, false);  // gamma > beta > alpha
            check (dl.visibleRowId (0) == "t-3"
                       && dl.visibleRowId (2) == "t-1",
                   "datalist sorts text columns backwards");
            dl.setSize (320, 120);
            dl.resized();
            const auto dlImg = renderToImage (320, 120, [&] (juce::Graphics& g)
                { dl.paintEntireComponent (g, false); });
            check (inkPixels (dlImg) > 200, "datalist paints rows + header");
        }

        // === v0.2.0 ColourIds ===============================================
        // 1) Per-instance override wins over everything.
        ebs::StatusBar redBar;
        redBar.logLine ("override");
        redBar.setColour (ebs::StatusBar::backgroundColourId,
                          juce::Colours::red);
        const auto redImg = renderToImage (300, 24, [&] (juce::Graphics& g)
            { redBar.setSize (300, 24); redBar.paint (g); });
        checkPixel (redImg, { 150, 12 }, juce::Colours::red,
                    "status bar honours a per-instance backgroundColourId");

        // 2) Theme-level hook: a subclassed L&F restyles WITHOUT touching
        //    instances. Swap the global default, render, restore.
        struct ThemedLnf : ebs::LookAndFeel
        {
            using ebs::LookAndFeel::LookAndFeel;
            juce::Colour widgetThemeColour (int colourId) override
            {
                if (colourId == ebs::StatusBar::backgroundColourId)
                    return juce::Colour (0xff003300);
                return {};
            }
        } themedLnf;
        {
            juce::LookAndFeel::setDefaultLookAndFeel (&themedLnf);
            ebs::StatusBar themedBar;
            themedBar.logLine ("themed");
            const auto tImg = renderToImage (300, 24, [&] (juce::Graphics& g)
                { themedBar.setSize (300, 24); themedBar.paint (g); });
            checkPixel (tImg, { 150, 12 }, juce::Colour (0xff003300),
                        "widgetThemeColour hook restyles at theme level");
            juce::LookAndFeel::setDefaultLookAndFeel (&lnf);   // restore
        }

        // 3) IconButton glyph: per-instance iconColourId.
        PaintableIconButton greenPlay (ebs::IconButton::Shape::play);
        greenPlay.setSize (28, 28);
        greenPlay.setColour (ebs::IconButton::iconColourId,
                             juce::Colours::green);
        const auto gpImg = renderToImage (28, 28, [&] (juce::Graphics& g)
            { greenPlay.paint (g); });
        // Play triangle spans the full mid-height scanline x=9..19.
        checkPixel (gpImg, { 12, 14 }, juce::Colours::green,
                    "icon button honours a per-instance iconColourId");

        // 4) Framed mode: frameFillColourId drives the chrome background.
        PaintableIconButton framedFill (ebs::IconButton::Shape::play);
        framedFill.setSize (28, 28);
        framedFill.setFramed (true);
        framedFill.setColour (ebs::IconButton::frameFillColourId,
                              juce::Colour (0xff402060));
        const auto ffImg = renderToImage (28, 28, [&] (juce::Graphics& g)
            { framedFill.paint (g); });
        checkPixel (ffImg, { 6, 14 }, juce::Colour (0xff402060),
                    "framed icon button honours frameFillColourId");

        // === v0.3 widgets & palette =========================================
        // 1) New always-dark viz-chrome palette tokens (hex-frozen contract).
        check (ebs::grid()        == juce::Colour (0x20ffffff), "viz-chrome grid hex");
        check (ebs::scaleLine()   == juce::Colour (0x10ffffff), "viz-chrome scale-line hex");
        check (ebs::rulerBg()     == juce::Colour (0xff1a1a1a), "viz-chrome ruler background hex");
        check (ebs::curveGrid()   == juce::Colour (0x40ffffff), "viz-chrome curve-grid hex");
        check (ebs::vizHeaderBg() == juce::Colour (0xff191b1e), "viz-chrome header background hex");
        check (ebs::vizLegendBg() == juce::Colour (0xff191b1e), "viz-chrome legend background hex");
        check (ebs::cpuBg()       == juce::Colour (0xff222230), "viz-chrome cpu well hex");

        // 2) ebs::Knob routes through the shared LookAndFeel painter.
        ebs::Knob knob;
        knob.setSize (48, 48);
        knob.setRange (0.0, 1.0);
        knob.setRotaryParameters ({ juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true });
        knob.setValue (1.0, juce::dontSendNotification);
        const auto knobImg = renderToImage (48, 48, [&] (juce::Graphics& g)
            { knob.paint (g); });
        check (inkPixels (knobImg) > 300, "knob paints substantial ink");
        {
            // Point ON the value-arc stroke centreline at the end angle.
            // The EiffelBS/OVT arc convention is CLOCKWISE FROM 12 OCLOCK
            // (matches the rotated line pointer built from (0,-r)):
            // x = cx + r*sin(a), y = cy - r*cos(a). The opaque accent arc
            // covers the already-painted track band -> exact accent.
            const float endA = juce::MathConstants<float>::pi * 2.75f;
            const int px = (int) std::lround (24.0f + 20.0f * std::sin (endA));
            const int py = (int) std::lround (24.0f - 20.0f * std::cos (endA));
            checkPixel (knobImg, { px, py }, ebs::accent(),
                        "knob value arc reaches the end-angle centreline");
        }
        // Pivot-centred mode still yields a substantial accent arc at
        // minimum value (short arc towards the range midpoint).
        knob.setCentredFill (true);
        knob.setValue (0.0, juce::dontSendNotification);
        {
            constexpr auto accentish = [] (juce::Colour c)
            {
                const auto a = ebs::accent();
                return c.getAlpha() == 255
                    && std::abs (c.getRed()   - a.getRed())   <= 12
                    && std::abs (c.getGreen() - a.getGreen()) <= 12
                    && std::abs (c.getBlue()  - a.getBlue())  <= 12;
            };
            int accentHits = 0;
            const auto centredImg = renderToImage (48, 48, [&] (juce::Graphics& g)
                { knob.paint (g); });
            for (int y = 0; y < 48; ++y)
                for (int x = 0; x < 48; ++x)
                    if (accentish (centredImg.getPixelAt (x, y)))
                        ++accentHits;
            check (accentHits >= 30, "centred-fill knob draws its pivot arc at minimum");
        }

        // 3) ebs::MorphSlider: pivot-centred track, exact-composited fill.
        ebs::MorphSlider morph;
        morph.setSize (120, 16);
        morph.setRange (0.0, 1.0);
        morph.setValue (0.25, juce::dontSendNotification);
        const auto morphImg = renderToImage (120, 16, [&] (juce::Graphics& g)
            { morph.paint (g); });
        {
            // Geometry: thumbWidth 12 -> trackLeft 6, span 108; norm .25 ->
            // thumb centre x = 33; pivot centre x = 60; fill = [33..60].
            const auto bed = ebs::bgPanel().brighter (0.15f);
            checkPixel (morphImg, { 12, 8 }, bed,
                        "morph track bed outside the fill zone");
            // Exact sOVER composition: translucent accent fill over bed.
            const auto fg  = ebs::accent().withAlpha (0.7f);
            const uint8_t fa = fg.getAlpha();
            const auto over = [&] (const juce::Colour& bg)
            {
                return juce::Colour::fromRGBA (
                    (uint8_t) ((fa * fg.getRed()   + (255 - fa) * bg.getRed())   / 255),
                    (uint8_t) ((fa * fg.getGreen() + (255 - fa) * bg.getGreen()) / 255),
                    (uint8_t) ((fa * fg.getBlue()  + (255 - fa) * bg.getBlue())  / 255),
                    255);
            };
            checkPixel (morphImg, { 45, 8 }, over (bed),
                        "morph pivot-centred fill composes accent over bed");
            checkPixel (morphImg, { 33, 8 }, ebs::accent(),
                        "morph thumb rides the proportional position");
        }

        // 4) ebs::PowerToggle: engaged glow gold / idle grey / dimmed when off.
        constexpr auto countGold = [] (const juce::Image& img)
        {
            int n = 0;
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                {
                    const auto c = img.getPixelAt (x, y);
                    if (c.getAlpha() > 60 && c.getRed() > 190
                        && c.getGreen() > 170 && c.getBlue() < 130)
                        ++n;
                }
            return n;
        };
        ebs::PowerToggle powerOn;
        powerOn.setSize (28, 28);
        powerOn.setToggleState (true, juce::dontSendNotification);
        const auto pOnImg = renderToImage (28, 28, [&] (juce::Graphics& g)
            { powerOn.paint (g); });
        check (countGold (pOnImg) >= 8, "power toggle ON glows warm gold");

        ebs::PowerToggle powerOff;
        powerOff.setSize (28, 28);
        const auto pOffImg = renderToImage (28, 28, [&] (juce::Graphics& g)
            { powerOff.paint (g); });
        check (countGold (pOffImg) == 0 && inkPixels (pOffImg) > 10,
               "power toggle OFF shows grey glyph without gold");

        powerOn.setEnabled (false);
        const auto pDisImg = renderToImage (28, 28, [&] (juce::Graphics& g)
            { powerOn.paint (g); });
        check (countGold (pDisImg) == 0, "disabled power toggle drops the glow");

        // 5) Chrome hook: theme-level restyle of the checkbox well, built-ins
        //    untouched otherwise (the earlier toggle interior check above ran
        //    under the plain L&F and asserts the vizBg built-in).
        struct ChromeLnf : ebs::LookAndFeel
        {
            using ebs::LookAndFeel::LookAndFeel;
            juce::Colour widgetThemeColour (int colourId) override
            {
                if (colourId == ebs::LookAndFeel::checkboxFillColourId)
                    return juce::Colours::red;
                return {};
            }
        } chromeLnf;
        {
            juce::LookAndFeel::setDefaultLookAndFeel (&chromeLnf);
            tog.repaint();
            const auto chkImg = renderToImage (260, 60, [&] (juce::Graphics& g)
                { tog.setBounds (0, 0, 220, 24); tog.paint (g); });
            checkPixel (chkImg, { 12, 6 }, juce::Colours::red,
                        "chrome hook restyles the checkbox well app-wide");
            juce::LookAndFeel::setDefaultLookAndFeel (&lnf);   // restore
        }

        // === Theme broadcast (v0.4) ==========================================
        {
            struct Probe : ebs::ThemeSubscriber
            {
                int hits = 0;
                void themeChanged() override { ++hits; }
            };
            Probe probe;
            ebs::subscribeTheme (&probe);
            ebs::subscribeTheme (&probe);   // idempotent registration

            const auto popupBgBefore = lnf.findColour (
                juce::PopupMenu::backgroundColourId);
            check (probe.hits == 0, "subscriber silent before any switch");

            ebs::setTheme (ebs::Theme::Light);
            check (probe.hits == 1, "setTheme notifies a subscriber exactly once");
            check (! ebs::isDark(), "setTheme flips the shared palette state");
            check (lnf.findColour (juce::PopupMenu::backgroundColourId)
                       != popupBgBefore,
                   "LookAndFeel auto-refreshes on setTheme (no manual call)");

            ebs::unsubscribeTheme (&probe);
            ebs::setTheme (ebs::Theme::Dark);
            check (probe.hits == 1 && ebs::isDark(),
                   "unsubscribed listener stops receiving; palette restored");
        }

        // === Composed PNG receipt ===========================================
        // CANONICAL offscreen rendering: assemble a REAL component hierarchy
        // under a detached parent, then let paintEntireComponent do the
        // parenting (clip + per-child translation) exactly like on screen.
        juce::Component canvas;
        canvas.setSize (260, 210);

        canvas.addAndMakeVisible (primary);
        primary.setBounds (20, 18, 70, 28);
        canvas.addAndMakeVisible (plain);
        plain.setBounds (98, 18, 70, 28);
        canvas.addAndMakeVisible (tog);
        tog.setBounds (176, 20, 68, 24);

        const ebs::IconButton::Shape row[] =
        {
            ebs::IconButton::Shape::cross,   ebs::IconButton::Shape::folder,
            ebs::IconButton::Shape::grip,    ebs::IconButton::Shape::undo,
            ebs::IconButton::Shape::settings, ebs::IconButton::Shape::star
        };
        juce::OwnedArray<PaintableIconButton> icons;
        int x = 24;
        for (const auto s : row)
        {
            auto* ib = new PaintableIconButton (s);
            icons.add (ib);
            canvas.addAndMakeVisible (*ib);
            ib->setBounds (x, 58, 28, 28);
            x += 36;
        }

        // v0.3 row: morph slider + power toggles + knob (fresh look state).
        powerOn.setEnabled (true);
        knob.setCentredFill (false);
        knob.setValue (0.65, juce::dontSendNotification);

        canvas.addAndMakeVisible (morph);
        morph.setBounds (20, 96, 220, 16);

        canvas.addAndMakeVisible (powerOn);
        powerOn.setBounds (24, 118, 28, 28);
        canvas.addAndMakeVisible (powerOff);
        powerOff.setBounds (60, 118, 28, 28);
        canvas.addAndMakeVisible (knob);
        knob.setBounds (100, 112, 40, 40);

        canvas.addAndMakeVisible (bar);
        bar.setSize (220, 24);
        bar.setTopLeftPosition (20, 170);

        // v0.7.1: help bubble on the canvas (top-right of the button row).
        canvas.addAndMakeVisible (bubble);
        bubble.setBounds (236, 20, 14, 14);

        juce::Image receipt (juce::Image::ARGB, 260, 210, true);
        {
            juce::Graphics g (receipt);
            g.fillAll (ebs::bgDark());
            canvas.paintEntireComponent (g, false);
        }
        writePngReceipt (receipt);

        // Detach children BEFORE stack unwind order gets tricky: canvas is
        // destroyed first (declared last among the parents here), while its
        // children are older stack objects.
        canvas.removeAllChildren();

        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    std::printf ("\neiffelbs-ui smoke: %s (%d failure%s)\n",
                 failures == 0 ? "PASS" : "FAIL",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
