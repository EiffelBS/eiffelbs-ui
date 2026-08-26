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

        // ToggleButton (checked): checkbox interior uses the visualizer bg.
        PaintableToggleButton tog ("Option");
        tog.setToggleState (true, juce::dontSendNotification);
        const auto togImg = renderToImage (260, 60, [&] (juce::Graphics& g)
            { tog.setBounds (0, 0, 220, 24); tog.paint (g); });
        // Checkbox rect = local (0,4,16,16): sample the top-right INTERIOR,
        // clear of both the 1 px border and the check-mark stroke band.
        checkPixel (togImg, { 12, 6 }, ebs::vizBg(),
                    "toggle checkbox interior uses the visualizer background");

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
        ebs::StatusBar bar;
        bar.logLine ("smoke test line");
        bar.logLine ("Generating... 42 s elapsed");
        const auto barImg = renderToImage (300, 24, [&] (juce::Graphics& g)
            { bar.setSize (300, 24); bar.paint (g); });
        check (barImg.getPixelAt (150, 12) == ebs::bgPanel(),
               "status bar paints the panel surface");

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

        // === Composed PNG receipt ===========================================
        // CANONICAL offscreen rendering: assemble a REAL component hierarchy
        // under a detached parent, then let paintEntireComponent do the
        // parenting (clip + per-child translation) exactly like on screen.
        juce::Component canvas;
        canvas.setSize (260, 160);

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

        canvas.addAndMakeVisible (bar);
        bar.setSize (220, 24);
        bar.setTopLeftPosition (20, 116);

        juce::Image receipt (juce::Image::ARGB, 260, 160, true);
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
