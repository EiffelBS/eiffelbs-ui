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

#if defined(_WIN32) && ! defined(NOMINMAX)
// Consumer contexts do NOT all define NOMINMAX: the ARA SDK and the VST3
// plugin-client chain pull <windows.h> in bare, which turns min/max into
// function-like macros. Headers that call std::max or
// std::numeric_limits<int>::max() unparenthesised then fail to compile
// (C4003 / C2589) in exactly those consumers - OpenVoxTuner's tree hit all
// four sites before v0.12.7. Include the polluted context FIRST, before
// anything else, so this TU is the strictest one in the repo.
#include <windows.h>
#endif

#include <juce_gui_basics/juce_gui_basics.h>
// The whole juce_gui_extra module is pulled in BEFORE eiffelbs on purpose:
// ebs::StatusBar.h reaches juce::CodeEditorComponent / juce::CodeDocument
// through juce_gui_extra.h, and those two headers carry NO include guard of
// their own. Without this line the smoke TU would compile while every real
// consumer - which pulls the module in from elsewhere - would die on a
// class redefinition.
#include <juce_gui_extra/juce_gui_extra.h>
#include <eiffelbs/eiffelbs.h>

#include <cmath>
#include <cstdio>
#include <optional>

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

    /** Render into a CLEARED image: alpha marks what actually painted and
     *  nothing else. The opaque variant above is fine for a colour comparison
     *  but useless for "did the glyph paint", because its black fill is
     *  opaque and every pixel counts as ink. */
    template <typename Fn>
    juce::Image renderOnClear (int w, int h, Fn&& draw)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
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

#include <initializer_list>

// A string spelled as raw UTF-8 BYTES. MSVC reads this source in the system code page and writes
// literals in UTF-8, so every accented literal gets double-encoded on the way in: a French e-acute
// reaches the component as TWO Latin-1 glyphs. Strings whose codepoints must match exactly are spelled
// byte by byte here, where no compiler charset can touch them.
static juce::String u8 (std::initializer_list<int> bytes)
{
    char buf[64]; int n = 0;
    for (int b : bytes) buf[n++] = static_cast<char> (b);
    buf[n] = 0;
    return juce::String (juce::CharPointer_UTF8 (buf));
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
        // A MENU BUTTON carries its label and its chevron - and, when the host
        // asks, a GLYPH. The three are one control, so the check is a
        // comparison: the same label with an icon leaves MORE ink (the glyph is
        // drawn) and asks for MORE width (the slot is measured), while the
        // label-only button keeps painting its label. A glyph that never
        // reached the drawing would pass a "the button paints" check and fail
        // this one.
        {
            ebs::MenuChevronButton plain, iconed;
            plain.setButtonText ("Words: Outline + scrim");
            iconed.setButtonText ("Words: Outline + scrim");
            iconed.setShape (ebs::IconButton::Shape::film);
            plain.setSize (plain.widthForLabel(), 32);
            iconed.setSize (iconed.widthForLabel(), 32);
            check (iconed.getWidth() > plain.getWidth(),
                   "a menu button with a glyph asks for the glyph's slot");
            const auto renderMenu = [] (ebs::MenuChevronButton& b)
            {
                juce::Image img (juce::Image::ARGB, b.getWidth(), 32, true);
                juce::Graphics g (img);
                g.fillAll (juce::Colours::black);
                b.paintEntireComponent (g, true);
                return img;
            };
            const auto plainInk = inkPixels (renderMenu (plain));
            const auto iconedInk = inkPixels (renderMenu (iconed));
            std::printf ("[INFO] menu button ink: label %d, label+glyph %d\n",
                         plainInk, iconedInk);
            check (plainInk > 0, "a menu button with no glyph still paints its label");
            check (iconedInk > plainInk + 20,
                   "the menu glyph reaches the drawing");
        }

        // TRAILING TEXT: the model menus show two facts of different weight
        // ("model ... effort"). The second one is dim, right-aligned and ALWAYS
        // drawn (the model name may ellipsize; the effort must not), and it is
        // measured into the button's own width - a caller that lays the button
        // out with widthNeededFor(text) would cut it otherwise.
        {
            ebs::MenuChevronButton plain, trailed;
            plain.setButtonText ("Qwen3-8B-Instruct");
            trailed.setButtonText ("Qwen3-8B-Instruct");
            trailed.setTrailingText ("High");
            check (trailed.widthForLabel()
                       > plain.widthForLabel(),
                   "a menu button with trailing text asks for its width");
            const auto renderTrail = [] (ebs::MenuChevronButton& b)
            {
                b.setSize (b.widthForLabel(), 32);
                juce::Image img (juce::Image::ARGB, b.getWidth(), 32, true);
                juce::Graphics g (img);
                g.fillAll (juce::Colours::black);
                b.paintEntireComponent (g, true);
                return img;
            };
            const auto plainInk2 = inkPixels (renderTrail (plain));
            const auto trailedInk = inkPixels (renderTrail (trailed));
            std::printf ("[INFO] menu trailing ink: label %d, label+trail %d\n",
                         plainInk2, trailedInk);
            check (trailedInk > plainInk2 + 10,
                   "the trailing text reaches the drawing");
            check (trailed.getTrailingText() == juce::String ("High"),
                   "the trailing text is readable back");
            // NOT GLUED (author report 2026-10-26: "le chevron est directement
            // collé au texte"). The trail slot ends BEFORE the chevron's measured
            // gap, so the columns between them stay empty. Diffing the trails and
            // no-trails render at a FIXED width isolates the text pixels (the V
            // is painted identically in both, so it cancels): the gap band must
            // carry zero trail ink while the trail itself stays inside its slot.
            const auto renderAt = [] (ebs::MenuChevronButton& b, int w)
            {
                b.setSize (w, 32);
                juce::Image img (juce::Image::ARGB, w, 32, true);
                juce::Graphics g (img);
                g.fillAll (juce::Colours::black);
                b.paintEntireComponent (g, true);
                return img;
            };
            const int trailW = trailed.widthForLabel();
            const auto withTrail = renderAt (trailed, trailW);
            trailed.setTrailingText ("");
            const auto noTrailImg = renderAt (trailed, trailW);
            trailed.setTrailingText ("High");
            int gapDiff = 0, slotDiff = 0;
            for (int x = 0; x < trailW; ++x)
                for (int y = 0; y < 32; ++y)
                    if (withTrail.getPixelAt (x, y).getARGB()
                            != noTrailImg.getPixelAt (x, y).getARGB())
                        (x >= trailW - 25 && x <= trailW - 18 ? gapDiff : slotDiff) += 1;
            check (slotDiff > 40, "the trailing text paints inside its measured slot");
            check (gapDiff == 0, "a real gap separates the trail from the chevron");
        }

        // TOKEN FIELD: a chip carries a drag description for text editors.
        {
            ebs::TokenField field;
            field.setAllowedTokens ({ { "music.model", "engine" } });
            field.setSize (300, 34);
            field.setTokens ({ "music.model" });
            field.resized();
            check (field.chipDragDescriptionForTest (0)
                       == ebs::TokenField::dragDescription ("music.model"),
                   "a token chip names its own drag payload");
            // Hand over the chip body, pointing hand over its cross.
            const auto chip = juce::Rectangle<int> (4, 0, 100, 24);
            check (field.chipCursorKindForTest (chip.getCentre()) == 1,
                   "a chip body shows the DRAG hand");
            check (field.chipCursorKindForTest ({ 3, 3 }) == 0,
                   "empty field space is not a drag handle");
        }

        // THE MODEL FACTS: a searchable picker and the menu button that shows
        // "model ... effort" must both paint and measure. The menu button's
        // width follows BOTH facts (a layout asking only for the model cuts the
        // effort - the 2026-10-25 dialog bug), and its width can be capped.
        {
            ebs::SearchableModelPicker picker;
            picker.setItems ({ "org/a", "org/b" }, { "Alpha 3B", "Beta 7B" },
                             { "tts", "stt" });
            picker.setSelectedId ("org/a");
            check (picker.displayText() == juce::String ("Alpha 3B"),
                   "the shared picker paints the selected label");
            picker.setSize (240, 26);
            juce::Image pickerImg (juce::Image::ARGB, 240, 26, true);
            {
                juce::Graphics pg (pickerImg);
                picker.paintEntireComponent (pg, true);
            }
            std::printf ("[INFO] shared picker ink: %d\n",
                         inkPixels (pickerImg));
            check (inkPixels (pickerImg) > 200,
                   "the shared picker leaves ink on screen");
            check (picker.itemCount() == 2 && picker.selectedIndex() == 0,
                   "the shared picker keeps the combo-compatible contract");

            ebs::ModelMenuButton button;
            button.setModels ({ "org/a", "org/b" }, { "Alpha 3B", "Beta 7B" });
            button.setSelectedModelId ("org/a");
            button.setEfforts ({ "off", "high" }, { "Off", "High" });
            button.setSelectedEffortId ("high");
            check (button.getSelectedModelId() == juce::String ("org/a")
                       && button.getEffortText() == juce::String ("High"),
                   "the menu button shows model AND effort");
            const int withEffort = button.widthForContent();
            button.setEffortText ("");
            const int withoutEffort = button.widthForContent();
            check (withEffort > withoutEffort,
                   "the menu button asks for the trailing text's width");
            button.setEffortText ("High");
            button.setMaxWidth (120);
            check (button.widthForContent() <= 120,
                   "the menu button honours a host cap");
            button.setSize (button.widthForContent(), button.heightForContent());
            juce::Image buttonImg (juce::Image::ARGB, button.getWidth(),
                                   button.getHeight(), true);
            {
                juce::Graphics bg (buttonImg);
                button.paintEntireComponent (bg, true);
            }
            std::printf ("[INFO] model menu button: %dx%d ink=%d\n",
                         button.getWidth(), button.getHeight(),
                         inkPixels (buttonImg));
            check (inkPixels (buttonImg) > 200,
                   "the menu button leaves ink on screen");
            check (button.menuItemTextForTest (0).startsWith ("Model: ")
                       && button.menuItemTextForTest (1) == juce::String ("Effort: High"),
                   "the menu names the two facts the face shows");
            button.pickerOpenForTest();
            button.pickerSearchForTest ("Beta");
            check (button.pickerVisibleCountForTest() == 1,
                   "the menu button's model step reuses the shared picker's filter");
            // GROUPING + SCROLL (author report 2026-10-26 - "la liste n'est plus
            // scrollable bien qu'il y ait plus de 90 modèles possibles", and the
            // provider grouping had become a second per-row column): providers are
            // header rows above their runs, typing one narrows to its models, and
            // a long catalog is a real scrolling view. One offscreen render proves
            // the headers reach ink; the seams prove the geometry.
            ebs::SearchableModelPicker grouped;
            juce::StringArray gIds, gLabels, gGroups;
            for (int p = 0; p < 4; ++p)
                for (int m = 0; m < 25; ++m)
                {
                    gIds.add ("srv" + juce::String (p) + "|m" + juce::String (m));
                    gLabels.add ("Model " + juce::String (p * 25 + m));
                    gGroups.add ("Provider " + juce::String (p));   // contiguous runs
                }
            grouped.setItems (gIds, gLabels, {}, gGroups);
            check (grouped.visibleIdCountForTest() == 100
                       && grouped.displayRowCountForTest() == 104
                       && grouped.headerCountForTest() == 4,
                   "providers become header rows above their runs");
            check (grouped.rowIsHeaderForTest (0)
                       && grouped.rowHeaderTextForTest (0).contains ("Provider"),
                   "the first display row is a provider header");
            grouped.setSearchForTest ("provider 2");
            check (grouped.visibleIdCountForTest() == 25
                       && grouped.displayRowCountForTest() == 26,
                   "typing a provider narrows to its models plus its header");
            grouped.setSearchForTest ("");
            check (grouped.viewRowsForTest() == 10
                       && grouped.scrollRangeForTest() >= 94,
                   "a 100-model catalog is a real scrolling view");
            const auto groupImg = renderToImage (320, 26 + 4 + 10 * 24 + 18 + 4,
                [&grouped] (juce::Graphics& g) { grouped.paintPopupForTest (g, 320); });
            check (inkPixels (groupImg) > 250,
                   "the grouped popup paints headers and model rows");
            // THE SCROLLBAR THE AUTHOR COULD NOT SEE + THE WHEEL SPEED. The
            // headless popup gets a real resized(); the bar must be visible AND
            // carry a thumb as long as the window, and ONE Windows notch (deltaY
            // = 3) must move whole lines - about ten - in one step.
            grouped.openHeadlessPopupForTest();
            check (grouped.barVisibleForTest(),
                   "the popup shows its scrollbar when content scrolls");
            check (grouped.barThumbLengthForTest() == 10 && grouped.barUpperForTest() == 104,
                   "the thumb is as long as the window, over the whole catalog");
            grouped.wheelScrollForTest (-3.0);   // one notch DOWN
            check (grouped.scrollOffsetForTest() == 10,
                   "one notch moves ten lines in whole-line steps");
            for (int n = 0; n < 9; ++n)
                grouped.wheelScrollForTest (-3.0);
            check (grouped.scrollOffsetForTest() == 94,
                   "the wheel reaches the last provider run");
            for (int n = 0; n < 20; ++n)
                grouped.wheelScrollForTest (3.0);
            check (grouped.scrollOffsetForTest() == 0,
                   "the wheel walks back home symmetrically");
            grouped.closeHeadlessPopupForTest();
            ebs::SearchableModelPicker flat;
            juce::StringArray fIds, fLabels;
            for (int i = 0; i < 8; ++i)
            {
                fIds.add ("f" + juce::String (i));
                fLabels.add ("Model " + juce::String (i));
            }
            flat.setItems (fIds, fLabels);
            flat.openHeadlessPopupForTest();
            check (! flat.barVisibleForTest() && flat.scrollOffsetForTest() == 0,
                   "an unscrollable list shows no bar at all");
            flat.closeHeadlessPopupForTest();

            // THE SUBMENU CHEVRONS (author report: "Il y avait avant un chevron
            // vers la droite pour aller au sous-menu"): each menu row draws its
            // own right-facing ">" - ink in the strip near the right edge, which
            // a plain PopupMenu row cannot produce.
            const int rowsW = 210;
            const auto rowsImg = renderToImage (rowsW, 4 * 2 + 32 * 2,
                [&button, rowsW] (juce::Graphics& g) { button.paintMenuRowsForTest (g, rowsW); });
            int chevInk = 0;
            for (int x = rowsW - 21; x <= rowsW - 8; ++x)   // cx = (right-4)-14 zone
                for (int y = 6; y < rowsImg.getHeight(); ++y)
                    if (rowsImg.getPixelAt (x, y).getAlpha() > 50)
                        ++chevInk;
            check (inkPixels (rowsImg) > 120 && chevInk > 12,
                   "the menu rows show the chevron that opens the next layer");
        }

        // SPLITTER HANDLE: drag, clamp, and a visible grip in both directions.
        {
            ebs::SplitterHandle rows (ebs::SplitterHandle::Orientation::rows);
            rows.setPositionRange (60, 400);
            rows.setPositionSilently (200);
            int reported = -1;
            rows.onPositionChanged = [&reported] (int p) { reported = p; };
            rows.dragByForTest (50);
            check (reported == 250, "a splitter reports the dragged position");
            rows.dragByForTest (10000);
            check (rows.getPositionValue() == 400,
                   "a splitter clamps to the host's maximum");
            rows.dragByForTest (-10000);
            check (rows.getPositionValue() == 60,
                   "a splitter clamps to the host's minimum");
            rows.setSize (200, 12);
            juce::Image img;
            {
                img = juce::Image (juce::Image::ARGB, rows.getWidth(),
                                   rows.getHeight(), true);
                juce::Graphics g (img);
                rows.paintEntireComponent (g, true);
            }
            check (inkPixels (img) > 0, "a splitter paints its grip");
            ebs::SplitterHandle cols (ebs::SplitterHandle::Orientation::columns);
            cols.setPositionRange (100, 500);
            cols.setPositionSilently (300);
            cols.dragByForTest (25);
            check (cols.getPositionValue() == 325,
                   "a columns splitter drags horizontally");
        }

        

        

        

        // ONE pill language: IconTextButton and PillButton paint the same
        // chrome, and a destructive glyph turns RED under the hover.
        {
            const auto pixelsWhere = [] (const juce::Image& img,
                                         juce::Colour target)
            {
                int n = 0;
                for (int y = 0; y < img.getHeight(); ++y)
                    for (int x = 0; x < img.getWidth(); ++x)
                    {
                        const auto p = img.getPixelAt (x, y);
                        if (p.getAlpha() > 60
                            && std::abs ((int) p.getRed() - (int) target.getRed()) < 26
                            && std::abs ((int) p.getGreen() - (int) target.getGreen()) < 26
                            && std::abs ((int) p.getBlue() - (int) target.getBlue()) < 26)
                            ++n;
                    }
                return n;
            };
            const auto renderIconText = [] (bool over)
            {
                ebs::IconTextButton b (ebs::IconButton::Shape::trash, "Remove");
                b.setFont (ebs::fontHint());
                b.setSize (b.widthForLabel(), 24);
                juce::Image img;
                {
                    img = juce::Image (juce::Image::ARGB, b.getWidth(),
                                       b.getHeight(), true);
                    juce::Graphics g (img);
                    b.paintForTest (g, over, false);
                }
                return img;
            };
            const auto rest = renderIconText (false);
            const auto hover = renderIconText (true);
            check (pixelsWhere (rest, ebs::panelBorder()) > 8,
                   "an icon-text button always draws its pill border (uniform look)");
            check (pixelsWhere (hover, ebs::danger()) > 20
                       && pixelsWhere (rest, ebs::danger()) == 0,
                   "a destructive pill turns RED on hover");
            ebs::PillButton same (ebs::IconButton::Shape::trash, "Remove");
            same.setSize (same.widthForLabel(), 24);
            juce::Image sameImg;
            {
                sameImg = juce::Image (juce::Image::ARGB, same.getWidth(),
                                       same.getHeight(), true);
                juce::Graphics g (sameImg);
                same.paintForTest (g, true, false);
            }
            check (pixelsWhere (sameImg, ebs::danger()) > 20,
                   "PillButton follows the same destructive hover rule");
        }

        // CLEARABLE EDITOR: the clear x appears with text, the eye masks.
        {
            ebs::ClearableEditor editor;
            editor.setSize (240, 24);
            int cleared = 0;
            editor.onCleared = [&cleared] { ++cleared; };
            int toggles = 0;
            editor.onRevealToggled = [&toggles] { ++toggles; };
            // The BASE TextEditor chrome must survive our override: a border
            // colour and a placeholder, both painted by the base.
            editor.setColour (juce::TextEditor::outlineColourId, ebs::panelBorder());
            editor.setColour (juce::TextEditor::backgroundColourId, ebs::bgDark());
            editor.setColour (juce::TextEditor::textColourId, ebs::text());
            editor.setTextToShowWhenEmpty ("Filter models...", ebs::textDim());
            {
                juce::Image chrome (juce::Image::ARGB, 240, 24, true);
                {
                    juce::Graphics g (chrome);
                    editor.paintEntireComponent (g, true);
                }
                int borderPixels = 0;
                for (int y = 0; y < chrome.getHeight(); ++y)
                    for (int x = 0; x < chrome.getWidth(); ++x)
                        if (chrome.getPixelAt (x, y).getAlpha() > 40)
                            if (x == 0 || y == 0
                                || x == chrome.getWidth() - 1
                                || y == chrome.getHeight() - 1)
                                ++borderPixels;
                check (borderPixels > 100,
                       "a clearable editor keeps its TextEditor border + "
                       "placeholder (base paintOverChildren)");
            }
            // PLACEHOLDER + CENTRING (author report 2026-10-26): the empty
            // state must SHOW its hint, and text must sit on the vertical
            // middle, not at the bottom (setIndents' 2nd arg is the TOP inset).
            {
                juce::Image empty (juce::Image::ARGB, 240, 26, true);
                {
                    juce::Graphics g (empty);
                    editor.paintEntireComponent (g, true);
                }
                int hintInk = 0;
                for (int y = 4; y < 22; ++y)
                    for (int x = 6; x < 160; ++x)
                        if (empty.getPixelAt (x, y).getAlpha() > 60)
                            ++hintInk;
                check (hintInk > 30,
                       "an empty clearable editor SHOWS its placeholder");
                const auto f = juce::File::getSpecialLocation (
                                   juce::File::tempDirectory)
                                   .getChildFile ("clearable-final.png");
                if (auto stream = f.createOutputStream())
                    juce::PNGImageFormat().writeImageToStream (empty, *stream);
                std::printf ("[INFO] wrote %s\n",
                             f.getFullPathName().toRawUTF8());
            }
            editor.setText ("openrouter", juce::dontSendNotification);
            {
                juce::Image withText (juce::Image::ARGB, 240, 26, true);
                {
                    juce::Graphics g (withText);
                    editor.paintEntireComponent (g, true);
                }
                int topInk = 0, middleInk = 0;
                for (int y = 4; y < 11; ++y)
                    for (int x = 4; x < 160; ++x)
                        if (withText.getPixelAt (x, y).getAlpha() > 60)
                            ++topInk;
                for (int y = 11; y < 21; ++y)
                    for (int x = 4; x < 160; ++x)
                        if (withText.getPixelAt (x, y).getAlpha() > 60)
                            ++middleInk;
                // "openrouter" has a descender (p), so the BOTTOM rows legitimately
                // carry a few pixels; what must NOT happen is an empty TOP band,
                // which is what the old top-indent bug produced.
                check (topInk > 10 && middleInk > 20,
                       "clearable text sits on the middle, not glued to the "
                       "bottom");
            }

            editor.clickClearForTest();
            check (editor.getText().isEmpty() && cleared == 1,
                   "a clearable editor empties itself in one gesture");
            editor.setSecretMode (true);
            editor.setText ("sk-secret", juce::dontSendNotification);
            check (editor.maskedForTest(),
                   "a secret field is masked by default");
            editor.clickRevealForTest();
            check (! editor.maskedForTest() && toggles == 1,
                   "the eye reveals the secret");
            editor.clickRevealForTest();
            check (editor.maskedForTest(), "the eye hides it again");
            juce::Image img;
            {
                img = juce::Image (juce::Image::ARGB, editor.getWidth(),
                                   editor.getHeight(), true);
                juce::Graphics g (img);
                editor.paintEntireComponent (g, true);
            }
            check (inkPixels (img) > 0, "a clearable editor paints");
        }

        // VARIABLE EDITOR: the {{ list filters, inserts, and stays inside.
        {
            ebs::VariableTextEditor editor;
            editor.setSize (320, 80);
            editor.setVariables ({ { "music.description", "the box" },
                                   { "music.duration", "seconds" },
                                   { "music.model", "engine" },
                                   { "assistant.datetime", "now" } });
            editor.setText ("A {{mus", juce::dontSendNotification);
            editor.setCaretPosition (7);   // after the whole typed prefix
            editor.openSuggestionsForTest();
            check (editor.suggestionsVisibleForTest()
                       && editor.suggestionsCountForTest() == 3,
                   "variable editor: the prefix filters the list");
            editor.chooseSuggestionForTest (1);   // music.duration
            check (editor.getText() == "A {{music.duration}}",
                   "variable editor: picking inserts the placeholder");
            editor.setText ("A {{zzz", juce::dontSendNotification);
            editor.setCaretPosition (7);
            editor.openSuggestionsForTest();
            check (! editor.suggestionsVisibleForTest(),
                   "variable editor: no match closes the list");
            // DRAG & DROP (phase B): a chip dropped into the editor inserts
            // its placeholder at the caret; a foreign payload is ignored.
            editor.setText ("Sound for ", juce::dontSendNotification);
            editor.setCaretPosition (10);
            editor.dropVariableForTest ("music.model");
            check (editor.getText() == "Sound for {{music.model}}",
                   "variable editor: a dropped chip inserts its placeholder");
            // The DROP POINT drives the caret: dropping on the FIRST line
            // lands there, not wherever the caret happened to be.
            editor.setMultiLine (true, true);
            editor.setText ("first line\nsecond line", juce::dontSendNotification);
            editor.dropVariableAtForTest ("music.duration", { 2, 4 });
            check (editor.getText().startsWith ("{{music.duration}}first"),
                   "variable editor: the caret follows the drop point");
            check (editor.isInterestedInDragSource (
                       juce::DragAndDropTarget::SourceDetails (
                           ebs::TokenField::dragDescription ("music.model"),
                           nullptr, {}))
                       && ! editor.isInterestedInDragSource (
                           juce::DragAndDropTarget::SourceDetails (
                               "some.other.drag", nullptr, {})),
                   "variable editor: only ebs.variable. payloads are accepted");
            // VISIBLE drop feedback (author report): during a drag the real
            // caret is hidden (focus lives on the chip), so the editor paints
            // its own caret + accent border; a still image proves the pixels.
            editor.setMultiLine (true, true);
            editor.setText ("first line\nsecond line", juce::dontSendNotification);
            editor.setDragPreviewForTest (true, { 6, 4 });
            {
                juce::Image dropImg (juce::Image::ARGB, 320, 80, true);
                {
                    juce::Graphics g (dropImg);
                    g.fillAll (juce::Colours::black);
                    editor.paintEntireComponent (g, true);
                }
                int accentPixels = 0;
                const auto accentColour = ebs::accent();
                for (int y = 0; y < 80; ++y)
                    for (int x = 0; x < 320; ++x)
                    {
                        const auto p = dropImg.getPixelAt (x, y);
                        const int dr = std::abs (p.getRed() - accentColour.getRed());
                        const int dg = std::abs (p.getGreen() - accentColour.getGreen());
                        const int db = std::abs (p.getBlue() - accentColour.getBlue());
                        if (p.getAlpha() > 120 && dr + dg + db < 90)
                            ++accentPixels;
                    }
                check (accentPixels > 40,
                       "variable editor: a drop shows its accent border + caret");
            }
            editor.setDragPreviewForTest (false, { 0, 0 });
            juce::Image img (juce::Image::ARGB, 320, 80, true);
            {
                juce::Graphics g (img);
                editor.paintEntireComponent (g, true);
            }
            check (inkPixels (img) > 0, "a variable editor paints");
        }

        // PILL TAB BAR: labels + icons, one active slot, taps reported.
        {
            ebs::PillTabBar bar;
            bar.setTabs ({ "Chat config", "Templates", "Web search" });
            bar.onTabIcon = [] (int i) -> std::optional<ebs::IconButton::Shape>
            {
                if (i == 0) return ebs::IconButton::Shape::chat;
                if (i == 1) return ebs::IconButton::Shape::sparkle;
                return ebs::IconButton::Shape::search;
            };
            int picked = -1;
            bar.onTabSelected = [&picked] (int i) { picked = i; };
            bar.setSize (420, 28);
            check (bar.tabCount() == 3 && bar.activeIndex() == 0,
                   "a pill tab bar starts on its first tab");
            check (bar.preferredWidth() > 0,
                   "a pill tab bar measures its slots");
            bar.clickTabForTest (2);
            check (picked == 2 && bar.activeIndex() == 2,
                   "a tab tap reports the new index once");
            check (bar.tabAtForTest (bar.getWidth() - 8) == 2,
                   "the rightmost slot belongs to the last tab");
            // SLIDE (author request): the active pill travels between slots
            // instead of blinking; the gate steps the animation itself.
            check (std::abs (bar.pillPositionForTest() - 0.0f) < 0.01f,
                   "the pill starts on the first slot");
            bar.stepAnimationForTest (1.0 / 60.0);
            const auto midway = bar.pillPositionForTest();
            check (midway > 0.0f && midway < 2.0f,
                   "a pill tab bar SLIDES its pill towards the new tab");
            for (int frame = 0; frame < 120; ++frame)
                bar.stepAnimationForTest (1.0 / 60.0);
            check (std::abs (bar.pillPositionForTest() - 2.0f) < 0.01f,
                   "a pill tab bar settles exactly on the picked slot");
            bar.setActiveIndex (0, false);
            bar.stepAnimationForTest (1.0 / 60.0);
            check (bar.pillPositionForTest() > 0.0f
                       && bar.pillPositionForTest() < 2.0f,
                   "the pill animates BACK, not teleports");
            juce::Image img;
            {
                img = juce::Image (juce::Image::ARGB, bar.getWidth(),
                                   bar.getHeight(), true);
                juce::Graphics g (img);
                bar.paintEntireComponent (g, true);
            }
            check (inkPixels (img) > 0, "a pill tab bar paints");
        }

        // The model menu button forwards its tooltip to the visible face.
        {
            ebs::ModelMenuButton button;
            button.setTooltip ("Which model runs the next turn");
            check (button.getTooltip().contains ("next turn"),
                   "a model menu button keeps its tooltip");
        }

        // PILL BUTTON: icon + label, sized to its content, with a hover state
        // that actually changes the drawing (the author asked for the pill
        // look with a real mouseover).
        {
            ebs::PillButton pill (ebs::IconButton::Shape::download, "Install");
            pill.setSize (pill.widthForLabel(), 24);
            check (pill.widthForLabel() > 40,
                   "a pill button measures its label");
            const auto render = [&pill] (bool over)
            {
                juce::Image img (juce::Image::ARGB, pill.getWidth(),
                                 pill.getHeight(), true);
                juce::Graphics g (img);
                pill.paintForTest (g, over, false);
                return img;
            };
            const auto idle = render (false);
            const auto hovered = render (true);
            int differences = 0;
            for (int y = 0; y < idle.getHeight(); ++y)
                for (int x = 0; x < idle.getWidth(); ++x)
                    if (idle.getPixelAt (x, y) != hovered.getPixelAt (x, y))
                        ++differences;
            check (differences > idle.getWidth() / 2,
                   "a pill button changes visibly on hover");
            // The "gated" LOOK comes from the tone (a dim pill), not from
            // Component::setEnabled: a disabled juce::Component is painted by
            // its parent, so an offscreen direct paint would test nothing.
            ebs::PillButton gated (ebs::IconButton::Shape::lock, "Runtime first");
            gated.setTone (ebs::PillButton::Tone::disabledish);
            gated.setSize (gated.widthForLabel(), 24);   // a component needs bounds
            juce::Image gatedImg;
            {
                gatedImg = juce::Image (juce::Image::ARGB, gated.getWidth(),
                                        gated.getHeight(), true);
                juce::Graphics gg (gatedImg);
                gated.paintForTest (gg, false, false);
            }   // the software renderer flushes when the Graphics goes away
            check (inkPixels (gatedImg) > 0,
                   "a gated pill still paints (dim), never a blank cell");
        }

        // STATE COLOURS: accent, warning and danger must stay distinguishable -
        // a filter, an action and an error are three different messages.
        check (ebs::accent() != ebs::warning()
                   && ebs::warning() != ebs::danger()
                   && ebs::accent() != ebs::danger(),
               "accent, warning and danger are three distinct colours");

        const ebs::IconButton::Shape shapes[] =
        {
            // The WHOLE enum: a new shape cannot ship without an ink check.
            ebs::IconButton::Shape::play,    ebs::IconButton::Shape::stop,
            ebs::IconButton::Shape::cross,   ebs::IconButton::Shape::refresh,
            ebs::IconButton::Shape::folder,  ebs::IconButton::Shape::grip,
            ebs::IconButton::Shape::undo,    ebs::IconButton::Shape::redo,
            ebs::IconButton::Shape::settings, ebs::IconButton::Shape::star,
            ebs::IconButton::Shape::wand,    ebs::IconButton::Shape::tool,
            ebs::IconButton::Shape::search,  ebs::IconButton::Shape::lock,
            ebs::IconButton::Shape::download, ebs::IconButton::Shape::eye,
            ebs::IconButton::Shape::eyeOff,  ebs::IconButton::Shape::rewind,
            ebs::IconButton::Shape::copy,    ebs::IconButton::Shape::sparkle,
            ebs::IconButton::Shape::kebab,   ebs::IconButton::Shape::music,
            ebs::IconButton::Shape::layers,  ebs::IconButton::Shape::activity,
            ebs::IconButton::Shape::chat,    ebs::IconButton::Shape::mic,
            ebs::IconButton::Shape::keys,    ebs::IconButton::Shape::repeat,
            ebs::IconButton::Shape::type,    ebs::IconButton::Shape::image,
            ebs::IconButton::Shape::film,
            ebs::IconButton::Shape::zoomIn,  ebs::IconButton::Shape::zoomOut,
            ebs::IconButton::Shape::chevronUp,   ebs::IconButton::Shape::chevronDown,
            ebs::IconButton::Shape::chevronLeft, ebs::IconButton::Shape::chevronRight,
            ebs::IconButton::Shape::grid,    ebs::IconButton::Shape::step,
            ebs::IconButton::Shape::power,   ebs::IconButton::Shape::trash,
            ebs::IconButton::Shape::save,    ebs::IconButton::Shape::expand,
            ebs::IconButton::Shape::compress,
            ebs::IconButton::Shape::pause,
            ebs::IconButton::Shape::plus
        };
        bool allShapesInk = true;
        for (const auto s : shapes)
        {
            PaintableIconButton ib (s);
            ib.setSize (28, 28);
            // On a CLEARED surface: the glyph is what leaves alpha behind, so
            // this asks "did the shape paint" and not "is the background
            // opaque" (the opaque renderer's black fill made this check true
            // for a shape that painted nothing at all).
            const auto img = renderOnClear (28, 28, [&] (juce::Graphics& g)
                { ib.paint (g); });
            if (inkPixels (img) < 5)
            {
                allShapesInk = false;
                std::printf ("       shape %d left no ink\n", (int) s);
            }
        }
        check (allShapesInk, "every IconButton shape paints visible ink");

        // The PAUSE glyph against the STOP one, by their CENTRE pixel: a stop
        // is a solid square (its centre is inked) and a pause is two bars with
        // a gap between them (its centre is not). The whole-enum check above
        // proves a shape paints SOMETHING; only this one can see a pause that
        // fell through to the stop branch, which is the mistake a transport
        // button makes the moment someone edits the chain.
        {
            PaintableIconButton pauseBtn (ebs::IconButton::Shape::pause);
            PaintableIconButton stopBtn (ebs::IconButton::Shape::stop);
            pauseBtn.setSize (28, 28);
            stopBtn.setSize (28, 28);
            const auto pauseImg = renderOnClear (28, 28, [&] (juce::Graphics& g)
                { pauseBtn.paint (g); });
            const auto stopImg = renderOnClear (28, 28, [&] (juce::Graphics& g)
                { stopBtn.paint (g); });
            const auto centre = juce::Point<int> (14, 14);
            // The bar is 60/255, not >10: a glyph EDGE is anti-aliased, so the
            // pause's faint bleed reads 16 at the exact centre while the stop's
            // solid square reads 255. Measuring "some ink" there would fail on
            // every correct pause.
            const int inkBar = 60;
            const auto pauseCentre = (int) pauseImg.getPixelAt (centre.x, centre.y).getAlpha();
            const auto stopCentre  = (int) stopImg.getPixelAt (centre.x, centre.y).getAlpha();
            std::printf ("[INFO] glyph centre alpha: pause %d, stop %d (bar %d)\n",
                         pauseCentre, stopCentre, inkBar);
            check (stopCentre > inkBar,
                   "the stop glyph inks its centre (it is a solid square)");
            check (pauseCentre <= inkBar,
                   "the pause glyph leaves its centre empty (two bars, one gap)");
        }

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

        // v0.12.5 StatusBar: an activity claim WITHOUT a determinate ratio
        // hosts an ebs::BusyIndicator strip in its own bar slot (the
        // segment used to show a bare "[Ns]"), and the strip goes back
        // down as soon as a ratio arrives or the claim finishes. The hops
        // are synchronous here (we ARE the message thread), so no waiting.
        {
            ebs::StatusBar claimBar;
            claimBar.setSize (300, 24);
            check (claimBar.getNumChildComponents() == 0,
                   "an idle status bar hosts no claim strip");

            auto claim = claimBar.beginActivity();
            check (claimBar.getNumChildComponents() == 1
                       && dynamic_cast<ebs::BusyIndicator*> (
                              claimBar.getChildComponent (0)) != nullptr,
                   "a progressless claim shows a busy strip");
            auto* strip = dynamic_cast<ebs::BusyIndicator*> (
                claimBar.getChildComponent (0));
            check (strip != nullptr && strip->isVisible()
                       && strip->getWidth() == 45 && strip->getHeight() == 8
                       && strip->getX() > 0,
                   "the claim strip fills the 45x8 bar slot");

            claim.progress (0.5f);
            check (strip != nullptr && ! strip->isVisible(),
                   "a determinate claim hands the slot back to the mini bar");

            claim.finish();
            check (strip != nullptr && ! strip->isVisible(),
                   "a finished claim keeps no strip visible");
        }

        // v0.12.1 TextSanitize: C0/C1 controls must never reach the
        // HarfBuzz shaper (missing-glyph jassert -> __debugbreak crash in
        // Debug, e.g. a standalone app on a double-encoded en-dash).
        {
            const auto hasControl = [] (const juce::String& s)
            {
                for (auto p = s.getCharPointer(); ! p.isEmpty(); ++p)
                {
                    const auto c = (juce::uint32) *p;
                    if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r')
                        || (c >= 0x7f && c <= 0x9f))
                        return true;
                }
                return false;
            };
            check (! hasControl (ebs::sanitizeForShaping ("plain ASCII")),
                   "sanitize keeps plain ASCII intact");
            // Genuine non-ASCII survives: en-dash + accent + CJK.
            const auto uni = juce::String::fromUTF8 (
                "caf\xC3\xA9 \xE2\x80\x93 \xE4\xB8\xAD");
            check (ebs::sanitizeForShaping (uni).toStdString()
                       == uni.toStdString(),
                   "sanitize preserves accents, en-dash and CJK");
            check (ebs::sanitizeForShaping ("a\tb\nc\rd") == "a\tb\nc\rd",
                   "sanitize keeps tab/LF/CR (JUCE substitutes them)");
            check (ebs::sanitizeForShaping ("").isEmpty(),
                   "sanitize handles the empty string");
            // The exact crash payload: en-dash UTF-8 (E2 80 93) decoded as
            // Latin-1, i.e. U+00E2 U+0080 U+0093 (fromUTF8 of C3 A2 C2 80
            // C2 93). The C1 pair must go; the printable U+00E2 stays.
            const auto corrupted = juce::String::fromUTF8 (
                "\xC3\xA2\xC2\x80\xC2\x93");
            const auto fixed = ebs::sanitizeForShaping (corrupted);
            check (! hasControl (fixed) && fixed.length() == 3
                       && fixed[0] == (juce::juce_wchar) 0xe2,
                   "sanitize strips the C1 pair of a double-encoded en-dash");
            check (ebs::sanitizeForShaping (fixed) == fixed,
                   "sanitize is idempotent");
            // End to end through the bar: corrupted log line + corrupted
            // meter value, then a real paint pass (the Debug crash repro:
            // without the logLine/updateMeter sanitize this trips the
            // HarfBuzz missing-glyph jassert).
            ebs::StatusBar crashBar;
            crashBar.setSize (400, 24);
            crashBar.logLine (juce::String::fromUTF8 (
                "Failed after \xE2\x80\x93: boom \xC3\xA2\xC2\x80\xC2\x93"));
            ebs::StatusBar::MeterReading mr;
            mr.ratio01 = 0.5f;
            mr.valueText = juce::String::fromUTF8 ("\xC3\xA2\xC2\x80");
            crashBar.updateMeter (ebs::StatusBar::MeterId::vram, mr);
            const auto crashImg = renderToImage (
                400, 24, [&] (juce::Graphics& g) { crashBar.paint (g); });
            check (inkPixels (crashImg) > 0,
                   "status bar paints a C1-poisoned line without crashing");
        }

        // === v0.12.7 StatusBar: the log window ============================
        // The log is the app's only text diagnostic surface. Four contracts,
        // each a defect this cut removes: it APPENDS (the old
        // replace-by-prefix heuristic overwrote unrelated lines), it stays
        // ALIVE while open (it used to be a snapshot frozen at open time),
        // its TRUNCATION is announced (the 500-line cap dropped lines
        // silently), and the reading position survives new arrivals.
        {
            // 1) APPEND-ONLY. The exact regression: "Import: " is shared, so
            //    the old heuristic - same text before the FIRST DIGIT within
            //    the last 12 entries - made the success line disappear when
            //    the error line landed.
            ebs::StatusBar hist;
            hist.logLine ("Import: converted 4 stems");
            hist.logLine ("Import: error: unreadable audio");
            hist.logLine ("Import: 4 stems written to disk");
            const auto snap = hist.historySnapshot();
            check ((int) snap.size() == 3
                       && snap[0] == "Import: converted 4 stems"
                       && snap[1] == "Import: error: unreadable audio"
                       && snap[2] == "Import: 4 stems written to disk",
                   "log keeps every line: a shared prefix never replaces one");

            // 2) ALIVE + INCREMENTAL. The window mirrors the history, then
            //    takes new lines with no reopen and no rebuild.
            ebs::LogView view;
            view.setSize (640, 300);
            view.reset (snap);
            const auto mirrored = view.getLogText();
            const auto at = [&mirrored] (const char* needle)
                { return mirrored.indexOf (needle); };
            check (view.getLogLineCount() == 3
                       && at ("converted 4 stems") >= 0
                       && at ("converted 4 stems") < at ("unreadable audio")
                       && at ("unreadable audio") < at ("written to disk"),
                   "log view mirrors the history, in order");
            view.sync ({ "Import: retrying", "Import: ok" }, 0);
            check (view.getLogLineCount() == 5
                       && view.getLogText().contains ("unreadable audio")
                       && view.getLogText().trim().endsWith ("Import: ok"),
                   "an open log window takes new lines without reopening");

            // 3) TRUNCATION VISIBLE. The cap is the consumer's number now,
            //    and every line it drops is counted and announced.
            ebs::StatusBar capped;
            capped.setMaxHistoryLines (4);
            check (capped.getMaxHistoryLines() == 4,
                   "history cap is configurable");
            for (int i = 0; i < 10; ++i)
                capped.logLine ("line " + juce::String (i));
            const auto cappedSnap = capped.historySnapshot();
            check ((int) cappedSnap.size() == 4
                       && cappedSnap.front() == "line 6"
                       && cappedSnap.back() == "line 9"
                       && capped.getTotalLoggedCount() == 10
                       && capped.getDroppedLineCount() == 6,
                   "the cap keeps the newest lines and counts what it dropped");
            check (ebs::StatusBar().getMaxHistoryLines() == 5000,
                   "default history cap is 5000 lines, not the silent 500");

            // 4) SCROLL POLICY. The tail is followed by default; a reader
            //    who scrolls up keeps the line they were on - by CONTENT, so
            //    the same text survives both arrivals and a front-drop that
            //    renumbers the whole document.
            std::vector<juce::String> rows;
            for (int i = 0; i < 200; ++i)
                rows.push_back ("row " + juce::String (i));

            ebs::LogView tail;
            tail.setSize (640, 300);
            tail.reset (rows);
            check (tail.isShowingTail(),
                   "a freshly opened log window shows the newest line");
            tail.sync ({ "arrival at the tail" }, 0);
            check (tail.isShowingTail(),
                   "arrivals keep a tail-following window on the tail");

            ebs::LogView reader;
            reader.setSize (640, 300);
            reader.reset (rows);
            reader.getEditor().scrollToLine (40);
            check (! reader.isShowingTail(),
                   "scrolling up takes the window off the tail");
            reader.sync ({ "arrival 1", "arrival 2" }, 0);
            check (reader.getEditor().getFirstLineOnScreen() == 40,
                   "a reader scrolled up keeps their position as lines arrive");
            // The cap dropping 5 lines shifts every remaining line up by 5:
            // the view must follow the content, not the index.
            reader.sync ({ "arrival 3" }, 5);
            check (reader.getEditor().getFirstLineOnScreen() == 35
                       && ! reader.isShowingTail(),
                   "a front-drop keeps the same text under the reader");

            // ... while a tail-following window stays on the tail through
            //     the very same drop.
            tail.sync ({ "newest" }, 7);
            check (tail.isShowingTail()
                       && tail.getLogText().trim().endsWith ("newest"),
                   "a tail-following window survives a front-drop");

            // 5) READ-ONLY SURFACE: no caret is painted into a document the
            //    user cannot edit (a blinking caret would claim otherwise),
            //    and typing at the caret changes nothing.
            {
                const auto textBefore = reader.getLogText();
                reader.getEditor().moveCaretToEnd (false);
                reader.getEditor().insertTextAtCaret ("MUST NOT APPEAR");
                const auto caretChild = [&reader]
                {
                    for (auto* child : reader.getEditor().getChildren())
                        if (dynamic_cast<juce::CaretComponent*> (child) != nullptr)
                            return true;
                    return false;
                };
                check (reader.getEditor().isReadOnly()
                           && ! caretChild()
                           && reader.getLogText() == textBefore,
                       "the log view is read-only, paints no caret, takes no edit");
            }

            // 6) TITLE BAR. juce::DialogWindow hands its base DocumentWindow
            //    closeButton and NOTHING else, and DocumentWindow maps
            //    requiredButtons to the peer flags - so the window was born
            //    asking the OS for an X alone. Constructing a DialogWindow
            //    creates no peer until it is shown, so this stays headless.
            auto* window = new ebs::LogWindow();
            const auto flags = window->getDesktopWindowStyleFlags();
            delete window;
            using Peer = juce::ComponentPeer;
            check ((flags & Peer::windowHasCloseButton) != 0
                       && (flags & Peer::windowHasMinimiseButton) != 0
                       && (flags & Peer::windowHasMaximiseButton) != 0,
                   "log window asks the OS for minimise, maximise and close");
        }

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

        // v0.17.8 PathRow browse filters: the dialog shows EVERYTHING until the host names
        // what its form reads, and what a host sets is exactly what its chooser gets.
        check (pathRow.getBrowseFilters() == "*",
               "a file row lists every file until the host declares its extensions");
        pathRow.setBrowseFilters ("*.lrc;*.srt;*.vtt");
        check (pathRow.getBrowseFilters() == "*.lrc;*.srt;*.vtt",
               "the browse filter is what the host set, character for character");
        ebs::PathRow dirRow (ebs::PathRow::Mode::Directory);
        dirRow.setBrowseFilters ("  *.json  ");   // spaces typed around a list never reach the dialog
        check (dirRow.getBrowseFilters() == "*.json",
               "the filter keeps exactly what the host wrote, without the spaces around it");
        // A Directory row stores its setting as the row's own state, and its chooser ignores
        // it: a folder dialog has no extension to list, so wildcards there change nothing.

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
                ebs::IconButton::Shape::refresh,
                ebs::IconButton::Shape::download,
                ebs::IconButton::Shape::eye,
                ebs::IconButton::Shape::eyeOff
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
                // Action column: N slots x 24 px from x=0; sample each.
                const int numGlyphSlots = (int) (sizeof (actionShapes)
                                                 / sizeof (actionShapes[0]));
                dl.setSize (numGlyphSlots * 24 + 56, 60);
                dl.resized();
                // One ink check per slot: each glyph leaves visible paint.
                // Threshold: mean brightness must RISE above the panel
                // background (dim glyphs like the hand still count).
                const auto glyphImg = renderToImage (
                    numGlyphSlots * 24 + 56, 60,
                    [&] (juce::Graphics& g)
                    { dl.paintEntireComponent (g, false); });
                const auto bgLum = [] (juce::Colour c)
                    { return 0.3f * c.getFloatRed()
                             + 0.6f * c.getFloatGreen()
                             + 0.1f * c.getFloatBlue(); };
                const float bg = bgLum (ebs::bgPanel());
                bool allGlyphInk = true;
                for (int i = 0; i < numGlyphSlots; ++i)
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
                             { 3, "Origin", 90 }, { 4, "Progress", 110 } });
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
            // Dedicated bar look for a Progress column: must leave ink too
            // (track + fill + percent).
            ebs::DataList::Row r4;
            r4.id = "t-4";
            r4.cells = { { 1, "delta" }, { 2, "50" }, { 3, "TTS" } };
            r4.progress = 0.25; r4.progressColumnId = 4;
            r4.progressIsBar = true;
            dl.setRows ({ r1, r2, r3, r4 });
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
            dl.sortBy (2, true);   // numeric-aware: 8 < 30 < 50 < 120
            check (dl.visibleRowCount() == 4
                       && dl.visibleRowId (0) == "t-2"
                       && dl.visibleRowId (1) == "t-1"
                       && dl.visibleRowId (2) == "t-4"
                       && dl.visibleRowId (3) == "t-3",
                   "datalist sorts numeric columns numerically");
            dl.sortBy (1, false);  // gamma > delta > beta > alpha
            check (dl.visibleRowId (0) == "t-3"
                       && dl.visibleRowId (3) == "t-1",
                   "datalist sorts text columns backwards");
            // Stable sort: rows with EQUAL keys keep id order across
            // refreshes (a progress tick rebuild must not permute the
            // downloading row with its "queued"/empty siblings).
            ebs::DataList dlTie;
            dlTie.setShowSearch (false);
            dlTie.setShowViews (false);
            dlTie.setColumns ({ { 1, "Name", 140 }, { 5, "Status", 110 } });
            ebs::DataList::Row q1, q2, q3;
            q1.id = "m-b"; q1.cells = { { 1, "b" }, { 5, "queued" } };
            q2.id = "m-a"; q2.cells = { { 1, "a" }, { 5, "queued" } };
            q3.id = "m-c"; q3.cells = { { 1, "c" }, { 5, "queued" } };
            dlTie.setRows ({ q1, q2, q3 });
            dlTie.sortBy (5, true);   // all keys equal -> id order a,b,c
            check (dlTie.visibleRowId (0) == "m-a"
                       && dlTie.visibleRowId (1) == "m-b"
                       && dlTie.visibleRowId (2) == "m-c",
                    "datalist breaks sort ties by stable row id");
            // Same rows, new progress values (a refresh tick): order must
            // not move.
            q1.progress = 0.1; q1.progressColumnId = 5;
            q1.progressIsBar = true;
            dlTie.setRows ({ q1, q2, q3 });
            check (dlTie.visibleRowId (0) == "m-a"
                       && dlTie.visibleRowId (1) == "m-b"
                       && dlTie.visibleRowId (2) == "m-c",
                    "datalist keeps equal-key order across progress refresh");
            // Selection + proxy + action-click behaviour slot.
            // A real UP click (zero drag distance) on the action column
            // must route to onAction with the slot's action id.
            dl.setRowActions ({ { 42, ebs::IconButton::Shape::download,
                                  "Install" } });
            juce::String clickedId;
            int clickedAction = 0;
            dl.onAction = [&] (const juce::String& id, int actionId)
            {
                clickedId = id;
                clickedAction = actionId;
            };
            dl.setSize (480, 160);
            dl.resized();
            const auto dlImg = renderToImage (480, 160,
                [&] (juce::Graphics& g)
                { dl.paintEntireComponent (g, false); });
            check (inkPixels (dlImg) > 200, "datalist paints rows + header");
            // setRows must repaint eagerly: paint the component straight
            // into an image AFTER a row-state change, with NO external
            // repaint in between - the progress glyph must already show.
            ebs::DataList::Row r4b = r4;
            r4b.progress = 0.9;
            dl.setRows ({ r1, r2, r3, r4b });
            const auto dlImg2 = renderToImage (480, 160,
                [&] (juce::Graphics& g)
                { dl.paintEntireComponent (g, false); });
            check (inkPixels (dlImg2) > 200,
                   "datalist setRows updates painted rows eagerly");
            // cellClicked on the action column with a real-click event
            // (down pos == event pos, not dragged) routes to onAction once.
            const int vis0 = dl.visibleRowCount() > 0 ? 0 : -1;
            if (vis0 >= 0)
            {
                const auto cellRect = dl.actionCellBoundsForTest (vis0);
                const auto centre = cellRect.getCentre().toFloat();
                juce::MouseEvent click (
                    juce::Desktop::getInstance().getMainMouseSource(),
                    centre,
                    juce::ModifierKeys::leftButtonModifier,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    dl.modelForTest() != nullptr ? &dl : nullptr,
                    &dl,
                    juce::Time::getCurrentTime(),
                    centre,
                    juce::Time::getCurrentTime(), 1, false);
                dl.modelForTest()->cellClicked (vis0,
                                                dl.actionColumnIdForTest(),
                                                click);
                check (clickedAction == 42 && clickedId.isNotEmpty(),
                       "datalist action click routes to onAction");
            }
            else
                check (false, "datalist action click routes to onAction");
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

        // 5) Disabled custom transport glyphs grey explicitly. The disabled
        //    button has weaker ink, and its play glyph no longer keeps the
        //    full accent colour.
        PaintableIconButton disabledPlay (ebs::IconButton::Shape::play);
        disabledPlay.setSize (28, 28);
        disabledPlay.setEnabled (false);
        const auto dpImg = renderToImage (28, 28, [&] (juce::Graphics& g)
            { disabledPlay.paint (g); });
        check (dpImg.getPixelAt (12, 14).getAlpha() > 20
                   && dpImg.getPixelAt (12, 14).getRed() < 0x60,
               "disabled icon button greys its custom glyph");

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

        // 5a-bis) ebs::ToggleSwitch: the DISABLED track. The dimming branch
        // used juce::Colour::withAlpha, which ASSERTS on the degenerate value
        // a transparent theme colour produces - so disabling a ToggleSwitch
        // broke the first repaint. Painted in every state so a regression is
        // a red test, not a crash on someone's panel.
        {
            ebs::ToggleSwitch disOn, disOff;
            disOn.setToggleState (true,  juce::dontSendNotification);
            disOn.setEnabled (false);
            disOff.setToggleState (false, juce::dontSendNotification);
            disOff.setEnabled (false);
            const auto disOnImg = renderToImage (44, 22, [&] (juce::Graphics& g)
                { disOn.paint (g); });
            const auto disOffImg = renderToImage (44, 22, [&] (juce::Graphics& g)
                { disOff.paint (g); });
            check (inkPixels (disOnImg) > 0 && inkPixels (disOffImg) > 0,
                   "disabled ToggleSwitch paints in both states without asserting");
            // Compare the two renders to each other rather than sampling a
            // magic pixel: the knob travels, so any fixed coordinate sits on
            // the knob in one state and the track in the other.
            check (disOnImg != disOffImg,
                   "disabled ToggleSwitch still reads ON vs OFF");
        }

        // 5b) ebs::Sidebar: collapse rail + width clamp + content hide.
        ebs::Sidebar side (ebs::Sidebar::Edge::Right);
        side.setWidths (180, 520, 300);
        juce::Component sideBody;
        side.setContent (&sideBody);
        side.setSize (318, 400);
        side.resized();
        check (side.outerWidth() == 318 && sideBody.isVisible()
                   && sideBody.getWidth() == 300
                   && side.handleBoundsForTest().getWidth() == ebs::Sidebar::railWidth(),
                "sidebar expanded lays out rail + body");
        const auto sideImg = renderToImage (318, 400, [&] (juce::Graphics& g)
            { side.paintEntireComponent (g, false); });
        check (inkPixels (sideImg) > 200, "sidebar paints rail chevron");
        int widthCb = -1;
        side.onWidthChanged = [&] (int w) { widthCb = w; };
        side.setSidebarWidth (90);   // below min -> clamped
        check (side.getSidebarWidth() == 180 && widthCb == 180,
                "sidebar width clamps to minimum and notifies");
        side.setSidebarWidth (900);  // above max -> clamped
        check (side.getSidebarWidth() == 520,
                "sidebar width clamps to maximum");
        side.setCollapsed (true);
        check (side.isCollapsed() && side.outerWidth() == ebs::Sidebar::railWidth()
                   && ! sideBody.isVisible(),
                "sidebar collapse hides content, rail only");
        side.setCollapsed (false);
        check (! side.isCollapsed() && sideBody.isVisible()
                   && side.outerWidth() == 520 + ebs::Sidebar::railWidth(),
                "sidebar expand restores content and width");
        // Proportional cap: no fixed max -> half the parent width.
        ebs::Sidebar prop (ebs::Sidebar::Edge::Right);
        prop.setWidths (180, 0, 300);   // 0 = proportional
        juce::Component host;
        host.setSize (1600, 800);
        host.addAndMakeVisible (prop);
        prop.setBounds (0, 0, 800, 800);
        prop.setSidebarWidth (1200);
        check (prop.getSidebarWidth () == 800,
                "sidebar caps the width at half the parent");
        check (prop.handleBoundsForTest().getWidth() == ebs::Sidebar::railWidth(),
                "sidebar rail keeps its width under the cap");
        host.removeAllChildren();
        side.setContent (nullptr);
         // 5c) ebs::BandGrip: height clamp and painting.
         ebs::BandGrip grip;
         grip.setHeightRange (96, 360);
         grip.setHeightSilently (200);
         check (grip.getHeightValue() == 200
                    && grip.clampForTest (20) == 96
                    && grip.clampForTest (500) == 360,
                "band grip clamps the host-owned height range");
         grip.setSize (120, ebs::BandGrip::defaultGripHeight());
         const auto gripImg = renderToImage (120, ebs::BandGrip::defaultGripHeight(),
             [&] (juce::Graphics& g) { grip.paint (g); });
         check (inkPixels (gripImg) > 20, "band grip paints its resize affordance");

         // 5d) ebs::BusyIndicator: themed strip + indeterminate/determinate API.
         ebs::BusyIndicator busy;
         busy.setSize (220, 16);
         busy.setStage ("Enhancing");
         check (busy.isIndeterminate() && busy.getProgress() < 0.0,
                "busy indicator starts as an indeterminate strip");
         const auto busyImg = renderToImage (220, 16, [&] (juce::Graphics& g)
             { busy.paint (g); });
         check (inkPixels (busyImg) > 40,
                "busy indicator paints its animated progress strip");
         busy.setProgress (0.5);
         check (! busy.isIndeterminate() && busy.getProgress() == 0.5,
                "busy indicator supports normalized determinate progress");
         busy.clear();
         check (busy.isIndeterminate(),
                "busy indicator clear returns to indeterminate mode");
         // The strip is a HIDDEN overlay in its two consumers (the TTS
         // panel hotspot, a status-bar claim slot), and juce::ProgressBar
         // only starts/stops its 30 ms timer from its OWN visibility
         // change: without the forwarding, a hidden indicator keeps the
         // message loop ticking for the rest of the session. NB: a
         // standalone BusyIndicator starts with its OWN visible flag
         // false, so show it first - setVisible (false) on a never-shown
         // instance is a no-op and would prove nothing.
         busy.setVisible (true);
         check (busy.stripControlVisibleForTest(),
                "shown busy indicator runs its strip timer");
         busy.setVisible (false);
         check (! busy.stripControlVisibleForTest(),
                "hidden busy indicator stops its strip timer");
         busy.setVisible (true);
         check (busy.stripControlVisibleForTest(),
                "re-shown busy indicator restarts its strip timer");
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

        // === DisclosureChevron + ExpansibleFrame (v0.12.29) ==============
        // The fold must be VISIBLE: setExpanded repaints the strip, so a
        // host that never moves it still sees the glyph flip. Direction is
        // read from ink columns: the two arm tips carry more ink than the
        // vertex, so closed (points right) has its heavy column on the left
        // (x 8) and open (points back left) has it on the right (x 13).
        {
            auto colInk = [] (const juce::Image& img, int x)
            {
                int n = 0;
                for (int y = 0; y < img.getHeight(); ++y)
                    if (img.getPixelAt (x, y).getBlue() > 60)
                        ++n;
                return n;
            };

            ebs::DisclosureChevron strip;
            strip.setSize (22, 40);
            const auto closedImg = renderToImage (22, 40, [&] (juce::Graphics& g)
                { strip.paint (g); });
            strip.setExpanded (true);
            const auto openImg = renderToImage (22, 40, [&] (juce::Graphics& g)
                { strip.paint (g); });
            check (colInk (closedImg, 13) > 0 && colInk (openImg, 8) > 0,
                   "the disclosure strip paints ink in both fold states");
            check (colInk (closedImg, 8) > colInk (closedImg, 13)
                       && colInk (openImg, 13) > colInk (openImg, 8),
                   "setExpanded flips the chevron to point back left");

            ebs::ExpansibleFrame frame;
            frame.setSize (120, 60);
            int relayouts = 0;
            frame.onExpandedChanged = [&] { ++relayouts; };
            frame.setExpanded (true);
            check (frame.expanded() && relayouts == 1,
                   "setExpanded opens the frame and tells the host once");
            frame.setExpanded (true);
            check (relayouts == 1, "a repeat setExpanded is silent");
            check (frame.interior().getWidth() == 120
                       - ebs::ExpansibleFrame::chevronWidth()
                       - ebs::ExpansibleFrame::chevronGap(),
                   "interior() excludes the chevron slot");

            // A VERTICAL fold (v0.17.0): the zone grows downward, so the glyph has
            // to say so - down when folded, up when open - while the strip keeps its
            // right edge and interior() is untouched (a horizontal strip would cost
            // the very height a short, wide frame is short of). Direction is read
            // from the HORIZONTAL SPREAD of ink per ROW, the mirror of the column
            // probe above: the two arm tips are wide apart, the vertex is pinched.
            auto rowSpan = [] (const juce::Image& img, int y)
            {
                int lo = -1, hi = -1;
                for (int x = 0; x < img.getWidth(); ++x)
                    if (img.getPixelAt (x, y).getBlue() > 60)
                    {
                        if (lo < 0) lo = x;
                        hi = x;
                    }
                return hi - lo;
            };

            ebs::DisclosureChevron vstrip;
            vstrip.setSize (40, 22);
            vstrip.setVerticalFold (true);
            // Read the direction AT RENDER TIME: a value read after the flip
            // describes the flip, not the frame that was just painted (the
            // first version of this check did exactly that and failed a
            // correct strip).
            const auto foldedDir = vstrip.chevronDirection();
            const auto vClosed = renderToImage (40, 22, [&] (juce::Graphics& g)
                { vstrip.paint (g); });
            vstrip.setExpanded (true);
            const auto openDir = vstrip.chevronDirection();
            const auto vOpen = renderToImage (40, 22, [&] (juce::Graphics& g)
                { vstrip.paint (g); });
            check (foldedDir == ebs::ChevronDirection::Down
                       && rowSpan (vClosed, 8) - rowSpan (vClosed, 13) >= 3,
                   "a folded vertical strip points DOWN (arms wide, vertex pinched)");
            check (openDir == ebs::ChevronDirection::Up
                       && rowSpan (vOpen, 13) - rowSpan (vOpen, 8) >= 3,
                   "an open vertical strip points UP (vertex now on top)");

            frame.setFoldVertical (true);
            check (frame.foldIsVertical(),
                   "ExpansibleFrame forwards the fold axis to its strip");
            check (frame.interior().getWidth() == 120
                       - ebs::ExpansibleFrame::chevronWidth()
                       - ebs::ExpansibleFrame::chevronGap(),
                   "a vertical fold keeps the strip on the right: interior() unchanged");
        }

        // === FrameBand (v0.13.0, fixed widths since v0.14.0) ==============
        // A row of frames that keeps each block at the width its own content asks
        // for, leaves spare room empty at the right edge, and scrolls only below
        // the hard floor. Widths are read back from the bounds the band set;
        // cue direction is read from the VERTICAL SPREAD of accent ink per column:
        // a cue dissolve band is bgPanel (blue 62), so only glyph ink passes the probe;
        // the spread, not the count, says which side the chevron vertex sits on.
        {
            auto colSpan = [] (const juce::Image& img, int x)
            {
                int lo = -1, hi = -1;
                for (int y = 0; y < img.getHeight(); ++y)
                    if (img.getPixelAt (x, y).getBlue() > 150)
                    {
                        if (lo < 0) lo = y;
                        hi = y;
                    }
                return hi - lo;   // vertical extent of ink in one column
            };

            auto colInk = [] (const juce::Image& img, int x)
            {
                int n = 0;
                for (int y = 0; y < img.getHeight(); ++y)
                    if (img.getPixelAt (x, y).getBlue() > 60)
                        ++n;
                return n;
            };

            ebs::FrameBand band (10);
            ebs::FramedBody fa, fb, fc;
            band.addFrame (&fa, 100, 60, true);
            band.addFrame (&fb, 120, 70, true);
            band.addFrame (&fc, 140, 80, false);   // fixed: never given spare room

            // Z-order is the child index: 0 is the back, the last entry is the front.
            // The strip paints over the frames, so it must be in front of them, and
            // the cues must stay in front of the strip.
            check (band.getIndexOfChildComponent (& band.scrollTrack())
                       > band.getIndexOfChildComponent (& fa),
                   "the scroll strip is in front of the frames it draws over");
            check (band.getIndexOfChildComponent (& band.rightCueStrip())
                       > band.getIndexOfChildComponent (& band.scrollTrack()),
                   "the cues stay in front of the scroll strip");

            check (band.preferredWidth() == 380 && band.minWidth() == 230,
                   "preferred and minimum widths count the gaps once");

            band.setBounds (0, 0, 500, 90);
            check (band.maxScroll() == 0, "a band that fits does not scroll");
            check (fa.getWidth() == 100 && fb.getWidth() == 120 && fc.getWidth() == 140,
                   "a frame keeps exactly the width its content asks for");
            check (! band.canScrollLeft() && ! band.canScrollRight(),
                   "a band that shows everything keeps both cues dark");

            band.setBounds (0, 0, 250, 90);
            check (fa.getWidth() == 60 && fb.getWidth() == 70 && fc.getWidth() == 140,
                   "below the floor a growable block stops at its minimum");
            check (band.maxScroll() == 40,
                   "the scroll span is the widths that were computed, not the floor");

            band.snapScroll (100);
            check (band.scrollPosition() == 40 && band.canScrollLeft() && ! band.canScrollRight(),
                   "snapScroll clamps to the span and arms the cue that has room");

            band.snapScroll (0);
            check (! band.canScrollLeft() && band.canScrollRight(),
                   "back at rest the right cue is armed and the left goes dark");

            band.revealFrame (&fc);
            check (band.targetScrollPosition() == 40,
                   "revealFrame asks for the smallest move, clamped to what the band can show");

            const auto leftImg = renderToImage (30, 82, [&] (juce::Graphics& g)
                { band.leftCueStrip().paint (g); });
            const auto rightImg = renderToImage (30, 82, [&] (juce::Graphics& g)
                { band.rightCueStrip().paint (g); });
            check (colSpan (leftImg, 12) < colSpan (leftImg, 17)
                       && colSpan (rightImg, 17) < colSpan (rightImg, 12),
                   "each cue keeps its vertex on the side where content is hidden");

            // A frame is never stretched, and a block whose width is fixed by its
            // content is never squeezed below it: only growable blocks give.
            band.setFrameWidths (&fb, 120, 70, false);
            band.setBounds (0, 0, 250, 90);
            check (fa.getWidth() == 60 && fb.getWidth() == 120 && fc.getWidth() == 140,
                   "a non-growable block keeps its preferred width when the row shrinks");
            check (band.maxScroll() == 90,
                   "the scroll span counts the widths that were kept, not the floor");

            // The strip appears only while content is hidden: its thumb is the share
            // of the row the view shows, and it sits where the row currently is.
            check (band.scrollTrack().isVisible() && band.trackWidth() == 242,
                   "a scrollable band shows the strip, inset 4 px at each edge");
            check (band.thumbLength() == 178,
                   "the thumb is the share of the row the view shows, of the strip width");
            check (band.thumbX() == 0, "at rest the thumb sits at the near end of the strip");

            const auto stripImg = renderToImage (band.trackWidth(), 8, [&] (juce::Graphics& g)
                { band.scrollTrack().paint (g); });
            check (colInk (stripImg, 90) == 4 && colInk (stripImg, 235) == 0,
                   "the strip paints a solid thumb at the near end and nothing past it");

            band.snapScroll (45);
            check (band.thumbX() == 32, "half way through the row the thumb is half way along");

            band.snapScroll (90);
            check (band.thumbX() == band.trackWidth() - band.thumbLength(),
                   "at full scroll the thumb reaches the far end of the strip");

            const auto endImg = renderToImage (band.trackWidth(), 8, [&] (juce::Graphics& g)
                { band.scrollTrack().paint (g); });
            check (colInk (endImg, 235) > colInk (endImg, 5),
                   "the thumb moves with the row: at full scroll its ink is at the far end");
            band.snapScroll (0);

            band.setBounds (0, 0, 500, 90);
            check (fa.getWidth() == 100 && fb.getWidth() == 120 && fc.getWidth() == 140,
                   "spare room is left empty: no block absorbs it");
            check (! band.scrollTrack().isVisible() && band.thumbLength() == 0,
                   "a band that shows everything keeps the scroll strip dark");
        }


        // v0.15.3 LevelMeter: a peak bar, a gain-reduction segment past the peak,
        // and one dB text line. The bar is drawn from numbers the host hands it, so
        // every check below is on the geometry those readings imply.
        // Bar interior is x 1..72 (the 1 px border sits on the edges), so a peak of
        // 0.5 fills to x = 1 + int(0.5 * 72) = 36, and a 6 dB reduction asks for
        // 0.5 * 10^(6/20) = 0.998, i.e. the red segment runs from x 37 to x 71.
        ebs::LevelMeter meter;
        meter.setBounds (0, 0, 74, 18);
        meter.setBarHeight (5);

        const auto renderMeter = [&] (float peak, float gr)
        {
            meter.setPeak (peak);
            meter.setGainReduction (gr);
            return renderToImage (74, 18, [&] (juce::Graphics& g)
            {
                meter.paintEntireComponent (g, false);
            });
        };

        const auto litMeter = renderMeter (0.5f, 6.0f);
        checkPixel (litMeter, { 10, 3 }, ebs::outputColour(),
                    "meter fills the bar to the level that left the stage");
        checkPixel (litMeter, { 50, 3 }, ebs::danger(),
                    "meter draws the gain-reduction segment from the right edge inwards");

        // A meter that is not limiting must show no red at all.
        const auto idleMeter = renderMeter (0.5f, 0.0f);
        checkPixel (idleMeter, { 72, 3 }, ebs::vizBg(),
                    "meter leaves the untouched part of the bar on the meter background");
        int dangerPixels = 0;
        for (int y = 0; y < idleMeter.getHeight(); ++y)
            for (int x = 0; x < idleMeter.getWidth(); ++x)
                if (idleMeter.getPixelAt (x, y) == ebs::danger())
                    ++dangerPixels;
        check (dangerPixels == 0, "a meter with zero gain reduction shows no red segment");

        // The segment is sized by the reduction: half the reduction covers half as much
        // of the bar, so a smaller reduction cannot reach into the peak's territory.
        const auto halfRed = renderMeter (0.5f, 3.0f);
        checkPixel (halfRed, { 60, 3 }, ebs::danger(),
                    "a smaller gain reduction still paints its segment");
        checkPixel (halfRed, { 50, 3 }, ebs::vizBg(),
                    "a smaller gain reduction covers less of the bar");

        // The bar must FALL with the feed: a meter holding a block peak would show a
        // frozen bar long after the sound stopped. Same pixel, quieter feed.
        const auto quietMeter = renderMeter (0.25f, 0.0f);
        checkPixel (quietMeter, { 30, 3 }, ebs::vizBg(),
                    "the bar shrinks when the peak falls");

        // The reading line is what the user reads, so it has to carry glyph ink.
        // Counted BELOW the bar: an 8 pt glyph never lands on its exact colour (every
        // pixel is a partial coverage), so ink is any pixel that is neither the black
        // pre-fill nor the meter background.
        int lineInk = 0;
        for (int y = 5; y < litMeter.getHeight(); ++y)
            for (int x = 0; x < litMeter.getWidth(); ++x)
            {
                const auto c = litMeter.getPixelAt (x, y);
                if (c != juce::Colours::black && c != ebs::vizBg())
                    ++lineInk;
            }
        check (lineInk > 0, "the dB reading paints glyphs on the text line");

        // A hot feed cannot draw past the bar, and a negative reduction is inactive.
        meter.setPeak (1.5f);
        check (meter.getPeak() == 1.0f, "peak is clamped to the top of the bar");
        meter.setGainReduction (-3.0f);
        check (meter.getGainReduction() == 0.0f, "negative gain reduction reads as inactive");

        // The meter carries a tooltip, so it has to answer the tooltip query: Component
        // alone does not give that interface, SettableTooltipClient does.
        meter.setTooltip ("peak, reduction, trim");
        check (meter.getTooltip() == "peak, reduction, trim",
               "the meter answers a tooltip query");

        // v0.15.16 ActivityDot: one dot per signal path, sized by the level that path
        // carried. The host hands it a linear peak from its own snapshot, so every check
        // below is on the disc that number implies. The dB floor is -45, full scale is 1,
        // and the disc's travel is the inner 6 px of a 10 px slot (10 - 4).
        ebs::ActivityDot dot;
        dot.setBounds (0, 0, 16, 16);
        dot.setDotDiameter (10);

        const auto renderDot = [&] (std::initializer_list<float> feeds)
        {
            dot.reset();
            for (const float f : feeds)
                dot.setLevel (f);
            return renderToImage (16, 16, [&] (juce::Graphics& g)
            {
                dot.paintEntireComponent (g, false);
            });
        };

        const auto countColour = [] (const juce::Image& img, juce::Colour c)
        {
            int n = 0;
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                    if (img.getPixelAt (x, y) == c)
                        ++n;
            return n;
        };

        // Silence: the slot exists (a ring, a dim pip) and carries NO accent at all.
        const auto silentDot = renderDot ({ 0.0f });
        check (countColour (silentDot, ebs::outputColour()) == 0,
               "a dot with no signal behind it paints no accent");
        // The ring is antialiased, so its ink is counted rather than sampled at one
        // pixel: any pixel that is neither the black pre-fill nor the slot background
        // is edge or pip.
        int slotInk = 0;
        for (int y = 0; y < silentDot.getHeight(); ++y)
            for (int x = 0; x < silentDot.getWidth(); ++x)
            {
                const auto c = silentDot.getPixelAt (x, y);
                if (c != juce::Colours::black && c != ebs::vizBg())
                    ++slotInk;
            }
        check (slotInk > 0, "an unlit dot still draws its slot ring and its dim pip");
        check (dot.getFill() == 0.0f, "silence reads as a dark disc");

        // Full scale: the accent covers the inner disc, and its area grows with level.
        const auto loudDot = renderDot ({ 1.0f });
        const int loudPixels = countColour (loudDot, ebs::outputColour());
        check (loudPixels > 0, "a path carrying full scale lights the accent disc");
        checkPixel (loudDot, { 8, 8 }, ebs::outputColour(),
                    "the disc is solid at its centre when the path is at full scale");
        check (dot.getFill() == 1.0f, "full scale fills the disc");

        const auto halfDot = renderDot ({ 0.5f });
        const int halfPixels = countColour (halfDot, ebs::outputColour());
        check (halfPixels > 0 && halfPixels < loudPixels,
               "a quieter path paints a smaller disc, not a dimmer one");

        // The dB scale is what makes a quiet path readable: -6 dBFS is not half the
        // travel, it is almost all of it, while -40 dBFS is nearly dark.
        dot.reset();
        dot.setLevel (0.5f);   // -6 dBFS
        check (dot.getFill() > 0.8f, "a -6 dBFS path reads near the top of the disc");
        dot.reset();
        dot.setLevel (0.01f);  // -40 dBFS
        const float quietFill = dot.getFill();
        check (quietFill > 0.0f && quietFill < 0.2f,
               "a -40 dBFS path lights a small disc rather than a bright one");

        // Under the floor there is nothing at all, and a hot feed cannot overfill.
        dot.reset();
        dot.setLevel (0.001f);   // -60 dBFS
        check (dot.getFill() == 0.0f, "below the -45 dBFS floor the dot stays dark");
        dot.setLevel (2.0f);
        check (dot.getLevel() == 1.0f && dot.getFill() == 1.0f,
               "a level above full scale is clamped, never drawn past the slot");

        // The release ramp: one peak holds for a moment, then the disc falls away on
        // its own once the path stops carrying signal.
        dot.reset();
        dot.setLevel (1.0f);
        dot.setLevel (0.0f);
        check (dot.getFill() > 0.9f, "one peak holds the dot briefly, so short words read");
        int releaseTicks = 0;
        while (dot.getFill() > 0.0f && releaseTicks < 100)
        {
            dot.setLevel (0.0f);
            ++releaseTicks;
        }
        check (releaseTicks > 5 && releaseTicks < 40,
               "the released dot falls over a fraction of a second, not one tick");

        // reset() skips the ramp: a host that hides the dot does not want it to glow
        // on the next layout pass.
        dot.setLevel (1.0f);
        dot.reset();
        check (dot.getFill() == 0.0f && dot.getLevel() == 0.0f,
               "reset drops the dot and its remembered feed at once");

        // Nothing is ever painted outside the rectangle the host gave the dot: a tight
        // strip must not let two neighbours bleed into each other. The dot is really
        // parented here, because paintEntireComponent does NOT translate by a component's
        // own position - a detached component would be painted at the image origin and the
        // check would read its own ink as a leak.
        juce::Image dotCanvas (juce::Image::ARGB, 40, 40, true);
        {
            juce::Graphics gc (dotCanvas);
            gc.fillAll (juce::Colours::black);
        }
        juce::Component dotHolder;
        dotHolder.setSize (40, 40);
        ebs::ActivityDot wideDot;
        dotHolder.addAndMakeVisible (wideDot);
        wideDot.setBounds (8, 8, 24, 24);
        wideDot.setDotDiameter (10);
        wideDot.setLevel (1.0f);
        {
            juce::Graphics gc (dotCanvas);
            dotHolder.paintEntireComponent (gc, true);
        }
        int outside = 0;
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 40; ++x)
            {
                const bool inside = (x >= 8 && x < 32 && y >= 8 && y < 32);
                if (! inside && dotCanvas.getPixelAt (x, y) != juce::Colours::black)
                    ++outside;
            }
        check (outside == 0, "the dot paints inside its own bounds and nowhere else");
        dotHolder.removeAllChildren();

        // The dot answers a tooltip query like every other display in the library.
        dot.setTooltip ("signal on this path");
        check (dot.getTooltip() == "signal on this path",
               "the dot answers a tooltip query");

        // Controls must keep working THROUGH the dot: it is an indicator, not a control.
        check (! dot.hitTest (8, 8),
               "the dot takes no clicks of its own, so the knob under it still works");

        // v0.17.2 ActivityKnob: the same dot, carried by the knob it reports. A component that
        // owns exactly ONE knob has nowhere else to put it - the cell's power toggle already
        // spends 18 of its 74 px on the tick well - so the dot IS the knob's hub, drawn on top
        // of the dial at the centre of the bounds. The dial itself is painted first, by the
        // shared LookAndFeel, exactly as a plain ebs::Knob paints it.
        ebs::ActivityKnob actKnob;
        actKnob.setBounds (0, 0, 54, 54);
        actKnob.setActivityDotDiameter (9);

        const auto renderActKnob = [&] (int size, float feed)
        {
            actKnob.setBounds (0, 0, size, size);
            // Half the range, so the value arc has a length to paint: at 0 the arc is a
            // zero-length path and there is nothing to look for.
            actKnob.setValue (5.0, juce::dontSendNotification);
            actKnob.resetActivity();
            if (feed > 0.0f)
                actKnob.setActivityLevel (feed);
            return renderToImage (size, size, [&] (juce::Graphics& g)
            {
                actKnob.paintEntireComponent (g, false);
            });
        };

        const auto idleKnob = renderActKnob (54, 0.0f);
        const auto fedKnob  = renderActKnob (54, 1.0f);

        check (countColour (idleKnob, ebs::outputColour()) == 0,
               "a knob whose path is silent paints no accent disc");
        check (countColour (fedKnob, ebs::outputColour()) > 0,
               "a knob whose path carries signal paints its accent disc");
        checkPixel (fedKnob, { 27, 27 }, ebs::outputColour(),
                    "the disc sits at the knob's centre, which the pointer line leaves free");
        check (actKnob.getActivityFill() == 1.0f, "full scale fills the knob's disc");
        check (actKnob.getActivityLevel() == 1.0f, "the knob remembers the level it was fed");

        // The dial must survive the dot: the accent value arc is still painted, and the white
        // pointer line still carries ink beside the disc.
        check (countColour (fedKnob, ebs::accent()) > 0,
               "the knob still paints its value arc under the dot");
        int pointerInk = 0;
        for (int y = 0; y < 54; ++y)
            for (int x = 0; x < 54; ++x)
            {
                const auto c = fedKnob.getPixelAt (x, y);
                if (c.getRed() > 200 && c.getGreen() > 200 && c.getBlue() > 200)
                    ++pointerInk;
            }
        check (pointerInk > 0, "the knob still paints its white pointer line beside the dot");

        // The disc is bounded by the knob's FACE and by the pointer line: a squeezed dial shrinks
        // the dot instead of letting it spill over the cap, and a dial too small for a readable
        // dot shows none at all - an indicator that hides its own value is worse than none.
        const auto smallKnob = renderActKnob (20, 1.0f);
        int accentOutsideFace = 0;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x)
                if (smallKnob.getPixelAt (x, y) == ebs::outputColour()
                    && (x < 7 || x > 12 || y < 7 || y > 12))
                    ++accentOutsideFace;
        check (accentOutsideFace == 0, "a small dial keeps its disc inside the cap");

        // 31 px is the smallest knob in OpenVoxTuner's Effects row (the Delay column): its
        // pointer tip stops 1.3 px from the centre, so the knob paints NO dot rather than
        // covering the line that says where it is set.
        const auto tinyKnob = renderActKnob (31, 1.0f);
        check (countColour (tinyKnob, ebs::outputColour()) == 0,
               "a dial too small for a clear dot paints none, never one over its pointer");

        // And the boundary behaves: 42 px is the smallest dial that can carry the default 9 px
        // dot clear of its pointer, so it draws one.
        const auto smallOkKnob = renderActKnob (42, 1.0f);
        check (countColour (smallOkKnob, ebs::outputColour()) > 0,
               "a dial big enough for the dot clear of its pointer draws it");

        // It is still a knob: the slider contract is untouched, and the tooltip answers like
        // every other display in the library.
        actKnob.setBounds (0, 0, 54, 54);
        check (actKnob.getMinimum() == 0.0 && actKnob.getMaximum() == 10.0,
               "an activity knob keeps the slider contract of a plain knob");
        actKnob.setTooltip ("signal on this path");
        check (actKnob.getTooltip() == "signal on this path",
               "the knob's dot answers a tooltip query");
        check (actKnob.getActivityDotDiameter() == 9,
               "the knob remembers the dot diameter it was given");

        // === Composed PNG receipt ===========================================
        // CANONICAL offscreen rendering: assemble a REAL component hierarchy
        // under a detached parent, then let paintEntireComponent do the
        // parenting (clip + per-child translation) exactly like on screen.
        juce::Component canvas;
        canvas.setSize (260, 280);

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

        // v0.13.0: the frame band on the receipt - three frames in a width that
        // forces them to their floors, at rest, so the right cue shows its glyph.
        ebs::FramedBody bandA, bandB, bandC;
        ebs::FrameBand band;
        canvas.addAndMakeVisible (band);
        band.setBounds (10, 200, 240, 60);   // resized() lays the frames out
        band.addFrame (&bandA, 90, 55, true);
        band.addFrame (&bandB, 90, 55, true);
        band.addFrame (&bandC, 90, 55, false);

        // v0.15.13 (the spine) + v0.15.14 (accent-free captions) + v0.15.15 (measured room names): two frames on the receipt carry a left vertical title spine, at the width
        // the component asks for and the full frame height - what a host in a tight bottom row pays.
        ebs::FrameTitleBand titleA, titleC;
        bandA.addAndMakeVisible (titleA);
        titleA.setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), 60);
        titleA.setTitle ("Key");
        bandC.addAndMakeVisible (titleC);
        titleC.setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), 60);
        titleC.setTitle ("Reverb");

        // v0.15.3: the level meter under the knob, in the SAME cell - the slot a host
        // gives it inside an effects column, which costs the frame no width.
        meter.setPeak (0.6f);
        meter.setGainReduction (4.0f);
        meter.setTrim (1.0f);
        canvas.addAndMakeVisible (meter);
        meter.setBounds (96, 152, 74, 18);

        // v0.15.16: two activity dots beside that cell - a path carrying signal and a
        // silent one - at the 16 px slot a host puts a dot in. Parenting matters here:
        // the disc has to keep its level once the component is really on screen.
        canvas.addAndMakeVisible (dot);
        dot.setBounds (150, 132, 16, 16);
        dot.reset();
        dot.setLevel (1.0f);
        ebs::ActivityDot silentDotHost;
        canvas.addAndMakeVisible (silentDotHost);
        silentDotHost.setBounds (170, 132, 16, 16);
        silentDotHost.reset();
        silentDotHost.setLevel (0.0f);

        juce::Image receipt (juce::Image::ARGB, 260, 280, true);
        {
            juce::Graphics g (receipt);
            g.fillAll (ebs::bgDark());
            canvas.paintEntireComponent (g, false);
        }
        writePngReceipt (receipt);

        // The meter must survive real parenting: on the canvas it sits under the knob
        // in the same cell, at (96,152) with a 74x18 slot. Bar interior is x 97..168,
        // so peak 0.6 fills to x = 97 + int(0.6 * 72) = 140 and the 4 dB reduction
        // (wanted clamped to full scale) draws red from x 140 to x 168.
        checkPixel (receipt, { 120, 154 }, ebs::outputColour(),
                    "the meter keeps its peak fill when parented on the canvas");
        checkPixel (receipt, { 160, 154 }, ebs::danger(),
                    "the meter keeps its gain-reduction segment when parented");

        // The two dots, on the same canvas: the fed one keeps its disc at its centre,
        // and the silent one keeps its slot background - the level is a property of the
        // PATH, so a silent path must not borrow the lit one's ink.
        checkPixel (receipt, { 158, 140 }, ebs::outputColour(),
                    "a fed dot keeps its disc when parented on the canvas");
        checkPixel (receipt, { 178, 137 }, ebs::vizBg(),
                    "a silent dot keeps its empty slot when parented beside a lit one");

        // Detach children BEFORE stack unwind order gets tricky: canvas is
        // destroyed first (declared last among the parents here), while its
        // children are older stack objects.
        canvas.removeAllChildren();

        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    // v0.12.27 TextReadability: a neutral backing over an arbitrary
    // background. `off` must not move a pixel; `outline` must add a dark
    // contour around the glyphs; `scrim` must darken the band; and the
    // contour must be SKIPPED below the minimum glyph height, because the
    // smallest rows of a roll are carried by the band, not by a halo.
    {
        const juce::String word ("legible");
        const juce::Rectangle<float> box (0.0f, 0.0f, 300.0f, 80.0f);

        // On a WHITE background so dark backing ink is countable and the
        // opaque white fill is not.
        const auto renderBacking = [&] (ebs::Readability mode, float size,
                                        bool drawBacking = true,
                                        float fade = 1.0f)
        {
            const auto font = ebs::createFont (size, true);
            return renderToImage (300, 80, [&] (juce::Graphics& g)
            {
                g.fillAll (juce::Colours::white);
                const auto path = ebs::glyphPathForText (
                    font, word, box, juce::Justification::centred, false);
                if (drawBacking)
                    ebs::drawReadabilityBacking (g, path, font, mode, 1.0f,
                                                 fade);
                g.setColour (juce::Colours::white);
                g.fillPath (path);
            });
        };

        // How much ink the backing left, alpha-agnostic: the fill is opaque
        // white, so every dark pixel is the backing, and a faded contour
        // composites to a LIGHTER grey over the white rather than to a
        // narrower one. Total darkness is what a fade moves - `alpha` x the
        // area it covers - which is why it, and not a thresholded pixel
        // count, can tell an opacity change from a width change.
        const auto totalDarkness = [] (const juce::Image& img)
        {
            double sum = 0.0;
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                    sum += 1.0 - (double) img.getPixelAt (x, y).getBrightness();
            return sum;
        };

        // Dark, opaque ink: the white fill never makes any.
        const auto darkInk = [] (const juce::Image& img)
        {
            int n = 0;
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                {
                    const auto c = img.getPixelAt (x, y);
                    if (c.getAlpha() > 200 && c.getBrightness() < 0.5f)
                        ++n;
                }
            return n;
        };

        // The glyph path must be the very arrangement drawText builds: a
        // real path with plausible extent, not an empty one.
        {
            const auto font = ebs::createFont (48.0f, true);
            const auto path = ebs::glyphPathForText (
                font, word, box, juce::Justification::centred, false);
            check (path.getBounds().getWidth() > 20.0f
                       && path.getBounds().getHeight() > 10.0f,
                   "glyphPathForText returns a real glyph path");
        }

        check (darkInk (renderBacking (ebs::Readability::off, 48.0f)) == 0,
               "readability off paints no backing ink");
        check (darkInk (renderBacking (ebs::Readability::shadow, 48.0f)) > 0,
               "readability shadow draws ink");
        check (darkInk (renderBacking (ebs::Readability::outline, 48.0f)) > 0,
               "readability outline draws a contour");
        check (darkInk (renderBacking (ebs::Readability::outline, 12.0f)) == 0,
               "readability contour is skipped below the minimum glyph height");

        // `off` is the promise that a host opting into the library paints
        // byte-identical pixels: same draw with and without the call.
        {
            const auto withOff = renderBacking (ebs::Readability::off, 48.0f, true);
            const auto without = renderBacking (ebs::Readability::off, 48.0f, false);
            bool same = true;
            for (int y = 0; y < withOff.getHeight() && same; ++y)
                for (int x = 0; x < withOff.getWidth() && same; ++x)
                    if (withOff.getPixelAt (x, y) != without.getPixelAt (x, y))
                        same = false;
            check (same, "readability off is a no-op next to the plain fill");
        }

        // v0.14.1 the backing carries the caller's own fade for a run: a
        // lyric roll dims the rows away from the centre, and a full-strength
        // contour around a nearly-gone word reads as the outline being the
        // text. The fade multiplies OPACITY only - the width stays the
        // caller's `strength` - so a faded contour must touch the SAME
        // pixels, more faintly, and never a thinner ring.
        {
            const auto full = renderBacking (ebs::Readability::outline, 48.0f,
                                             true, 1.0f);
            const auto half = renderBacking (ebs::Readability::outline, 48.0f,
                                             true, 0.5f);
            const auto none = renderBacking (ebs::Readability::outline, 48.0f,
                                             true, 0.0f);
            check (totalDarkness (half) > 0.0
                       && totalDarkness (half) < totalDarkness (full) * 0.75,
                   "readability fade dims the contour");
            check (totalDarkness (none) == 0.0,
                   "readability fade 0 draws no backing");
            // A width that followed the fade would SQUARE the drop: the ink's
            // total darkness is alpha x area, so an opacity-only fade lands
            // near 0.5 at `fade` 0.5 where a fade that also thinned the stroke
            // lands near 0.25 - measured by mass, never by a threshold, which
            // cannot compare two composites drawn at different alphas.
            const auto ratio = totalDarkness (half) / totalDarkness (full);
            std::printf ("[INFO] readability fade 0.5 ink ratio: %.3f\n", ratio);
            check (ratio > 0.4 && ratio < 0.6,
                   "readability fade scales the contour's opacity, not its width");

            // v0.15.3 the contour takes a tint (the shadow stays black). The
            // defaulted colour must paint the HISTORIC pixels: same call with
            // and without the seventh argument, byte for byte.
            {
                const auto viaDefault = renderToImage (300, 80, [&] (juce::Graphics& g)
                {
                    const auto font = ebs::createFont (48.0f, true);
                    g.fillAll (juce::Colours::white);
                    const auto path = ebs::glyphPathForText (
                        font, word, box, juce::Justification::centred, false);
                    ebs::drawReadabilityBacking (g, path, font,
                                                 ebs::Readability::outline, 1.0f, 1.0f);
                    g.setColour (juce::Colours::white);
                    g.fillPath (path);
                });
                const auto viaBlack = renderToImage (300, 80, [&] (juce::Graphics& g)
                {
                    const auto font = ebs::createFont (48.0f, true);
                    g.fillAll (juce::Colours::white);
                    const auto path = ebs::glyphPathForText (
                        font, word, box, juce::Justification::centred, false);
                    ebs::drawReadabilityBacking (g, path, font,
                                                 ebs::Readability::outline, 1.0f, 1.0f,
                                                 juce::Colours::black);
                    g.setColour (juce::Colours::white);
                    g.fillPath (path);
                });
                bool same = true;
                for (int y = 0; y < viaDefault.getHeight() && same; ++y)
                    for (int x = 0; x < viaDefault.getWidth() && same; ++x)
                        if (viaDefault.getPixelAt (x, y) != viaBlack.getPixelAt (x, y))
                            same = false;
                check (same, "readability default contour is the historic black");
            }

            // And the tint must ARRIVE: a red contour leaves reddish ink where
            // the black one leaves neutral grey.
            {
                const auto redness = [&] (ebs::Readability mode, juce::Colour contour)
                {
                    const auto font = ebs::createFont (48.0f, true);
                    const auto img = renderToImage (300, 80, [&] (juce::Graphics& g)
                    {
                        g.fillAll (juce::Colours::white);
                        const auto path = ebs::glyphPathForText (
                            font, word, box, juce::Justification::centred, false);
                        ebs::drawReadabilityBacking (g, path, font, mode, 1.0f,
                                                     1.0f, contour);
                        g.setColour (juce::Colours::white);
                        g.fillPath (path);
                    });
                    // Distance from white, NEVER getBrightness: JUCE's
                    // brightness is the max channel, so a saturated red reads
                    // as bright as the white around it and a darkness meter
                    // cannot see it at all.
                    double red = 0.0, total = 0.0;
                    for (int y = 0; y < img.getHeight(); ++y)
                        for (int x = 0; x < img.getWidth(); ++x)
                        {
                            const auto c = img.getPixelAt (x, y);
                            const auto offWhite = (765.0 - (double) c.getRed()
                                                   - (double) c.getGreen()
                                                   - (double) c.getBlue()) / 765.0;
                            red += offWhite * ((double) c.getRed() / 255.0);
                            total += offWhite;
                        }
                    return total > 0.0 ? red / total : 0.0;
                };
                const auto blackInk = redness (ebs::Readability::outline,
                                               juce::Colours::black);
                const auto redInk = redness (ebs::Readability::outline,
                                             juce::Colours::red);
                std::printf ("[INFO] readability contour redness black=%.3f red=%.3f\n",
                             blackInk, redInk);
                check (redInk > blackInk + 0.2,
                       "readability contour carries the caller's tint");
            }

            // v0.15.4 the weight ARRIVES as a stroke, and a weight of 1 is the
            // historic pixels: the eighth parameter is additive, never a
            // silent reshuffle of a width a host already tuned.
            {
                const auto renderWeight = [&] (std::optional<float> weight)
                {
                    const auto font = ebs::createFont (48.0f, true);
                    return renderToImage (300, 80, [&] (juce::Graphics& g)
                    {
                        g.fillAll (juce::Colours::white);
                        const auto path = ebs::glyphPathForText (
                            font, word, box, juce::Justification::centred, false);
                        if (weight.has_value())
                            ebs::drawReadabilityBacking (g, path, font,
                                                         ebs::Readability::outline, 1.0f,
                                                         1.0f, juce::Colours::black, *weight);
                        else
                            ebs::drawReadabilityBacking (g, path, font,
                                                         ebs::Readability::outline, 1.0f,
                                                         1.0f, juce::Colours::black);
                        g.setColour (juce::Colours::white);
                        g.fillPath (path);
                    });
                };
                const auto inkW1 = totalDarkness (renderWeight (1.0f));
                const auto inkW2 = totalDarkness (renderWeight (2.0f));
                std::printf ("[INFO] readability contour ink w1=%.0f w2=%.0f\n",
                             inkW1, inkW2);
                // Mass, not a threshold: twice the stroke width is roughly
                // twice the ink, and no pixel count can compare two composites
                // drawn at different widths over the same fill.
                check (inkW2 > inkW1 * 1.5 && inkW2 < inkW1 * 2.5,
                       "readability contour weight thickens the stroke");

                const auto viaDefault = renderWeight (std::nullopt);
                const auto viaOne = renderWeight (1.0f);
                bool same = true;
                for (int y = 0; y < viaDefault.getHeight() && same; ++y)
                    for (int x = 0; x < viaDefault.getWidth() && same; ++x)
                        if (viaDefault.getPixelAt (x, y) != viaOne.getPixelAt (x, y))
                            same = false;
                check (same, "readability contour weight 1 is the historic pixels");
            }
        }

        // The scrim darkens the band on a mid background, and a strength of
        // zero leaves it alone.
        {
            const auto bandLuma = [] (const juce::Image& img)
            {
                double sum = 0.0;
                int n = 0;
                for (int y = 80; y < 120; ++y)
                    for (int x = 0; x < img.getWidth(); ++x)
                    {
                        sum += (double) img.getPixelAt (x, y).getBrightness();
                        ++n;
                    }
                return n > 0 ? sum / (double) n : 0.0;
            };
            const juce::Rectangle<float> full (0.0f, 0.0f, 200.0f, 200.0f);
            const auto renderScrim = [&] (float strength)
            {
                return renderToImage (200, 200, [&] (juce::Graphics& g)
                {
                    g.fillAll (juce::Colours::grey);
                    ebs::drawReadabilityScrim (g, full, 100.0f, strength);
                });
            };
            const auto plain = renderScrim (0.0f);
            const auto scrimmed = renderScrim (1.0f);
            check (bandLuma (scrimmed) < bandLuma (plain) - 0.02,
                   "readability scrim darkens the focus band");
            check (juce::approximatelyEqual (bandLuma (renderScrim (0.0f)),
                                             bandLuma (plain)),
                   "readability scrim at strength 0 changes nothing");
        }

        // The contour width scales with the glyph and stays inside its rails.
        check (ebs::readabilityContourWidth (200.0f)
                   > ebs::readabilityContourWidth (30.0f)
               && ebs::readabilityContourWidth (300.0f) <= 12.0f
               && ebs::readabilityContourWidth (2.0f) >= 1.2f,
               "readability contour width scales and is clamped");

        // v0.15.4 the reader's own WEIGHT multiplies that width, and it
        // multiplies AFTER the clamp: on the smallest rows the designed width
        // already sits on the 1.2 px floor, so a weight folded INTO the clamp
        // is a control that does nothing exactly where a reader who cannot
        // read the words is looking.
        {
            check (juce::approximatelyEqual (ebs::readabilityContourWidth (48.0f, 1.0f),
                                             ebs::readabilityContourWidth (48.0f)),
                   "readability contour weight 1 is the designed width");
            const auto floorW = ebs::readabilityContourWidth (16.0f, 1.0f);
            const auto floorHeavy = ebs::readabilityContourWidth (16.0f, 2.0f);
            std::printf ("[INFO] readability contour floor w1=%.2f w2=%.2f\n",
                         floorW, floorHeavy);
            check (floorHeavy > floorW * 1.9f,
                   "readability contour weight multiplies after the clamp");
            check (juce::approximatelyEqual (ebs::readabilityContourWidth (48.0f, 0.01f),
                                             ebs::readabilityContourWidth (48.0f, 0.25f))
                       && juce::approximatelyEqual (ebs::readabilityContourWidth (48.0f, 99.0f),
                                                    ebs::readabilityContourWidth (48.0f, 4.0f)),
                   "readability contour weight is railed at 0.25..4");
        }
    }

    // v0.15.13 (the spine) + v0.15.14 (its caption is drawn as accent-free tracked capitals) + v0.15.15 (measured room names): a left vertical spine naming the frame,
    // clipping to the frame's own rounded corners and shrinking the title until it fits the band height.
    {
        const int w = 120, tallH = 170, shortH = 84;

        auto renderFrameTitle = [&] (const juce::String& title, int height)
        {
            ebs::FramedBody frame;
            frame.setSize (w, height);
            ebs::FrameTitleBand band;
            frame.addAndMakeVisible (band);
            band.setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), height);
            band.setTitle (title);

            juce::Image img (juce::Image::ARGB, w, height, true);
            juce::Graphics g (img);
            g.fillAll (juce::Colours::black);
            frame.paintEntireComponent (g, false);
            return img;
        };

        // Inside the band: the strip tint painted over the frame body, not a raw accent.
        const auto tint = renderToImage (w, tallH, [&] (juce::Graphics& g) { g.setColour (ebs::bgPanel()); g.fillRect (juce::Rectangle<int> (0, 0, w, tallH)); g.setColour (ebs::accent().withAlpha (0.10f)); g.fillRect (juce::Rectangle<int> (0, 0, w, tallH)); }).getPixelAt (10, 3);
        // The extreme corner is OUTSIDE the rounded path, so the square edge of the strip
        // must never show: black means the band followed the frame curve.
        // The SAME frame with no spine at all is the reference for what a frame paints by itself.
        auto renderPlainFrame = [&] (int height)
        {
            ebs::FramedBody plain;
            plain.setSize (w, height);
            return renderToImage (w, height, [&] (juce::Graphics& g)
            {
                plain.paintEntireComponent (g, false);
            });
        };

        const auto tall = renderFrameTitle ("Effects", tallH);
        const auto plainFrame = renderPlainFrame (tallH);

        checkPixel (tall, { 10, 3 }, tint, "the title band tints inside the frame body");
        // At the extreme corner the spine adds nothing of its own: what sits there is exactly what a
        // frame with no band puts there. A strip squaring off the curve would leave its own tint.
        checkPixel (tall, { 1, 1 }, plainFrame.getPixelAt (1, 1),
                    "the title band hugs the frame's rounded corner instead of squaring it off");

        // The text is drawn tracked apart at 50 % alpha, so no pixel of the strip can be the full-strength
        // palette colour; and ink cannot be counted against a palette colour any more. The reference
        // is the SAME band with an empty title: tint, separator and corner clip are identical there,
        // so every pixel that differs from it is glyph ink.
        auto renderBareBand = [&] (int height)
        {
            ebs::FramedBody frame;
            frame.setSize (w, height);
            ebs::FrameTitleBand band;
            frame.addAndMakeVisible (band);
            band.setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), height);

            juce::Image img (juce::Image::ARGB, w, height, true);
            juce::Graphics g (img);
            g.fillAll (juce::Colours::black);
            frame.paintEntireComponent (g, false);
            return img;
        };

        auto countTextInk = [&] (const juce::Image& img, const juce::Image& bare)
        {
            int n = 0;
            for (int x = 1; x < ebs::FrameTitleBand::bandWidth() - 2; ++x)   // the separator column stays out
                for (int y = 4; y < img.getHeight() - 4; ++y)
                    if (img.getPixelAt (x, y) != bare.getPixelAt (x, y))
                        ++n;
            return n;
        };

        const auto tallBare = renderBareBand (tallH);
        int fullStrength = 0;
        for (int x = 1; x < ebs::FrameTitleBand::bandWidth() - 2; ++x)
            for (int y = 4; y < tallH - 4; ++y)
                if (tall.getPixelAt (x, y) == ebs::text())
                    ++fullStrength;

        const int tallInk = countTextInk (tall, tallBare);
        std::printf ("[INFO] title band in a %d px spine: ink=%d pixels of the full-strength text colour\n",
                     tallH, tallInk, fullStrength);
        check (tallInk > 40 && fullStrength == 0,
               "the vertical title paints readable glyph cores at reduced strength, never the bright palette colour");

        // The separator is the frame's own trim, drawn on the band's inner edge.
        int sepPixels = 0;
        for (int y = 8; y < tallH - 4; ++y)
            for (int x = ebs::FrameTitleBand::bandWidth() - 3;
                 x < ebs::FrameTitleBand::bandWidth(); ++x)
                if (tall.getPixelAt (x, y) == ebs::panelBorder())
                    ++sepPixels;
        std::printf ("[INFO] separator pixels in the last columns: %d (band height %d)\n", sepPixels, tallH);
        check (sepPixels > tallH / 2, "the band keeps its separator line over the frame trim");

        // The fit is measured, not guessed: a long title costs length along the HEIGHT, never width.
        const juce::String shortTitle ("Effects"), longTitle ("Key & Scale Detection");
        const int shortLen = ebs::bandTextLength (shortTitle, ebs::kBandFontSize);
        const int longLen  = ebs::bandTextLength (longTitle, ebs::kBandFontSize);
        const int minLen   = ebs::bandTextLength (longTitle, ebs::kBandMinFontSize);
        std::printf ("[INFO] measured band lengths: short=%d long@13pt=%d long@9pt=%d width=%d\n",
                     shortLen, longLen, minLen, ebs::FrameTitleBand::bandWidth());
        const int shortBudget = shortH - 6, tallBudget = tallH - 6;   // spine height minus the pad at both ends
        std::printf ("[INFO] tracked fit budget: %d px in a %d px spine, %d px in a %d px row\n",
                     shortBudget, shortH, tallBudget, tallH);
        check (longLen > shortLen && longLen > shortBudget,
               "a longer title costs length along the spine and forces the shrink step in a short frame");

        // Tracking eats into the budget, so the fit promise is made for what the product SHIPS. Every frame name
        // the app can display keeps all its letters at the smallest size inside the height the bottom band gives
        // a row; measured per name against the real budget, never assumed. The synthetic stress title above is
        // deliberately longer than any shipped caption and exists to prove the shrink step runs - the spine cannot
        // hold an unbounded caption, which is why titles are short by design (measured limitation, printed below).
        const juce::StringArray shippedTitles = { "Correction", "Key & Scale", "Effects", "Reverb", "Harmony" };

        for (int i = 0; i < shippedTitles.size(); ++i)
            check (ebs::bandTextLength (shippedTitles[i], ebs::kBandMinFontSize) <= tallBudget,
                   "every shipped frame name keeps every letter at the minimum size inside a real row");

        std::printf ("[INFO] limitation measured: stress title needs %d px at min size vs %d px available in a short spine\n",
                     minLen, shortBudget);
        check (minLen > shortBudget,
               "a caption longer than any shipped name loses its last letters in a short spine - titles stay short by design");

        // The shrunk render keeps ink inside the spine: nothing clipped away to an empty band.
        const auto shortImg = renderFrameTitle (longTitle, shortH);
        const int shortInk = countTextInk (shortImg, renderBareBand (shortH));
        std::printf ("[INFO] title band ink in a %d px spine with a long title: %d\n",
                     shortH, shortInk);
        check (shortInk > 40 && ebs::bandTextLength (longTitle, ebs::kBandFontSize) > shortH - 6,
               "the band shrinks its glyphs in a short frame and keeps readable glyph cores");

        // Width is locale-independent by construction: the text runs along the height. The spine width is
        // now measured from the UPPERCASE bold glyphs themselves, so a narrow column is provably cleared.
        const juce::StringArray spineTitles = { "Correction", "Key & Scale", "Effects", "Reverb", "Harmony", longTitle };

        int widest13 = 0, widest9 = 0;
        for (int i = 0; i < spineTitles.size(); ++i)
        {
            const auto t = spineTitles[i];
            std::printf ("[INFO] spine title %d: length@13pt=%d thickness@13pt=%d thickness@9pt=%d\n",
                         i + 1, ebs::bandTextLength (t, ebs::kBandFontSize),
                         ebs::bandGlyphThickness (t, ebs::kBandFontSize),
                         ebs::bandGlyphThickness (t, ebs::kBandMinFontSize));
            widest13 = juce::jmax (widest13, ebs::bandGlyphThickness (t, ebs::kBandFontSize));
            widest9  = juce::jmax (widest9,  ebs::bandGlyphThickness (t, ebs::kBandMinFontSize));
        }

        std::printf ("[INFO] widest uppercase bold glyphs: %d px at 13pt, %d px at 9pt; band asks %d px\n",
                     widest13, widest9, ebs::FrameTitleBand::bandWidth());
        check (widest13 + 2 <= ebs::FrameTitleBand::bandWidth()
                   && ebs::FrameTitleBand::bandWidth() <= widest13 + 6
                   && widest13 < 26,
               "the spine is measured from its own glyphs: at least one pixel clear at each edge, and no wider than the old band");

        check (ebs::bandTextLength ("Effet", ebs::kBandFontSize) < longLen,
               "a longer locale costs length along the spine, never more band width");

        // Tracking is measured, not assumed: the fit check must return a LONGER length than the glyphs alone,
        // by exactly spacing * (n - 1), so the size the band picks fits what it actually draws.
        const int tracked = ebs::bandTextLength ("REVERB", ebs::kBandFontSize);
        const int tight = juce::roundToInt (juce::GlyphArrangement::getStringWidth (ebs::createFont (ebs::kBandFontSize, true), "REVERB"));

        std::printf ("[INFO] tracking: tracked=%d px, glyphs alone=%d px, gap %g px between letters\n",
                     tracked, tight, ebs::kBandLetterSpacing);
        check (tracked > tight + 3 && ebs::bandGlyphThickness ("REVERB", ebs::kBandFontSize) == widest13,
               "letters are tracked apart along the spine, and tracking never thickens the glyphs across it");

        // A title-less band is a quiet strip: tint and separator, no glyph ink.
        const auto bare = renderFrameTitle (juce::String(), tallH);
        check (countTextInk (bare, tallBare) == 0 && bare.getPixelAt (10, 3) == tint,
               "an empty title leaves the strip without glyphs");

        // Disabled reads dimmed, not invisible.
        ebs::FramedBody disFrame;
        disFrame.setSize (w, tallH);
        ebs::FrameTitleBand disBand;
        disFrame.addAndMakeVisible (disBand);
        disBand.setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), tallH);
        disBand.setTitle ("Effects");
        disBand.setEnabled (false);
        juce::Image disImg (juce::Image::ARGB, w, tallH, true);
        juce::Graphics dg (disImg);
        dg.fillAll (juce::Colours::black);
        disFrame.paintEntireComponent (dg, false);
        int dimInk = 0;
        for (int x = 1; x < ebs::FrameTitleBand::bandWidth() - 2; ++x)
            for (int y = 4; y < tallH - 4; ++y)
                if (disImg.getPixelAt (x, y) == ebs::textDim())
                    ++dimInk;
        // What a frame WITHOUT a spine puts at those pixels is the baseline; anything the band adds is ink.
        const auto disPlain = renderPlainFrame (tallH);
        int anyInk = 0;
        for (int x = 1; x < ebs::FrameTitleBand::bandWidth() - 2; ++x)
            for (int y = 4; y < tallH - 4; ++y)
                if (disImg.getPixelAt (x, y) != disPlain.getPixelAt (x, y))
                    ++anyInk;

        int disBright = 0;
        for (int x = 1; x < ebs::FrameTitleBand::bandWidth() - 2; ++x)
            for (int y = 4; y < tallH - 4; ++y)
                if (disImg.getPixelAt (x, y) == ebs::text())
                    ++disBright;

        std::printf ("[INFO] disabled band: dimmed-exact %d, bright %d, changed %d\n",
                     dimInk, disBright, anyInk);
        // A faded component is rendered through a transparency layer, so an exact palette colour never
        // survives: what proves the state is ink present with NO pixel of the bright text colour.
        check (anyInk > 120 && disBright == 0,
               "a disabled band reads dimmed, never bright");

         // The locales OpenVoxTuner ships are the real stress test: French is long accented Latin, Japanese and
         // Chinese are ideograph runs. A standalone launched from patched settings does not apply the persisted
         // language on this machine, so locale rendering CANNOT be proven through a live screenshot - it has to be
         // proven here, in the component itself, at the height the bottom row gives a frame (170 px).
         // The caption is drawn WITHOUT accents: French and Spanish set all-caps headings without accent
         // marks, so the spine follows that convention. Checked on the titles OVT really ships (French,
         // Spanish), on German-style marks to prove the map covers them, and on ideographs to prove they
         // keep every glyph they need - nothing is dropped from a language that has no marks.
         
         
         
         
         check (ebs::bandCaps (u8 ({ 'R', 0xC3, 0xA9, 'v', 'e', 'r', 'b', 0xC3, 0xA9, 'r', 'a', 't', 'i', 'o', 'n' })) == "REVERBERATION"
                && ebs::bandCaps (u8 ({ 'T', 'o', 'n', 'a', 'l', 'i', 't', 0xC3, 0xA9, ' ', 'e', 't', ' ', 'g', 'a', 'm', 'm', 'e' })) == "TONALITE ET GAMME"
                && ebs::bandCaps (u8 ({ 'C', 'o', 'r', 'r', 'e', 'c', 'c', 'i', 0xC3, 0xB3, 'n' })) == "CORRECCION",
                "an accented locale title draws its caption as accent-free capitals");

         check (ebs::bandCaps (u8 ({ 'K', 0xC3, 0xBC, 'r' })) == "KUR" && ebs::bandCaps (u8 ({ 0xC3, 0x87, 'a' })) == "CA"
                && ebs::bandCaps (u8 ({ 'A', 0xC3, 0xB1, 'o' })) == "ANO" && ebs::bandCaps (u8 ({ 's', 't', 'r', 'a', 0xC3, 0x9F, 'e' })) == "STRASSE",
                "the drop covers the Latin marks a shipped locale can hand it, umlauts and cedilla included");

         check (ebs::bandCaps (u8 ({ 0xE3, 0x82, 0xAD, 0xE3, 0x83, 0xBC, 0xE3, 0x81, 0xA8, 0xE3, 0x82, 0xB9, 0xE3, 0x82, 0xB1, 0xE3, 0x83, 0xBC, 0xE3, 0x83, 0xAB })) == u8 ({ 0xE3, 0x82, 0xAD, 0xE3, 0x83, 0xBC, 0xE3, 0x81, 0xA8, 0xE3, 0x82, 0xB9, 0xE3, 0x82, 0xB1, 0xE3, 0x83, 0xBC, 0xE3, 0x83, 0xAB })
                && ebs::bandCaps (u8 ({ 0xE6, 0xA0, 0xB9, 0xE9, 0x9F, 0xB3, 0xE4, 0xB8, 0x8E, 0xE9, 0x9F, 0xB3, 0xE9, 0x98, 0xB6 })) == u8 ({ 0xE6, 0xA0, 0xB9, 0xE9, 0x9F, 0xB3, 0xE4, 0xB8, 0x8E, 0xE9, 0x9F, 0xB3, 0xE9, 0x98, 0xB6 }),
                "an ideograph title keeps every glyph: casing and accent rules touch Latin only");

         const juce::StringArray localeTitles = { u8 ({ 'T', 'o', 'n', 'a', 'l', 'i', 't', 0xC3, 0xA9, ' ', 'e', 't', ' ', 'g', 'a', 'm', 'm', 'e' }), u8 ({ 'R', 0xC3, 0xA9, 'v', 'e', 'r', 'b', 0xC3, 0xA9, 'r', 'a', 't', 'i', 'o', 'n' }), u8 ({ 0xE3, 0x82, 0xAD, 0xE3, 0x83, 0xBC, 0xE3, 0x81, 0xA8, 0xE3, 0x82, 0xB9, 0xE3, 0x82, 0xB1, 0xE3, 0x83, 0xBC, 0xE3, 0x83, 0xAB }), u8 ({ 0xE6, 0xA0, 0xB9, 0xE9, 0x9F, 0xB3, 0xE4, 0xB8, 0x8E, 0xE9, 0x9F, 0xB3, 0xE9, 0x98, 0xB6 }) };
         const int spineH = tallH - 6;   // the height budget the band's own shrink loop compares against

         for (int i = 0; i < localeTitles.size(); ++i)
        {
            const juce::String t = ebs::bandCaps (localeTitles[i]);   // exactly what the spine draws, accents dropped
            const int len13 = ebs::bandTextLength (t, ebs::kBandFontSize);
            const int len9  = ebs::bandTextLength (t, ebs::kBandMinFontSize);
            const int thick13 = ebs::bandGlyphThickness (t, ebs::kBandFontSize);
            const auto img = renderFrameTitle (localeTitles[i], tallH);   // the band upper-cases and de-accents itself
            const int ink = countTextInk (img, tallBare);
            std::printf ("[INFO] locale title %d: caption=%s length@13pt=%d length@9pt=%d budget=%d thickness@13pt=%d ink=%d\n",
                         i + 1, t.toRawUTF8(), len13, len9, spineH, thick13, ink);
            check (len9 <= spineH && ink > 40 && thick13 + 2 <= ebs::FrameTitleBand::bandWidth(),
                   "every shipped locale title fits the fixed band height, clears both spine edges and paints readable glyphs");
        }

         // v0.15.15: the consumer frame names its rooms LEAD and HARMONY and its ducking knob
         // DUCKING in every locale it ships. The consumer measures each column floor from these
         // exact strings (18 px power strip + label width at 14 pt + 2 px edge), so measuring them
         // here checks a rename against the fixed 74 px cell instead of eyeballing one language.
         const juce::StringArray leadNames = { u8 ({ 'L', 'e', 'a', 'd' }), u8 ({ 'L', 'e', 'a', 'd' }),
                                               u8 ({ 'L', 'e', 'a', 'd' }), u8 ({ 'L', 'e', 'a', 'd' }),
                                               u8 ({ 0xE3, 0x83, 0xAA, 0xE3, 0x83, 0xBC, 0xE3, 0x83, 0x89 }),
                                               u8 ({ 0xE4, 0xB8, 0xBB, 0xE5, 0x94, 0xB1 }) };
         const juce::StringArray harmonyNames = { u8 ({ 'H', 'a', 'r', 'm', 'o', 'n', 'y' }),
                                                  u8 ({ 'H', 'a', 'r', 'm', 'o', 'n', 'i', 'e' }),
                                                  u8 ({ 'H', 'a', 'r', 'm', 'o', 'n', 'i', 'e' }),
                                                  u8 ({ 'A', 'r', 'm', 'o', 'n', 0xC3, 0xA9, 'a', 's' }),
                                                  u8 ({ 0xE3, 0x83, 0x8F, 0xE3, 0x83, 0xBC, 0xE3, 0x83, 0xA2, 0xE3, 0x83, 0x8B, 0xE3, 0x83, 0xBC }),
                                                  u8 ({ 0xE5, 0x92, 0x8C, 0xE5, 0xA3, 0xB0 }) };
         const juce::StringArray duckNames = { u8 ({ 'D', 'u', 'c', 'k', 'i', 'n', 'g' }),
                                               u8 ({ 'D', 'u', 'c', 'k', 'i', 'n', 'g' }),
                                               u8 ({ 'D', 'u', 'c', 'k', 'i', 'n', 'g' }),
                                               u8 ({ 'D', 'u', 'c', 'k', 'i', 'n', 'g' }),
                                               u8 ({ 0xE3, 0x83, 0x80, 0xE3, 0x83, 0x83, 0xE3, 0x82, 0xAD, 0xE3, 0x83, 0xB3, 0xE3, 0x82, 0xA3 }),
                                               u8 ({ 0xE9, 0x97, 0xAA, 0xE9, 0x99, 0x8D }) };

         auto reverbHeaderNeed = [] (const juce::String& text) -> int
         { return 18 + static_cast<int> (ebs::fontToggleButton().getStringWidth (text)) + 2; };

         for (int i = 0; i < leadNames.size(); ++i)
         {
             const int leadNeed = reverbHeaderNeed (leadNames[i]);
             const int harmNeed = reverbHeaderNeed (harmonyNames[i]);
             const int duckNeed = reverbHeaderNeed (duckNames[i]);
             std::printf ("[INFO] reverb names %d: lead_need=%d harmony_need=%d ducking_need=%d cell_floor=74\n",
                          i + 1, leadNeed, harmNeed, duckNeed);
         }

         int widestLead = 0, widestHarmony = 0, widestDuck = 0;
         for (int i = 0; i < leadNames.size(); ++i)
         {
             widestLead    = juce::jmax (widestLead, reverbHeaderNeed (leadNames[i]));
             widestHarmony = juce::jmax (widestHarmony, reverbHeaderNeed (harmonyNames[i]));
             widestDuck    = juce::jmax (widestDuck, reverbHeaderNeed (duckNames[i]));
         }

         // Measured maxima (14 pt toggle font): lead 43 px, harmony 67 px (Spanish "Armonias"),
         // ducking 59 px - the widest thing any shipped locale asks for. Every one rides the fixed
         // 74 px cell, so the folded Reverb frame stays ONE column in all six languages and a fully
         // folded row keeps its width when a rename lands. Ideographs ask for LESS than Latin: JA
         // needs 54 and ZH 34, because two and three glyphs carry what four to eight latin letters say.
         check (widestLead <= 74 && widestHarmony <= 74 && widestDuck <= 74,
                "every shipped room name rides the fixed cell floor, so a rename cannot widen the Reverb frame");

         check (reverbHeaderNeed ("Harmony") > reverbHeaderNeed ("Lead"),
                "the stacked folded column is measured from the harmony name, exactly as the consumer stacks it");



        // v0.15.13 (the spine) + v0.15.14 (accent-free captions) + v0.15.15 (measured room names) receipt slot: three frames at the height a bottom row gives them, each
        // named along its left edge. The pixel checks above say the band hugs the curve; this is the
        // picture a human eye reads.
        juce::Image spineProof (juce::Image::ARGB, 300, 190, true);
        {
            juce::Component spineCanvas;
            spineCanvas.setSize (300, 190);

            ebs::FramedBody frameA, frameB, frameC;
            ebs::FrameTitleBand spineA, spineB, spineC;

            spineCanvas.addAndMakeVisible (frameA);
            frameA.setBounds (5, 10, 85, 170);
            spineCanvas.addAndMakeVisible (frameB);
            frameB.setBounds (105, 10, 85, 170);
            spineCanvas.addAndMakeVisible (frameC);
            frameC.setBounds (205, 10, 85, 170);

            spineA.setTitle ("Key & Scale");
            spineB.setTitle ("Effets");
            spineC.setTitle ("Reverb");

            for (int i = 0; i < 3; ++i)
            {
                auto* owner = (i == 0 ? &frameA : (i == 1 ? &frameB : &frameC));
                auto* bandSlot = (i == 0 ? &spineA : (i == 1 ? &spineB : &spineC));

                owner->addAndMakeVisible (*bandSlot);
                bandSlot->setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), 170);
            }

            juce::Graphics sg (spineProof);
            sg.fillAll (ebs::bgDark());
            spineCanvas.paintEntireComponent (sg, false);
        }

        const juce::File spineFile (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                        .getChildFile ("eiffelbs-title-band.png"));

        if (auto out = spineFile.createOutputStream())
        {
            juce::PNGImageFormat().writeImageToStream (spineProof, *out);
            out->flush();
        }
        std::printf ("[INFO] title spine receipt: %s\n", spineFile.getFullPathName().toRawUTF8());

        // Locale receipt: the same spine geometry carrying the four longest titles a locale can hand it. This is
        // the picture that proves "the band stays 17 px, the language pays in height and in accent-free capitals"
        // on paper, because the live app cannot be driven into a foreign language on this machine.
        juce::Image localeProof (juce::Image::ARGB, 400, 190, true);
        {
            juce::Component localeCanvas;
            localeCanvas.setSize (400, 190);

            ebs::FramedBody lframe[4];
            ebs::FrameTitleBand lspine[4];

            for (int i = 0; i < 4; ++i)
            {
                localeCanvas.addAndMakeVisible (lframe[i]);
                lframe[i].setBounds (5 + i * 100, 10, 90, 170);
                lspine[i].setTitle (localeTitles[i]);
                lframe[i].addAndMakeVisible (lspine[i]);
                lspine[i].setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), 170);
            }

            juce::Graphics lg (localeProof);
            lg.fillAll (ebs::bgDark());
            localeCanvas.paintEntireComponent (lg, false);
        }

        const juce::File localeFile (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                        .getChildFile ("eiffelbs-title-band-locales.png"));

        if (auto out = localeFile.createOutputStream())
        {
            juce::PNGImageFormat().writeImageToStream (localeProof, *out);
            out->flush();
        }
        std::printf ("[INFO] locale spine receipt: %s\n", localeFile.getFullPathName().toRawUTF8());
    }

    // --- TokenField: the allowed set is the only set ----------------------
    // A field of ids in free text accepts every typo and every name the build
    // does not provide, and the failure lands later, somewhere else. Here the
    // host's list is the only list: measured on the pure helpers a keyboard
    // would drive, plus the keep-unknown rule a hand-edited file needs.
    {
        ebs::TokenField tf;
        tf.setSize (420, 26);

        std::vector<ebs::TokenField::Token> allowed
        {
            { "music.description", "the Music Description box" },
            { "music.duration",    "the Music tab's Duration, in seconds" },
            { "assistant.input",   "the chat input box" }
        };
        tf.setAllowedTokens (allowed);

        check (tf.suggestionsFor ("").size() == 3,
               "an empty fragment offers every allowed value");
        check (tf.suggestionsFor ("dur") == juce::StringArray { "music.duration" },
               "typing filters to the matching value");
        check (tf.suggestionsFor ("zzz").isEmpty(),
               "a fragment nothing matches offers nothing");

        check (! tf.addToken ("bogus"),
               "a value the host does not allow cannot be added");
        check (tf.addToken ("music.duration"),
               "an allowed value is added");
        check (! tf.addToken ("music.duration"),
               "... and never lands twice");
        check (! tf.addToken ("MUSIC.DURATION"),
               "... the same value in another case is the same value");
        check (tf.tokens().size() == 1 && tf.tokens()[0] == "music.duration",
               "the token list carries the canonical spelling");
        check (! tf.suggestionsFor ("").contains ("music.duration"),
               "what is already held is not offered again");
        check (tf.removeToken ("music.duration"), "a chip removes");
        check (! tf.removeToken ("music.duration"),
               "... and removal is honest about a value it does not hold");

        int changes = 0;
        tf.onTokensChanged = [&changes] { ++changes; };
        tf.addToken ("assistant.input");
        tf.removeToken ("assistant.input");
        check (changes == 2, "every edit notifies once");

        // The chips WRAP: a narrow field is TALLER, never a field that
        // hides the tokens that do not fit.
        {
            ebs::TokenField wrap;
            wrap.setSize (300, 28);
            wrap.setAllowedTokens (allowed);
            for (const auto& t : { "music.description", "music.duration",
                                   "assistant.input" })
                wrap.addToken (t);
            const int wide = wrap.heightForWidth (600);
            const int narrow = wrap.heightForWidth (200);
            check (wide <= 32,
                   "one chip row measures one row (chip height + field padding)");
            check (narrow > wide,
                   "a narrow field asks for more HEIGHT instead of hiding a chip");
            check (wrap.rowsForWidth (600) == 1 && wrap.rowsForWidth (200) > 1,
                   "rowsForWidth is the same simulation the layout runs");
        }

        tf.setTokens ({ "music.description", "assistant.time" });
        check (tf.tokens().size() == 2
                   && tf.tokens().contains ("assistant.time"),
               "setTokens KEEPS an unknown value - a hand-edited file loses nothing");
        check (changes == 2, "setTokens is a load, not an edit");
    }

    // --- v0.18.0 TokenField FREE-TEXT mode (tag fields) --------------------
    // A tag field is the opposite contract: the allowed set is only a
    // SUGGESTION list (the tags already in use), and the reader must be able
    // to create a new one. The enum-like default above must stay unchanged.
    {
        ebs::TokenField tags;
        tags.setSize (420, 26);
        tags.setAllowFreeText (true);
        tags.setAllowedTokens ({ { "door", "" }, { "rain", "" } });

        check (tags.allowsFreeText(), "free-text mode reports itself");
        check (tags.suggestionsFor ("").size() == 2,
               "existing tags are still offered as suggestions");
        check (tags.addToken ("impact"),
               "a NEW tag is created in free-text mode");
        check (! tags.addToken ("Impact"),
               "... and the duplicate check is case-insensitive");
        check (tags.addToken ("rain") && tags.tokens().size() == 2,
               "an existing suggestion still adds normally");

        tags.normaliseFreeText = [] (const juce::String& s)
        { return s.trim().toLowerCase(); };
        check (tags.addToken ("  Metal  ")
                   && tags.tokens().contains ("metal"),
               "normaliseFreeText is applied to a created tag");
        check (tags.suggestionsFor ("").size() == 1,
               "held tags are not offered again");
        check (tags.removeToken ("impact") && tags.tokens().size() == 2,
               "a created chip removes like any other");

        ebs::TokenField strict;
        strict.setSize (200, 26);
        strict.setAllowedTokens ({ { "a", "" } });
        check (! strict.addToken ("free") && strict.tokens().isEmpty(),
               "free text stays OFF unless the host opts in");
    }

        // v0.17.8 WrappedText: word wrap measured by font metrics, because a juce::Label in THIS build has no word
        // wrap at all - `setMultiLine` and `setWordWrapText` do not exist (the compile that first asked for wrapped
        // captions failed with C2039 on both), so a caption is left breaking by hand for one width, which lies the
        // moment the window shrinks, or running past the edge in silence.
        {
            ebs::WrappedText block;
            const juce::String para = "Plain LRC stamps whole lines; SRT and VTT bring line windows. Enhanced LRC "
                                      "brings word times for the words it tags. Nothing is invented for a word the "
                                      "file does not stamp.";

            block.setText (para);

            const int wide = 400, narrow = 170;
            const int oneLineH = juce::roundToInt (ebs::fontBody().getHeight());
            const int hWide   = block.requiredHeightForWidth (wide);
            const int hNarrow = block.requiredHeightForWidth (narrow);

            check (block.linesForWidth (wide) > 1 && block.linesForWidth (narrow) > block.linesForWidth (wide),
                   "the same text needs more lines at a narrow width");
            check (hNarrow > hWide && hWide > oneLineH,
                   "a narrower width asks for more HEIGHT instead of clipping the sentence");

            // The component's number is the layout's own arithmetic: rebuild the attributed string with the same
            // font, colour, wrap and leading, and compare heights exactly. A host laying out from a guess would
            // fail this on the first resize.
            juce::AttributedString as (para);
            as.setFont (ebs::fontBody());
            as.setColour (ebs::text());
            as.setWordWrap (juce::AttributedString::WordWrap::byWord);
            as.setJustification (juce::Justification::topLeft);
            as.setLineSpacing (ebs::kTextLeading);

            juce::TextLayout ref;
            ref.createLayout (as, static_cast<float> (narrow));

            check (hNarrow == juce::roundToInt (ref.getHeight()),
                   "requiredHeightForWidth is the measured layout height, not an estimate");

            bool inside = true;

            for (int i = 0; i < ref.getNumLines(); ++i)
                if (static_cast<int> (std::ceil (ref.getLine (i).getLineBoundsX().getEnd())) > narrow)
                    inside = false;

            check (inside, "no wrapped line runs past the width it was measured against");

            // A hard break written by the host is a real line: a report joins its entries that way.
            ebs::WrappedText rows;
            rows.setText ("first entry\nsecond entry");
            check (rows.linesForWidth (600) == 2 && rows.requiredHeightForWidth (600) > oneLineH,
                   "hard breaks survive wrapping and both lines get room");

            // Truncation is loud: the kept tail carries the mark, every kept line fits inside the width, and the
            // room shrinks to what is shown - never full-height space hiding text nobody can read.
            const auto ellipsis = juce::String::charToString (0x2026);
            block.setMaxLines (2);

            const int hTrunc = block.requiredHeightForWidth (narrow);
            const auto shown = block.textForWidth (narrow);

            check (block.linesForWidth (narrow) == 2, "the truncated caption paints exactly the allowed lines");
            check (hTrunc < hNarrow, "kept short means a shorter room, not hidden text in tall space");
            check (shown.contains (ellipsis), "the cut is marked on the last kept line");

            bool tailFits = true;

            for (const auto& l : juce::StringArray::fromTokens (shown, "\n", {}))
                if (ebs::fontBody().getStringWidth (l) > narrow)
                    tailFits = false;

            check (tailFits, "the mark never pushes a kept line past the width it was told to keep");

            auto countColour = [&] (const juce::Image& img, const juce::Colour& c)
            {
                int n = 0;

                for (int x = 0; x < img.getWidth(); ++x)
                    for (int y = 0; y < img.getHeight(); ++y)
                        if (img.getPixelAt (x, y) == c)
                            ++n;

                return n;
            };

            block.setSize (narrow, hTrunc);

            const auto inked = renderOnClear (narrow, hTrunc, [&] (juce::Graphics& g) { block.paint (g); });
            check (inkPixels (inked) > 60 && countColour (inked, ebs::text()) > 20,
                   "the truncated caption paints its kept lines at full strength");

            // Disabled dims the fill and touches no word.
            block.setEnabled (false);

            const auto dimmed = renderOnClear (narrow, hTrunc, [&] (juce::Graphics& g) { block.paint (g); });
            check (inkPixels (dimmed) > 60 && countColour (dimmed, ebs::text()) == 0,
                   "a disabled caption dims its text without moving a single word");

            // Empty text asks for no room: nothing is invented where the host has nothing to say.
            ebs::WrappedText none;
            check (none.requiredHeightForWidth (200) == 0 && none.linesForWidth (200) == 0,
                   "empty text measures zero lines and zero height");

            // A word wider than the line is never cut mid-glyph: it keeps its whole shape on a line of its own and
            // asks for room, so nothing disappears at a width where a Label would have clipped it in silence.
            ebs::WrappedText longWord;
            longWord.setText ("incomprehensibilities demonstratepermeability");
            const int tight = 40;

            check (longWord.linesForWidth (tight) == 2
                       && longWord.textForWidth (tight).contains ("incomprehensibilities")
                       && longWord.requiredHeightForWidth (tight) > oneLineH,
                   "a word wider than the line stays whole on its own line and asks for height, never mid-glyph");
        }

        // v0.17.8 receipt: BOTH bands in one picture - a frame named on its LEFT spine, the way every existing host
        // names its rooms, and a wide short dialog frame named along its TOP edge. A human eye reads this to check
        // that adding the header band changed nothing for the left band; the pixel checks above say what each is
        // made of.
        {
            const int pw = 320, ph = 190;

            // This receipt is rendered by the file's own helper: a template image, pre-filled black, painted by
            // components inside ONE Graphics object. What this work observed about writing such pictures to PNG is
            // recorded honestly here, because it decides how this proof must be read: painting the same component tree
            // into an ordinary ARGB image in separate Graphics scopes gives identical picture data whose written file
            // reads A=0 everywhere; one run built by renderToImage wrote an opaque PNG (A=255, exact palette colours)
            // and another run of the same route wrote A=0. The evidence that holds is therefore the checks and the
            // printed pixel values - a viewable file for this composite is not guaranteed on this machine. Colour
            // references must be built in the SAME image format as the proof: a template image keeps a 1-bit alpha
            // stencil, so its blended colours land at different bytes than an ordinary ARGB image's, and equality
            // across formats fails for reasons foreign to the band.

            juce::Component headCanvas;
            headCanvas.setSize (pw, ph);

            ebs::FramedBody leftFrame, topFrame;
            ebs::FrameTitleBand spine;
            ebs::FrameTitleHeader header;

            headCanvas.addAndMakeVisible (leftFrame);
            leftFrame.setBounds (10, 10, 300, 70);
            headCanvas.addAndMakeVisible (topFrame);
            topFrame.setBounds (10, 95, 300, 85);

            spine.setTitle ("Reverb");
            header.setTitle ("Detection de tonalite");

            leftFrame.addAndMakeVisible (spine);
            spine.setBounds (0, 0, ebs::FrameTitleBand::bandWidth(), 70);   // the spine rides its own frame's height
            topFrame.addAndMakeVisible (header);
            header.setBounds (0, 0, 300, ebs::bandHeaderHeight());

            juce::Image headProof = renderToImage (pw, ph, [&topFrame, &leftFrame] (juce::Graphics& g)
            {
                // Each frame is painted as its own root, at the place the canvas assigns it; a headless canvas root
                // paints only itself, so painting the canvas would show two empty bodies.
                {
                    juce::Graphics::ScopedSaveState ss (g);
                    g.setOrigin (10, 95);
                    topFrame.paintEntireComponent (g, false);   // the dialog frame named along its TOP edge
                }

                {
                    juce::Graphics::ScopedSaveState ss (g);
                    g.setOrigin (10, 10);
                    leftFrame.paintEntireComponent (g, false);  // the same voice on its LEFT spine, untouched by this
                }
            });

            // The picture must show BOTH bands: the same tint the spine paints inside a frame body, laid along the
            // top edge under its own hairline. A proof that stays the canvas colour is the honest way to catch "the
            // component never painted".
            const int bh = ebs::bandHeaderHeight();

            // The reference strip is built in this proof's own template format, for the reason recorded above.
            juce::Image headRef (juce::Image::ARGB, 300, bh, true);
            {
                juce::Graphics rg (headRef);
                rg.setColour (ebs::bgPanel());          rg.fillRect (juce::Rectangle<int> (0, 0, 300, bh));
                rg.setColour (ebs::accent().withAlpha (0.10f)); rg.fillRect (juce::Rectangle<int> (0, 0, 300, bh));
            }

            const auto headTint = headRef.getPixelAt (30, bh - 2);

            checkPixel (headProof, { 30, 95 + bh - 2 }, headTint,
                        "the receipt shows the top band's tint inside its own frame body");
            checkPixel (headProof, { 160, 95 + bh - 1 }, ebs::panelBorder(),
                        "the receipt shows the band's hairline closing its bottom edge");

            int headInk = 0;

            for (int x = 20; x < 300; ++x)
                for (int y = 95; y < 95 + bh - 1; ++y)   // above the hairline, inside the strip
                    if (headProof.getPixelAt (x, y) != headTint
                        && headProof.getPixelAt (x, y) != ebs::bgPanel()
                        && headProof.getPixelAt (x, y) != ebs::bgDark())   // the canvas colour is not ink
                        ++headInk;

            check (headInk > 100, "the receipt shows a caption painted on the top band, not an empty strip");

            std::printf ("[INFO] receipt pixels: fill(2,2)=#%08x left body(150,40)=#%08x band tint(30,%d)=#%08x hairline(160,%d)=#%08x\n",
                         headProof.getPixelAt (2, 2).getARGB(),
                         headProof.getPixelAt (150, 40).getARGB(),
                         95 + bh - 2, headProof.getPixelAt (30, 95 + bh - 2).getARGB(),
                         95 + bh - 1, headProof.getPixelAt (160, 95 + bh - 1).getARGB());

            const juce::File headFile (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                          .getChildFile ("eiffelbs-title-header.png"));

            if (auto out = headFile.createOutputStream())
            {
                juce::PNGImageFormat().writeImageToStream (headProof, *out);
                out->flush();
            }
            std::printf ("[INFO] top band receipt: %s\n", headFile.getFullPathName().toRawUTF8());
        }

        // v0.17.8 FrameTitleHeader: a TOP title band - the spine's caption voice laid along a frame's top edge,
        // additive to FrameTitleBand (the left spine is untouched, and every host naming its frames on the left
        // keeps doing exactly that). A dialog is wide and short, so the band spends WIDTH, the room it has.
        {
            const int w = 320;
            const int h = ebs::bandHeaderHeight();

            auto renderTopBand = [&] (const juce::String& title)
            {
                ebs::FramedBody frame;
                frame.setSize (w, h + 40);
                ebs::FrameTitleHeader head;
                frame.addAndMakeVisible (head);
                head.setBounds (0, 0, w, h);
                head.setTitle (title);

                juce::Image img (juce::Image::ARGB, w, h + 40, true);
                juce::Graphics g (img);
                g.fillAll (juce::Colours::black);
                frame.paintEntireComponent (g, false);
                return img;
            };

            // The tint the band paints over a frame body, computed exactly as the paint does it: bgPanel under an
            // accent wash at 10 %. Compared as a colour, not against the palette.
            const auto tint = renderToImage (w, h, [&] (juce::Graphics& g)
            {
                g.setColour (ebs::bgPanel());          g.fillRect (juce::Rectangle<int> (0, 0, w, h));
                g.setColour (ebs::accent().withAlpha (0.10f)); g.fillRect (juce::Rectangle<int> (0, 0, w, h));
            }).getPixelAt (30, h - 2);   // inside the band, left of where a caption sits

            const auto withTitle = renderTopBand ("Detection de tonalite");
            const auto bare      = renderTopBand ("");

            ebs::FramedBody plain;
            plain.setSize (w, h + 40);
            const auto plainFrame = renderToImage (w, h + 40, [&] (juce::Graphics& g)
            {
                plain.paintEntireComponent (g, false);
            });

            check (h == ebs::FrameTitleHeader::bandHeight() && h > 8,
                   "the top band asks for its measured height, not a number picked by eye");
            checkPixel (withTitle, { 30, h - 2 }, tint, "the top band tints inside the frame body");
            checkPixel (withTitle, { w / 2, h - 1 }, ebs::panelBorder(),
                        "a solid hairline closes the band's bottom edge, where the name stops and content begins");
            // The extreme corner stays exactly what a plain frame paints there: a strip squaring off the shared
            // curve would leave its own tint on the corner pixel.
            check (withTitle.getPixelAt (1, 1) == plainFrame.getPixelAt (1, 1),
                   "the top band hugs the frame's rounded corner instead of squaring it off");

            auto countHeadInk = [&] (const juce::Image& img, const juce::Image& ref)
            {
                int n = 0;

                for (int x = 4; x < w - 4; ++x)
                    for (int y = 2; y < h - 2; ++y)   // the hairline row stays out
                        if (img.getPixelAt (x, y) != ref.getPixelAt (x, y))
                            ++n;

                return n;
            };

            const int headInk = countHeadInk (withTitle, bare);

            int fullStrength = 0;

            for (int x = 4; x < w - 4; ++x)
                for (int y = 2; y < h - 2; ++y)
                    if (withTitle.getPixelAt (x, y) == ebs::text())
                        ++fullStrength;

            check (headInk > 100, "the caption paints readable glyphs inside the band");
            check (fullStrength == 0,
                   "the name sits at half strength, so a header never shouts over its content");

            // The height is fixed by measurement across locales: glyphs shrink when a long translated name does
            // not fit, and smaller glyphs are THINNER, which is what keeps one measured number cleared. The
            // shipped OpenTimbre captions (accents dropped as the band draws them) must each fit the narrowest
            // dialog width the app gives a band, and the tallest glyph the ladder can start from must clear pads.
            const juce::StringArray headLocales = { u8 ({ 'T', 'o', 'n', 'a', 'l', 'i', 't', 0xC3, 0xA9, ' ',
                                                          'e', 't', ' ', 'g', 'a', 'm', 'm', 'e' }),
                                                    u8 ({ 'R', 0xC3, 0xA9, 'v', 'e', 'r', 'b', 0xC3, 0xA9, 'r',
                                                          'a', 't', 'i', 'o', 'n' }),
                                                    u8 ({ 'E', 'x', 'p', 'o', 'r', 't', 'a', 't', 'i', 'o',
                                                          'n', ' ', 'd', 'e', 's', ' ', 'f', 'i', 'c', 'h',
                                                          'i', 'e', 'r', 's' }),
                                                    u8 ({ 0xE3, 0x82, 0xAD, 0xE3, 0x83, 0xBC, 0xE3, 0x81, 0xA8,
                                                          0xE3, 0x82, 0xB9, 0xE3, 0x82, 0xB1, 0xE3, 0x83, 0xBC,
                                                          0xE3, 0x83, 0xAB }) };
            const int tightW = 170;   // a band the width of a small dialog's content

            for (int i = 0; i < headLocales.size(); ++i)
            {
                const auto caps = ebs::bandCaps (headLocales[i]);
                const int len9  = ebs::bandTextLength (caps, ebs::kBandMinFontSize);
                const int thick13 = ebs::bandGlyphThickness (caps, ebs::kBandFontSize);

                std::printf ("[INFO] top band locale %d: caption=%s length@9pt=%d budget=%d thickness@13pt=%d height=%d\n",
                             i + 1, caps.toRawUTF8(), len9, tightW - 2 * juce::roundToInt (ebs::kBandHeaderPad),
                             thick13, h);

                check (len9 <= tightW - 2 * juce::roundToInt (ebs::kBandHeaderPad)
                           && thick13 + 2 * juce::roundToInt (ebs::kBandHeaderPad) <= h,
                       "a long translated name shrinks inside the fixed band height at a narrow width");
            }

            // The empty title is allowed to be silent: tint and hairline only, exactly as the spine paints when
            // a frame has no name.
            int bareDiff = 0;

            for (int x = 4; x < w - 4; ++x)
                for (int y = 2; y < h - 2; ++y)
                    if (bare.getPixelAt (x, y) != plainFrame.getPixelAt (x, y))
                        ++bareDiff;

            // Nothing outside the strip may move when a band has no name: tint and trim only, exactly as the spine
            // paints silent when a frame has nothing to say.
            const int stripArea = (w - 2 * juce::roundToInt (ebs::frameCornerRadius)) * h;

            check (bareDiff > 0 && bareDiff <= stripArea,
                   "an empty band paints its tint and trim inside the curve, and nothing outside them");
        }

    std::printf ("\neiffelbs-ui smoke: %s (%d failure%s)\n",
                 failures == 0 ? "PASS" : "FAIL",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
