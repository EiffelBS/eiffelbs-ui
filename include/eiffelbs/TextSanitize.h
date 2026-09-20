// TextSanitize.h
// eiffelbs-ui - display-text hardening for JUCE's HarfBuzz shaper.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// JUCE's text shaper (juce_SimpleShapedText.cpp lowLevelShape) hits a
// missing-glyph jassert - a __debugbreak crash in Debug builds - as soon
// as a measured/drawn line contains a character the resolved typeface
// cannot shape. In practice the trigger is always a C0/C1 CONTROL
// character (U+0000-U+001F, U+007F-U+009F): Segoe UI and friends provide
// no glyphs for them, and JUCE only substitutes CR/LF/TAB itself. The
// classic producer is UTF-8 bytes decoded as Latin-1 (one byte = one
// codepoint, i.e. juce::String(const char*) in JUCE 8): every multi-byte
// sequence then becomes C1 controls (en-dash E2 80 93 turns into
// U+00E2 U+0080 U+0093, whose UTF-8 form is C3 A2 C2 80 C2 93).
//
// sanitizeForShaping() replaces those controls with a space and leaves
// every printable codepoint (accents, CJK, en-dash, ...) intact. It is
// idempotent: apply it at every edge feeding on-screen text (status
// bar, log window, meter values, labels, dialogs).

#pragma once

#include <juce_core/juce_core.h>

namespace ebs
{

/** Replace the characters JUCE's HarfBuzz shaper cannot render (C0
    controls below 0x20 except tab/LF/CR which JUCE substitutes itself,
    DEL and the C1 range 0x80-0x9F) with a space. Keeps every printable
    codepoint intact. Idempotent; cheap fast path returns the input
    untouched when it is already clean. */
inline juce::String sanitizeForShaping (const juce::String& s)
{
    bool dirty = false;
    for (auto p = s.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = (juce::uint32) *p;
        if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r')
            || (c >= 0x7f && c <= 0x9f))
        {
            dirty = true;
            break;
        }
    }
    if (! dirty)
        return s;
    juce::String out;
    out.preallocateBytes ((size_t) s.getNumBytesAsUTF8() + 8);
    for (auto p = s.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = (juce::juce_wchar) *p;
        if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r')
            || (c >= 0x7f && c <= 0x9f))
            out += ' ';
        else
            out += c;
    }
    return out;
}

} // namespace ebs
