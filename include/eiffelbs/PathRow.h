// PathRow.h
// eiffelbs-ui - shared path input row (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A path field for settings forms: TextEditor + "reveal in Explorer"
// (loupe) + "..." (browse) buttons in one row. Two modes:
//   - File     : browse selects a file
//   - Directory: browse selects a folder
// A host can make the row READ-ONLY (setReadOnly) for a path that is only
// shown, never typed: the editor is locked and the browse button goes with it.
// The reveal button opens the path's folder in Explorer (the path itself
// when it is a directory). The host reads getText() (e.g. from an Apply
// button) and/or reacts to onTextChanged.
//
// The row is a SettableTooltipClient so a host can explain what the field
// expects (setTooltip). The placeholder alone is not enough: it disappears on
// the first keystroke, and a path contract - which folder, which level - is
// exactly what a user cannot guess. JUCE walks the component chain when
// looking for a tooltip, so one set here also shows while hovering the
// editor or the two buttons.
//
// The browse dialog shows what the host declares it reads (setBrowseFilters,
// wildcards joined with ';'). A picker that lists every file in the folder offers a
// choice the form cannot honour: the user picks an .mp3 into a lyric-file field and
// finds out afterwards. Default is "*" - the open view - for hosts that never name
// their extensions; a Directory row keeps showing folders, as its dialog has no
// extension to filter on.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/Theme.h>
#include <eiffelbs/Fonts.h>
#include <eiffelbs/IconButton.h>

namespace ebs
{

class PathRow : public juce::Component,
                public juce::SettableTooltipClient
{
public:
    enum class Mode { File, Directory };

    PathRow (Mode modeIn = Mode::File, const juce::String& placeholder = {})
        : mode (modeIn)
    {
        addAndMakeVisible (editor);
        editor.setColour (juce::TextEditor::textColourId, ebs::text());
        editor.setColour (juce::TextEditor::backgroundColourId, ebs::bgDark());
        editor.setColour (juce::TextEditor::outlineColourId, ebs::panelBorder());
        editor.setFont (ebs::fontHint());
        editor.setTextToShowWhenEmpty (placeholder, ebs::textDim());
        editor.onTextChange = [this]
        { if (onTextChanged != nullptr) onTextChanged(); };

        addAndMakeVisible (revealBtn);
        revealBtn.setFramed (true);
        revealBtn.setTooltip ("Reveal in Explorer");
        revealBtn.onClick = [this] { reveal(); };

        addAndMakeVisible (browseBtn);
        browseBtn.setButtonText ("...");
        browseBtn.setTooltip (mode == Mode::Directory ? "Choose a folder"
                                                      : "Choose a file");
        browseBtn.onClick = [this] { browse(); };
    }

    juce::String getText() const { return editor.getText(); }

    /** Replace the dim "empty" text shown in the field. Used when the auto
        target CHANGES (a library root, an engine default): the reader must see
        which folder "(auto)" means, not just the word (author report
        2026-10-26). */
    void setPlaceholder (const juce::String& text)
    {
        placeholderText = text;
        editor.setTextToShowWhenEmpty (text, ebs::textDim());
    }

    juce::String placeholderTextForTest() const { return placeholderText; }
    void setText (const juce::String& t, bool notify = false)
    { editor.setText (t, notify); }

    /** Read-only row: the text shows what a host already knows (the file this
        window is describing), and typing in it must change NOTHING. The browse
        button goes with it - `setShowBrowse(false)` - so a path nobody may pick
        cannot be typed into the form by hand. */
    void setReadOnly (bool on)
    {
        editor.setReadOnly (on);
        if (on)
            setShowBrowse (false);
    }

    /** Component::setEnabled is not virtual: our own setter keeps the
        three children in sync with the row state. */
    void setRowEnabled (bool on)
    {
        Component::setEnabled (on);
        editor.setEnabled (on);
        revealBtn.setEnabled (on);
        browseBtn.setEnabled (on);
    }

    void setShowReveal (bool on)  { revealBtn.setVisible (on); }
    void setShowBrowse (bool on)  { browseBtn.setVisible (on); }

    /** Which files the browse dialog shows. A picker that lists EVERYTHING is not a filter:
        a form that takes a timed lyric file cannot be filled by an .mp3, and the user only
        finds that out after picking one - so the host names the extensions it reads. The list
        is wildcards joined with ';' ("*.lrc;*.srt;*.vtt"); passing an empty string restores the
        open "*" view on a File row. A Directory row keeps this setting as its own recorded state,
        and its chooser gets NO filter: the dialog lists folders, where an extension says nothing. */
    void setBrowseFilters (const juce::String& globs) { filters = globs.trim(); }

    /** The filter the browse dialog uses now - what a host sets here is what it gets. */
    juce::String getBrowseFilters() const { return filters; }

    std::function<void()> onTextChanged;

    void resized() override
    {
        auto b = getLocalBounds();
        browseBtn.setBounds (b.removeFromRight (36).withSizeKeepingCentre (32, 22));
        revealBtn.setBounds (b.removeFromRight (24).withSizeKeepingCentre (20, 20));
        b.removeFromRight (4);
        editor.setBounds (b);
    }

private:
    void reveal()
    {
        auto f = juce::File (editor.getText().trim());
        auto target = f.existsAsFile() ? f.getParentDirectory() : f;
        if (! target.isDirectory())
            target = target.getParentDirectory();
        if (target.isDirectory())
            target.startAsProcess();
    }

    void browse()
    {
        // A Directory row has no extension to filter: the dialog shows folders, and
        // passing wildcards there changes nothing. A File row shows what the host named,
        // or everything when the host named nothing.
        const auto filter = mode == Mode::Directory ? juce::String()
                         : (filters.isNotEmpty() ? filters : juce::String ("*"));
        chooser = std::make_unique<juce::FileChooser> (
            mode == Mode::Directory ? "Choose a folder" : "Choose a file",
            juce::File (editor.getText().trim()),
            filter);
        chooser->launchAsync (
            juce::FileBrowserComponent::openMode
                | (mode == Mode::Directory
                       ? juce::FileBrowserComponent::canSelectDirectories
                       : juce::FileBrowserComponent::canSelectFiles),
            [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult() == juce::File{})
                    return;
                editor.setText (fc.getResult().getFullPathName(), true);
            });
    }

    Mode mode;
    juce::String placeholderText;
    juce::String filters { "*" };   // what the browse dialog lists, set by the host
    juce::TextEditor editor;
    ebs::IconButton revealBtn { ebs::IconButton::Shape::search };
    juce::TextButton browseBtn { "..." };
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace ebs
