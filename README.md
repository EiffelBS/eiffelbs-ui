# eiffelbs-ui

Shared JUCE library implementing the **EiffelBS design system**: the dark/light
palette, typography helpers, the application-wide `LookAndFeel` and a small set
of generic widgets — extracted from [OpenVoxTuner](https://github.com/EiffelBS/OpenVoxTuner)
so every new JUCE project consumes ONE versioned implementation instead of a
per-app copy. Other EiffelBS applications consume the same tag; the public
surface below is what they all share.

## Contents

| Header | Provides |
|---|---|
| `Theme.h` | `ebs::Theme` mode + `currentTheme()` / `isDark()`; palette `bgDark`, `bgPanel`, `accent`, `accentSoft`, `text`, `textDim`, `headerAccent`, `panelBorder`; shared constants `vizBg`, `outputColour`, `danger`; always-dark viz-chrome tokens (`grid`, `scaleLine`, `rulerBg`, `curveGrid`, `vizHeaderBg`, `vizHeaderAccent`, `vizLegendBg`, `cpuBg`, `cpuText`) |
| `Fonts.h` | Platform-correct typeface helpers (`createFont`, `createFontRaw`, `createMonospaceFont`) and named sizes (`fontTitle`, `fontSectionLabel`, `fontComboBox`, ...) |
| `LookAndFeel.h` | `ebs::LookAndFeel`: full custom painting (buttons incl. the `"primary"` ComponentID contract, rotary knobs, toggles, combos, editors, labels, tooltips, popups, tabs, scrollbars) + `drawGroupFrame` / `drawFramePlain` + shared `widgetColour()` three-level resolver + chrome `ThemeChromeIds` |
| `IconButton.h` | `ebs::IconButton`: vector icon button (procedural + SVG-flattened Feather/Material glyphs incl. download/eye/eyeOff and the transport pair + rewind), viewer-toolbar set (zoom in / out, four chevrons, grid, step, power, trash, save), framed chrome mode, play/stop toggle state, grey disabled transport glyphs, `ColourIds` overrides |
| `FileGripButton.h` | `ebs::FileGripButton`: external-file drag handle — resolves the file AT DRAG TIME (a recycled row can never drag a stale path), open-hand cursor, native drag-out towards DAW/desktop |
| `FramedBody.h` | `ebs::FramedBody`: drop-in rounded framed panel body |
| `Knob.h` | `ebs::Knob`: typed rotary slider (arc track, optional pivot-centred fill via `setCentredFill`/`"centred"` property, gradient cap) rendered by the shared LookAndFeel; colours on standard `juce::Slider` rotary ids through the three-level contract |
| `MorphSlider.h` | `ebs::MorphSlider`: slim pivot-centred horizontal track for blend/crossfade parameters (0..1), self-painting, themed built-ins |
| `PowerToggle.h` | `ebs::PowerToggle`: power-glyph toggle with warm glow when engaged, disabled dimming; `glowColourId` override |
| `ToggleSwitch.h` | `ebs::ToggleSwitch`: iOS-style sliding switch for settings pages (plain `juce::ToggleButton` underneath, ~120 ms knob glide); `trackOnColourId` / `trackOffColourId`; disabled dims everything |
| `StatusBar.h` | `ebs::StatusBar` global log bar + activity slots (`beginActivity`: spinner + label + elapsed, ratio bar, queue bar, burst) + resource meters (right-aligned slots, GPU cell) + single-instance floating `LogWindow` (append-only, live, visible truncation, OS-native title bar, `ebs::LogView` over `juce::CodeEditorComponent`), thread-safe `logLine()`, C0/C1 sanitising, `ColourIds` overrides |
| `BusyIndicator.h` | `ebs::BusyIndicator`: reusable indeterminate/determinate progress strip behind the shared theme (stage text or normalized progress) |
| `TextSanitize.h` | `ebs::sanitizeForShaping()`: replaces the C0/C1 controls JUCE's HarfBuzz shaper cannot render (missing-glyph crash guard); idempotent; apply at every edge feeding on-screen text |
| `HelpBubble.h` | `ebs::HelpBubble`: 14x14 accent "?" bubble showing the host tooltip — section instructions live in the bubble, same look across panels and projects |
| `PathRow.h` | `ebs::PathRow`: path input row (editor + reveal-in-Explorer + browse), File/Directory modes, tooltip-aware via `SettableTooltipClient` |
| `DataList.h` | `ebs::DataList`: filterable/sortable table list over `juce::TableListBox` — `Row`/`Column`/`View` model, view+search+sort proxy, stable-id selection, per-row action column, progress cells (bar or percent), content-aware auto-size, show/hide column menu, native file-drag grips |
| `Sidebar.h` | `ebs::Sidebar`: collapsible/resizable side panel — the library owns only the chrome (edge rail: collapse chevron + width drag, `onWidthChanged`); the host owns the content and the width policy |
| `BandGrip.h` | `ebs::BandGrip`: vertical resize strip for a bottom band — the host owns the geometry, the grip owns the interaction (drag, clamping, reset, relayout) |
| `Waveform.h` | `ebs::Waveform`: compact audio preview with time labels (`m:ss`, remaining-count mode), a draggable and resizable selection band, explicit playhead state (Stopped / Playing / Finished), shared thumbnail cache via `setCache`. **NOT in the umbrella** - include it directly and link `juce::juce_audio_utils` |
| `ThumbnailPool.h` | `ebs::ThumbnailPool`: bounded pool of `juce::AudioThumbnail`s sharing one cache, uid-keyed, evicted by least-recently-used, destruction order correct by construction. **NOT in the umbrella** - include it directly and link `juce::juce_audio_utils` |
| `CompatOt.h` | Transitional `namespace ot = ebs;` alias for codebases mid-migration |

## Colour overrides

Widgets expose JUCE-standard `ColourIds`. Resolution order:

1. per-instance `setColour (id, colour)` (standard JUCE),
2. theme-level hook — subclass `ebs::LookAndFeel` and override
   `widgetThemeColour (int colourId)` (return non-transparent to restyle
   every instance at once; default overrides nothing),
3. built-in shape-aware palette default.

Text buttons: setting `juce::TextButton::buttonColourId` **per instance**
keeps that colour as the body tint across rest / hover / down (brighter/darker
variations on hover/down) while inheriting the shared accent hover rule
(the 1 px liseret). Without an explicit colour, buttons use the built-in
form-style fills (`bgDark` body, `panelBorder` edge).

**Toggled state (v0.7.2).** For a button with `setClickingTogglesState (true)`,
the ON state resolves in this order:

1. an explicitly set `TextButton::buttonOnColourId` wins outright (solid fill,
   no outline);
2. a chip that only set `buttonColourId` keeps its EXACT fill — the accent
   outline is the active-state signal, so translucent chips do not change
   alpha/brightness when toggled;
3. a bare button switches to the solid accent.

`drawButtonText` additionally honours an instance `textColourOffId` and keeps
white over pressed/toggled accent fills.

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

- JUCE 8 (the smoke test fetches 8.0.8; pass `EIFFELBS_UI_JUCE_DIR` to test
  against your own checkout).
- C++20, CMake 3.22+.
- Since **v0.12.7** the INTERFACE target also links `juce::juce_gui_extra`
  (the log window's `LogView` lives there). Provide that module in your own
  JUCE setup, or configuration fails with
  `target juce::juce_gui_extra not found`.
- Since **v0.12.27** the two audio widgets (`Waveform.h`, `ThumbnailPool.h`) are
  deliberately kept OUT of the umbrella `eiffelbs.h`: they need
  `juce::AudioThumbnail` / `juce::AudioThumbnailCache`, so an umbrella include
  compiles the whole `juce_audio_devices` module in every consumer TU. Include
  them directly and link `juce::juce_audio_utils`. On Windows, define
  `NOMINMAX` on any target whose TUs reach `<windows.h>` bare (the ARA SDK and
  the VST3 plugin-client chain do): JUCE's `juce_UMPEndpoint.h` calls
  `std::min` WITHOUT parentheses, which the Windows min/max macros break.

## Consumption (FetchContent, pinned tag)

```cmake
include(FetchContent)
FetchContent_Declare(eiffelbs-ui
    GIT_REPOSITORY https://github.com/EiffelBS/eiffelbs-ui.git
    GIT_TAG        v0.12.28)              # pin tags; behaviour changes bump minor
FetchContent_MakeAvailable(eiffelbs-ui)

target_link_libraries(my_app PRIVATE
    eiffelbs::ui                        # headers + ABI-critical JUCE defines
    juce::juce_gui_basics               # plus juce_gui_extra since v0.12.7
    juce::juce_gui_extra)
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
- Additive components = patch bump; behaviour / ColourIds / API changes =
  minor bump. Every release tag is expected to ship with a green smoke run.

## Roadmap

- **v0.1** — extraction: palette + fonts + LookAndFeel + generic widgets,
  consumed side-by-side with the apps' local copies.
- **v0.2** — `ColourIds` on `IconButton` / `StatusBar` with the three-level
  resolution contract (instance > theme hook > built-in); consumer cutover
  done in the first consumer (local copies deleted, FetchContent pin).
  **v0.2.1** — `IconButton::Shape::lock` padlock glyph; semantics are
  app-defined.
- **v0.3** — rotary `Knob` + painter, `MorphSlider`, `PowerToggle`
  (carried over from the OpenVoxTuner origin UI), always-dark viz-chrome
  palette tokens and chrome `ThemeChromeIds` (tab pill fill/text, bar rule,
  checkbox well) with built-ins frozen at their pre-v0.3 values; per-instance
  `TextButton::buttonColourId` chip fills under the shared hover rule.
  First consumers of the tag: OpenVoxTuner and companion apps.
- **v0.4** — theme-switch broadcast: `setTheme()` + `ThemeSubscriber`
  (subscribe/unsubscribe), LookAndFeel self-subscribed and auto-refreshed on
  switch. **v0.4.1** — theme-aware default checkbox well. **v0.4.2** —
  `drawButtonText` honours instance `textColourOffId`; `drawButtonBackground`
  toggle branch honours instance `buttonOnColourId`.
- **v0.5** — `StatusBar` activity slots: `beginActivity()` claims a slot
  rendered as spinner + label + elapsed with a ratio bar; `setQueue()` shows
  the processing-queue position; burst block and `onQueueClicked` follow.
- **v0.6** — `StatusBar` resource meters: right-aligned slots, GPU cell,
  glued labels + dividers, exact 5 px gaps, widths reserved for 100 % (no
  jitter), compact view.
- **v0.7** — `StatusBar` multiple concurrent activity claims.
  **v0.7.1** — `HelpBubble`. **v0.7.2** — toggle contract: an explicit
  `buttonOnColourId` wins; a chip that only set `buttonColourId` keeps its
  exact fill with the accent outline as the state signal.
  **v0.7.3** — `PathRow` shared path input row.
- **v0.8** — `DataList`: generic take/model-style lists (views, search,
  sortable columns, row actions, progress cells) over `juce::TableListBox`.
  **v0.8.1** — `IconButton` shapes `download` / `eye` / `eyeOff`.
  **v0.9** — `DataList` dedicated Progress-column bar; action clicks route to
  `onAction`; progress rows repaint eagerly.
- **v0.10** — `ebs::Sidebar`: collapsible/resizable side panel with a rail
  handle; the host owns content and width policy.
- **v0.11** — `DataList` stable sort with id tiebreak + progress-safe
  refresh; `Sidebar` cursor/affordance polish (no proportional clamp, silent
  `setSidebarWidth`, zone cursors). **v0.11.2** — `DataList` content-aware
  auto-size + show/hide column menu, per-row transparent grip overlay;
  new `ebs::ToggleSwitch` and `ebs::FileGripButton`; `fontHint` raised to
  13 px.
- **v0.12** — `StatusBar` `LogWindow` on the OS-native title bar.
  **v0.12.1** — C0/C1 control sanitising before shaping (HarfBuzz
  missing-glyph crash guard). **v0.12.3** — `ebs::BusyIndicator`.
  **v0.12.4** — grey disabled custom transport glyphs. **v0.12.5** —
  `BusyIndicator` status-bar strips, new `ebs::BandGrip`, `ToggleSwitch`
  disabled-state fix. **v0.12.6** — `IconButton` rewind transport glyph.
  **v0.12.7** — `LogWindow` becomes append-only, alive (live appends) and
  honest (visible truncation, native title-bar buttons); the INTERFACE
  target links `juce_gui_extra`.
  **v0.12.28** â€” `IconButton` viewer-toolbar glyph set: zoomIn / zoomOut, the four
  chevrons, grid, step, power, trash, save (ported from OpenVoxTuner); the smoke ink
  gate now covers the WHOLE enum.
- **Next** — extend ColourIds coverage (`FramedBody`, favourite gold,
  LogWindow chrome); keep the smoke green as the gate for every
  consumer-facing tag.

## License

AGPLv3 — see [LICENSE](LICENSE). Extracted from OpenVoxTuner
(original copyright (C) 2026 EiffelBS).
