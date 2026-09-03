# eiffelbs-ui

Shared JUCE library implementing the **EiffelBS design system**: the dark/light
palette, typography helpers, the application-wide `LookAndFeel` and a small set
of generic widgets — extracted from [OpenVoxTuner](https://github.com/EiffelBS/OpenVoxTuner)
(origin) and [OpenTimbre](https://github.com/EiffelBS/OpenTimbre) so every new
JUCE project consumes ONE versioned implementation instead of a per-app copy.

## Contents

| Header | Provides |
|---|---|
| `Theme.h` | `ebs::Theme` mode + `currentTheme()` / `isDark()`; palette `bgDark`, `bgPanel`, `accent`, `accentSoft`, `text`, `textDim`, `headerAccent`, `panelBorder`; shared constants `vizBg`, `outputColour`, `danger`; always-dark viz-chrome tokens (`grid`, `scaleLine`, `rulerBg`, `curveGrid`, `vizHeaderBg`, `vizHeaderAccent`, `vizLegendBg`, `cpuBg`, `cpuText`) |
| `Fonts.h` | Platform-correct typeface helpers (`createFont`, `createFontRaw`, `createMonospaceFont`) and named sizes (`fontTitle`, `fontSectionLabel`, `fontComboBox`, ...) |
| `LookAndFeel.h` | `ebs::LookAndFeel`: full custom painting (buttons incl. the `"primary"` ComponentID contract, rotary knobs, toggles, combos, editors, labels, tooltips, popups, tabs, scrollbars) + `drawGroupFrame` / `drawFramePlain` + shared `widgetColour()` three-level resolver + chrome `ThemeChromeIds` |
| `IconButton.h` | `ebs::IconButton`: vector icon button (procedural + SVG-flattened Feather/Material glyphs), framed chrome mode, play/stop toggle state, `ColourIds` overrides |
| `FramedBody.h` | `ebs::FramedBody`: drop-in rounded framed panel body |
| `Knob.h` | `ebs::Knob`: typed rotary slider (arc track, optional pivot-centred fill via `setCentredFill`/`"centred"` property, gradient cap) rendered by the shared LookAndFeel; colours on standard `juce::Slider` rotary ids through the three-level contract |
| `MorphSlider.h` | `ebs::MorphSlider`: slim pivot-centred horizontal track for blend/crossfade parameters (0..1), self-painting, themed built-ins |
| `PowerToggle.h` | `ebs::PowerToggle`: power-glyph toggle with warm glow when engaged, disabled dimming; `glowColourId` override |
| `DataList.h` | `ebs::DataList`: filterable/sortable table list over `juce::TableListBox` — `Row`/`Column`/`View` model, view+search+sort proxy, stable-id selection, per-row action column, progress cells, optional host cell components |
| `StatusBar.h` | `ebs::StatusBar` global log bar + single-instance floating `LogWindow` (thread-safe `logLine()`), `ColourIds` overrides |
| `CompatOt.h` | Transitional `namespace ot = ebs;` alias for codebases mid-migration |

## Colour overrides (v0.2.0)

Widgets expose JUCE-standard `ColourIds`. Resolution order:

1. per-instance `setColour (id, colour)` (standard JUCE),
2. theme-level hook — subclass `ebs::LookAndFeel` and override
   `widgetThemeColour (int colourId)` (return non-transparent to restyle
   every instance at once; default overrides nothing),
3. built-in shape-aware palette default (unchanged since v0.1).

Text buttons: setting `juce::TextButton::buttonColourId` **per instance**
turns that colour into the body tint across rest / hover / down / toggled
states, so translucent "chip" buttons keep their tinted look while sharing
the common accent hover outline (the 1 px liseret). Without an explicit
colour, buttons use the built-in form-style fills (`bgDark` body,
`panelBorder` edge).

```cpp
// one-off:
delBtn.setColour (ebs::IconButton::iconColourId, juce::Colours::red);

// whole-app theme tweak:
struct MyLnf : ebs::LookAndFeel
{
    juce::Colour widgetThemeColour (int id) override
    {
        if (id == ebs::StatusBar::backgroundColourId)
            return juce::Colour (0xff101216);
        return {};
    }
};
```

## Theme switching (v0.4)

Beyond assigning `ebs::currentTheme()` directly, apps get a broadcast
mechanism so per-instance colours re-apply themselves:

```cpp
struct MyPanel : juce::Component, ebs::ThemeSubscriber
{
    MyPanel()     { ebs::subscribeTheme (this); }
    ~MyPanel() override { ebs::unsubscribeTheme (this); }
    void themeChanged() override { reapplyMyColours(); repaint(); }
};

ebs::setTheme (ebs::Theme::Light);   // palette swap + notify all subscribers
```

`ebs::LookAndFeel` subscribes itself: one `setTheme` call refreshes the
shared LookAndFeel AND every subscribed component - no centralised
re-apply walker needed. Dispatch is UI-thread only (same contract as
`currentTheme()`).

**v0.4.1:** the built-in checkbox well default is theme-aware. Under Dark
it stays the studio canvas tone; under Light it turns white with the
theme-aware outline so the accent tick keeps its contrast. Apps that
override `checkboxFillColourId` per instance (or through
`widgetThemeColour`) keep priority under the 3-level contract.

## Requirements

- JUCE 8 (developed against **8.0.8**)
- C++20, CMake 3.22+
- The consumer links `juce::juce_gui_basics`; the library never pulls JUCE in
  by itself.

## Consumption (FetchContent, pinned tag)

```cmake
include(FetchContent)
FetchContent_Declare(eiffelbs-ui
    GIT_REPOSITORY https://github.com/EiffelBS/eiffelbs-ui.git
    GIT_TAG        v0.4.1)              # pin tags; breaking changes bump major
FetchContent_MakeAvailable(eiffelbs-ui)

target_link_libraries(my_app PRIVATE
    eiffelbs::ui                        # headers + ABI-critical JUCE defines
    juce::juce_gui_basics)              # JUCE comes from your own setup
```

```cpp
#include <eiffelbs/eiffelbs.h>

ebs::LookAndFeel lnf;                              // one instance per process/editor
juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

auto button = juce::TextButton ("Generate");
button.setComponentID ("primary");                 // accent-filled key action

ebs::currentTheme() = ebs::Theme::Light;           // live palette switch...
lnf.refreshThemeColours();                         // ...then re-apply to the L&F
```

The INTERFACE target exports the ABI-critical globals
(`JUCE_STRING_UTF_TYPE=8`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`). Every
binary in the process must agree on these or `juce::String` becomes an ODR
violation — keep your own JUCE definitions consistent with them.

## Smoke test

Standalone offscreen proof: constructs the real widgets, paints through the
real LookAndFeel into software images, asserts pixel-level outcomes and writes
a PNG receipt to `%TEMP%\eiffelbs-ui-smoke.png`. Exit code 0 = PASS.

```sh
cmake -S . -B build -DEIFFELBS_UI_BUILD_SMOKE_TEST=ON ^
      -DEIFFELBS_UI_JUCE_DIR="C:/path/to/JUCE"   # optional: reuse a local checkout
cmake --build build --config Release
build\Release\eiffelbs-ui-smoke.exe
```

Without `EIFFELBS_UI_JUCE_DIR` the test fetches JUCE 8.0.8 (shallow clone).

## Versioning contract

- Public tokens (`ebs::*` names, colour function names, font names, widget
  behaviour) are stable within a major version.
- Breaking change = major bump (`v1.0.0` → `v2.0.0`).
- Additions (new widgets, ColourIds hooks) = minor bump.

## Roadmap

- **v0.1** — extraction: palette + fonts + LookAndFeel + generic widgets,
  consumed side-by-side with the apps' local copies.
- **v0.2** — `ColourIds` on `IconButton` / `StatusBar` with the three-level
  resolution contract (instance > theme hook > built-in); consumer cutover
  done in OpenTimbre (local copies deleted, FetchContent pin).
- **v0.2.1** — `IconButton::Shape::lock` padlock glyph (stroke-style SVG,
  default tooltip "Link"); semantics are app-defined (e.g. host-transport
  link in OpenTimbre).
- **v0.3** — rotary `Knob` + painter, `MorphSlider`, `PowerToggle`
  (carried over from the OpenVoxTuner origin UI), always-dark viz-chrome
  palette tokens and chrome `ThemeChromeIds` (tab pill fill/text, bar rule,
  checkbox well) with built-ins frozen at their pre-v0.3 values; per-instance
  `TextButton::buttonColourId` chip fills under the shared hover rule.
  First consumers of the tag: OpenVoxTuner and its OpenVoxKey companion
  (FetchContent pin, local LookAndFeel deleted), while OpenTimbre stays on
  the stable v0.2.x line.
- **v0.4** — theme-switch broadcast: `setTheme()` + `ThemeSubscriber`
  (subscribe/unsubscribe), LookAndFeel self-subscribed and auto-refreshed on
  switch. First consumer: OpenVoxTuner light-theme completion series.
- **v0.8** — `DataList`: generic take/model-style lists (views, search,
  sortable columns, row actions, progress cells) over `juce::TableListBox`.
  First consumer: OpenTimbre takes + Settings model lists.
- **Next** — extend ColourIds coverage (`FramedBody`, favourite gold,
  LogWindow chrome); OpenVisuAI adoption of the widget set when its editor
  grows one.

## License

AGPLv3 — see [LICENSE](LICENSE). Extracted from OpenVoxTuner / OpenTimbre
(original copyright (C) 2026 EiffelBS).
