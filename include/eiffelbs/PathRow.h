// PathRow.h
// eiffelbs-ui - shared path input row (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A path field for settings forms: TextEditor + "reveal in Explorer"
// (loupe) + "..." (browse) buttons in one row. Two modes:
//   - File     : browse selects a file
//   - Directory: browse selects a folder
// The reveal button opens the path's folder in Explorer (the path itself
// when it is a directory). The host reads getText() (e.g. from an Apply
// button) and/or reacts to onTextChanged.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/Theme.h>
#include <eiffelbs/Fonts.h>
#include <eiffelbs/IconButton.h>

namespace ebs
{

class PathRow : public juce::Component
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
    void setText (const juce::String& t, bool notify = false)
    { editor.setText (t, notify); }

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
        chooser = std::make_unique<juce::FileChooser> (
            mode == Mode::Directory ? "Choose a folder" : "Choose a file",
            juce::File (editor.getText().trim()),
            mode == Mode::Directory ? "" : "*");
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
    juce::TextEditor editor;
    ebs::IconButton revealBtn { ebs::IconButton::Shape::search };
    juce::TextButton browseBtn { "..." };
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace ebs
