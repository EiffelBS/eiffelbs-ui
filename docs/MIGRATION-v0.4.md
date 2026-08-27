# Migrating to eiffelbs-ui v0.4.x — notes for OpenTimbre / OpenVisuAI / OpenVoxKey

This document targets consumer apps still pinned to **v0.2.x** (OpenTimbre,
OpenVisuAI at the time of writing). It summarises what v0.3/v0.4 add and the
two contract fixes shipped in **v0.4.2**, so the next agent touching those
projects can plan a cutover without re-deriving conventions.

## Version note

v0.4.1 was tagged locally during development but never published; **v0.4.2 is
the first published tag on the 0.4 line** (theme broadcast + all contract
fixes). Pin `GIT_TAG v0.4.2`.

## What v0.3/v0.4 add over v0.2.x

1. **Centralised theme broadcast** (`eiffelbs/eiffelbs.h`):

   ```cpp
   ebs::setTheme (ebs::Theme::Light);          // global switch + notify
   auto t = ebs::currentTheme();               // live palette read
   class Editor : private ebs::ThemeSubscriber // get themeChanged() callbacks
   { void themeChanged() override; };          // re-apply instance colours here
   ```

   Convention: components **subscribe in the constructor, unsubscribe in the
   destructor**, and re-apply their per-instance colours inside
   `themeChanged()`. Palettes come from token functions (`bgDark()`,
   `bgPanel()`, `accent()`, `accentSoft()`, `text()`, `textDim()`,
   `panelBorder()`); `vizBg()` is intentionally theme-invariant.

2. **Checkbox wells are theme-aware by default** (dark well on Dark, white
   well on Light) — consumers no longer need to override them.

## Contract fixes landed in v0.4.2

The three-tier colour contract is: *instance `setColour` > `widgetThemeColour`
hook > library default*. Two overrides previously ignored tiers above them:

- `drawButtonText` now honours an instance
  `TextButton::textColourOffId` (transparent = keep the palette default).
- `drawButtonBackground` toggle branch now honours an instance
  `TextButton::buttonOnColourId`: solid ON fill without outline; without it,
  an instance rest fill is visibly lifted (+35 %); bare buttons stay solid
  accent.

Consequence for consumers: any button colour set via `setColour(...)` is now
**guaranteed visible**; audit places where you worked around the old
behaviour by hand-painting text.

## Recipe: canvas-overlay chips (buttons floating on vizBg plots)

Buttons that sit on the always-dark plot island must NOT follow palette text
tokens — Light would turn their labels dark-on-dark:

```cpp
btn.setColour (juce::TextButton::buttonColourId,   ebs::accentSoft());
btn.setColour (juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha (0.92f));
btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4caf50)); // active signal
```

Toolstrip icons sitting on THEMED backgrounds instead follow `ebs::text()` at
rest and flip bright white only for active/toggled states over a SOLID accent
pill (`DrawableButton::backgroundOnColourId`, not a 20 % tint).

## Cutover checklist for v0.2.x consumers

1. Flip `GIT_TAG` to v0.4.2.
2. Register/unregister editors (or top-level components owning the palette)
   as `ThemeSubscriber`; replace hardcoded `juce::Colour(0xff...)` UI tokens
   with `ebs::` functions where they should track themes.
3. Decide per control: themed chrome vs canvas-overlay chip (recipe above);
   do not blanket-flag families by visual guess — verify what backdrop each
   row actually sits on.
4. Re-test all four states (rest/hover × toggled) under BOTH palettes after
   every theme switch; the Library regenerates nothing automatically beyond
   palette reads — baked drawables need explicit regeneration in
   `themeChanged()`.

— EiffelBS, 2026-08-27
