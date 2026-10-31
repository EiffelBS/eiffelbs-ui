// NumberEditor.h
// eiffelbs-ui - a numeric single-line field: digits only, a range, a committed
// value. Built on ClearableEditor so the house clear affordance and styling
// stay one implementation.
//
// WHY: several forms carried a free text editor for an integer (duration,
// variations, seeds). Digits were filtered, but the field still behaved like
// prose: Enter did nothing, the caret could leave an out-of-range number, and
// there was no way to step a value. This component owns that contract once:
// type digits, step with Up/Down, and commit a clamped value on Return or
// focus loss. An empty field resolves to the caller's default, never to 0 by
// accident.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <limits>

#include "eiffelbs/ClearableEditor.h"

namespace ebs
{

class NumberEditor : public ClearableEditor
{
public:
    NumberEditor()
    {
        setInputRestrictions (19, "0123456789");
        setSelectAllWhenFocused (true);
        setJustification (juce::Justification::centredLeft);
    }

    /** The legal closed range, and the value an empty commit resolves to.
        `step` is what Up/Down (and setValue) move by. */
    void setRange (juce::int64 minimum, juce::int64 maximum,
                   juce::int64 fallback, juce::int64 increment = 1)
    {
        minValue = minimum;
        maxValue = juce::jmax (minimum, maximum);
        defaultValue = juce::jlimit (minValue, maxValue, fallback);
        stepValue = juce::jmax ((juce::int64) 1, increment);
        // A field limited to 0..8 does not need 19 typed digits; the cap also
        // keeps a pasted 30-digit run from becoming a bogus large value.
        const auto digits = juce::String (maxValue).length();
        setInputRestrictions (juce::jmax (1, digits), "0123456789");
    }

    juce::int64 getValue() const
    {
        const auto text = getText().trim();
        if (text.isEmpty())
            return defaultValue;
        return juce::jlimit (minValue, maxValue, text.getLargeIntValue());
    }

    /** Set the committed value; `notify` runs the normal TextEditor callbacks
        so the host's save/persist hook sees the change. */
    void setValue (juce::int64 value, bool notify = false)
    {
        setText (juce::String (juce::jlimit (minValue, maxValue, value)),
                 notify);
    }

    /** The value as committed now (clamped), after a focus/Return. */
    void commit()
    {
        normalizeText();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::upKey)
        {
            setValue (getValue() + stepValue, true);
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            setValue (getValue() - stepValue, true);
            return true;
        }
        if (key == juce::KeyPress::returnKey
            || key == juce::KeyPress::tabKey)
        {
            normalizeText();
            if (key == juce::KeyPress::returnKey)
            {
                moveKeyboardFocusToSibling (true);
                return true;
            }
        }
        return ClearableEditor::keyPressed (key);
    }

    void focusLost (FocusChangeType cause) override
    {
        normalizeText();
        ClearableEditor::focusLost (cause);
    }

private:
    void normalizeText()
    {
        if (normalizing)
            return;
        normalizing = true;
        const auto original = getText().trim();
        auto value = original.isEmpty() ? defaultValue
                                        : original.getLargeIntValue();
        value = juce::jlimit (minValue, maxValue, value);
        const auto text = juce::String (value);
        if (text != getText())
            setText (text, true);   // notification keeps the host's save hook
        normalizing = false;
    }

    juce::int64 minValue = 0;
    juce::int64 maxValue = std::numeric_limits<juce::int64>::max();
    juce::int64 defaultValue = 0;
    juce::int64 stepValue = 1;
    bool normalizing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NumberEditor)
};

} // namespace ebs
