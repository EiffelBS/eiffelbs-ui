// ModelMenuButton.h
// eiffelbs-ui - the ONE "model + effort" chevron: a quiet button whose label is
// the model, with the current effort trailing it in dim type, sized to its
// CONTENT. Clicking opens the same two-row menu the chat input has always had
// (Model / Effort); each row shows a right-pointing chevron and opens the next
// layer - the Model row opens the shared searchable model list, the Effort row
// the levels. A picker and a menu button can never drift apart.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY THIS EXISTS: the assistant chat input drew its own chevron, its own
// two-row callout and a filterable ListBox; the "Generate with AI" dialog drew
// the same chevron from a combined "model - effort" string, ellipsized it, and
// lost the effort (author report 2026-10-25). One component, one list, one
// label layout: the model may ellipsize, the effort stays readable, and every
// surface shows the same two facts.
//
// THE WIDTH IS THE COMPONENT'S OWN: `widthForContent()` is what a layout asks
// for. A host that stretches it across a full row is the layout that hides the
// effort behind an ellipsis; `setMaxWidth()` caps it where the host must.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/MenuChevronButton.h"
#include "eiffelbs/SearchableModelPicker.h"

#include <functional>

namespace ebs
{

class ModelMenuButton : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    ModelMenuButton()
    {
        face.setTooltip ("Choose the model and how much it thinks");
        face.onClick = [this] { showMenu(); };
        addAndMakeVisible (face);

        // The picker is a HELPER: it owns the shared list logic (search,
        // filtering, hover, footer) and opens its popup anchored to the face.
        // It is never painted itself - the face is the visible control.
        picker.setVisible (false);
        picker.onChange = [this] (const juce::String& id)
        {
            selectedModelIdValue = id;
            face.setButtonText (labelForModel (id));
            resized();
            if (onModelChosen != nullptr)
                onModelChosen (id);
        };
        addChildComponent (picker);
        face.setButtonText ("server default");
    }

    ~ModelMenuButton() override
    {
        // The CallOutBox owns the menu rows; drop our pointer first so a late
        // dismissal cannot call back into a destroyed button.
        if (menuContent != nullptr)
            menuContent->owner = nullptr;
        if (menuCallout != nullptr)
        {
            auto* box = menuCallout;
            menuCallout = nullptr;
            box->dismiss();
        }
    }

    /** Tooltip of the visible control. The FACE is what the pointer sits on,
        so the text must land there; the component keeps it too so a host that
        reads getTooltip() sees the same sentence. */
    void setTooltip (const juce::String& text)
    {
        face.setTooltip (text);
        juce::SettableTooltipClient::setTooltip (text);
    }

    // === The two facts the face shows ====================================
    /** The model name: left, may ellipsize when a max width forces it. */
    void setModelText (const juce::String& text)
    {
        modelText = text;
        face.setButtonText (modelText.isNotEmpty() ? modelText
                                                   : juce::String ("server default"));
        resized();
    }
    juce::String getModelText() const { return modelText; }

    /** The effort word: right, dim, ALWAYS readable (the fact a plain combined
        label loses first when the text is clipped). */
    void setEffortText (const juce::String& text)
    {
        effortText = text;
        face.setTrailingText (effortText);
        resized();
    }
    juce::String getEffortText() const { return effortText; }

    // === The model list (the shared searchable picker) ===================
    /** `groups` is the provider label per model (parallel to ids): the shared
        picker paints one header row per provider run in its popup, and typing
        a provider narrows the list - the grouping the chat input had before
        the component conversion. */
    void setModels (const juce::StringArray& ids,
                    const juce::StringArray& labels,
                    const juce::StringArray& details = {},
                    const juce::StringArray& groups = {})
    {
        modelIds = ids;
        modelLabels = labels;
        picker.setItems (ids, labels, details, groups);
        if (selectedModelIdValue.isNotEmpty())
            face.setButtonText (labelForModel (selectedModelIdValue));
    }
    void setSelectedModelId (const juce::String& id)
    {
        selectedModelIdValue = id;
        picker.setSelectedId (id, false);
        face.setButtonText (labelForModel (id));
        resized();
    }
    juce::String getSelectedModelId() const { return selectedModelIdValue; }
    std::function<void (const juce::String&)> onModelChosen;

    // === The effort list =================================================
    void setEfforts (const juce::StringArray& ids,
                     const juce::StringArray& labels)
    {
        effortIds = ids;
        effortLabels = labels;
    }
    void setSelectedEffortId (const juce::String& id)
    {
        selectedEffortIdValue = id;
        for (int i = 0; i < effortIds.size(); ++i)
            if (effortIds[i] == id)
                setEffortText (effortLabels[i]);
    }
    juce::String getSelectedEffortId() const { return selectedEffortIdValue; }
    std::function<void (const juce::String&)> onEffortChosen;

    /** A host that needs the row filled may cap the width; 0 = no cap. */
    void setMaxWidth (int maxPixels) { maxWidth = juce::jmax (0, maxPixels); }
    int getMaxWidth() const { return maxWidth; }

    /** The width this button needs to show both facts - the number a layout
        must use instead of the row's full width. */
    int widthForContent() const
    {
        const int natural = face.widthForLabel();
        return maxWidth > 0 ? juce::jmin (natural, maxWidth) : natural;
    }

    /** The face's height (the button has no chrome of its own). */
    int heightForContent() const { return 26; }

    void resized() override
    {
        face.setBounds (getLocalBounds());
        picker.setBounds (getLocalBounds());   // helper only: anchor fallback
    }

    // === The menu ========================================================
    /** Open the two-row Model / Effort menu. Public so a host may open it from
        its own affordance and the gate can drive the same path headlessly. */
    void showMenu()
    {
        if (menuOpen)
            return;   // never stack two of our own popups
        auto content = std::make_unique<MenuRows> (this,
                                                   juce::jmax (170, widthForContent()));
        menuContent = content.get();
        menuOpen = true;
        // The CallOutBox is a real top-level window: launch it only when the
        // face is on screen. Headless gates drive the rows through the seams.
        if (face.isShowing())
            menuCallout = &juce::CallOutBox::launchAsynchronously (
                std::move (content), face.getScreenBounds(), nullptr);
        else
            menuOpen = false;   // headless: no window, nothing to dismiss
    }

    /** The row text at `row` - the paint and the seams read ONE source. */
    juce::String menuRowText (int row) const
    {
        return row == 0
            ? "Model: " + (modelText.isNotEmpty() ? modelText
                                                   : juce::String ("server default"))
            : "Effort: " + (effortText.isNotEmpty() ? effortText
                                                     : juce::String ("Auto"));
    }

    // === TEST SEAMS ======================================================
    // The smoke test renders offscreen: it proves geometry, text and painting,
    // and it can drive the menu result through the SAME handler a click runs.
    int menuItemCountForTest() const { return 2; }
    juce::String menuItemTextForTest (int row) const { return menuRowText (row); }
    void chooseMenuResultForTest (int row) { onMenuResult (row + 1); }
    /** Drive the shared picker's list the way the popup would. */
    void pickerOpenForTest() { picker.openForTest(); }
    int pickerVisibleCountForTest() const { return picker.visibleIdCountForTest(); }
    void pickerSearchForTest (const juce::String& t)
    {
        picker.setSearchForTest (t);
    }
    /** The list the reader sees: provider headers, and every row reachable.
        Pass-through seams so a host's own gate can prove grouping + scroll
        through the SAME ModelMenuButton the chat input uses. */
    int pickerDisplayRowCountForTest() const     { return picker.displayRowCountForTest(); }
    int pickerHeaderCountForTest() const         { return picker.headerCountForTest(); }
    bool pickerRowIsHeaderForTest (int row) const
    {
        return picker.rowIsHeaderForTest (row);
    }
    juce::String pickerRowHeaderTextForTest (int row) const
    {
        return picker.rowHeaderTextForTest (row);
    }
    int pickerScrollRangeForTest() const         { return picker.scrollRangeForTest(); }
    int pickerHighlightDisplayRowForTest() const
    {
        return picker.highlightDisplayRowForTest();
    }

    /** Offscreen render of the two menu rows - the SAME component the CallOutBox
        paints on screen. The gate counts ink in the chevron strips, which a
        plain PopupMenu (no submenu arrow on these rows) cannot pass. */
    void paintMenuRowsForTest (juce::Graphics& g, int width)
    {
        MenuRows rows (this, juce::jmax (170, width));
        rows.paintEntireComponent (g, true);
    }

private:
    juce::String labelForModel (const juce::String& id) const
    {
        for (int i = 0; i < modelIds.size(); ++i)
            if (modelIds[i] == id)
                return i < modelLabels.size() ? modelLabels[i] : id;
        return id;
    }

    void onMenuResult (int result)
    {
        menuOpen = false;
        if (result == 1 && ! modelIds.isEmpty())
            picker.openPopupFor (&face);
        else if (result == 2)
            showEffortMenu();
    }

    void showEffortMenu()
    {
        if (effortIds.isEmpty())
            return;
        juce::PopupMenu menu;
        for (int i = 0; i < effortIds.size(); ++i)
            menu.addItem (i + 1, effortLabels[i], true,
                          effortIds[i] == selectedEffortIdValue);
        menu.showMenuAsync (
            juce::PopupMenu::Options()
                .withTargetComponent (&face)
                .withTargetScreenArea (face.getScreenBounds()),
            [safe = juce::Component::SafePointer<ModelMenuButton> (this)]
            (int result)
            {
                if (safe == nullptr || result <= 0
                    || result > safe->effortIds.size())
                    return;
                const auto id = safe->effortIds[result - 1];
                safe->selectedEffortIdValue = id;
                safe->setEffortText (safe->effortLabels[result - 1]);
                if (safe->onEffortChosen != nullptr)
                    safe->onEffortChosen (id);
            });
    }

    /** The Model / Effort popup: two rows, labels left, light ">" chevrons
        right, like a submenu (the chat input's own popup, restored in the
        shared component - author report 2026-10-26: "il y avait avant un
        chevron vers la droite pour aller au sous-menu"). A native PopupMenu
        draws its submenu arrow only on real submenus, and the filterable
        model list cannot live in one - so both rows are drawn here in the
        frame's menu language. Each row dismisses this box BEFORE opening the
        next menu, so two popups never stack. */
    struct MenuRows : juce::Component
    {
        // The button may be gone before this box is: the popup outlives no UI.
        juce::Component::SafePointer<ModelMenuButton> owner;
        int hoverRow = -1;
        static constexpr int rowH = 32;
        static constexpr int padV = 4;

        MenuRows (ModelMenuButton* o, int width) { owner = o; setSize (width, padV * 2 + rowH * 2); }

        ~MenuRows() override
        {
            if (auto* o = owner.getComponent())
                o->menuClosed();
        }

        void paint (juce::Graphics& g) override
        {
            auto* o = owner.getComponent();
            paintRow (g, 0, o != nullptr ? o->menuRowText (0) : "Model");
            paintRow (g, 1, o != nullptr ? o->menuRowText (1) : "Effort");
        }

        void paintRow (juce::Graphics& g, int row, const juce::String& text)
        {
            const auto r = juce::Rectangle<int> (2, padV + row * rowH,
                                                 getWidth() - 4, rowH).toFloat();
            if (row == hoverRow)
            {
                g.setColour (ebs::accent().withAlpha (0.16f));
                g.fillRoundedRectangle (r, 4.0f);
            }
            g.setColour (ebs::text());
            g.setFont (ebs::fontHint());
            g.drawText (text, r.withTrimmedLeft (10.0f).withTrimmedRight (24.0f),
                        juce::Justification::centredLeft, true);
            // Light ">" chevron, right-aligned: the row opens something. Two
            // thin strokes, like the frame buttons' V turned sideways.
            const float cx = r.getRight() - 14.0f;
            const float cy = r.getCentreY();
            juce::Path v;
            v.startNewSubPath (cx - 2.5f, cy - 4.0f);
            v.lineTo (cx + 2.5f, cy);
            v.lineTo (cx - 2.5f, cy + 4.0f);
            g.setColour (ebs::textDim());
            g.strokePath (v, juce::PathStrokeType (1.4f,
                                                   juce::PathStrokeType::mitered,
                                                   juce::PathStrokeType::rounded));
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int row = (e.y - padV) / rowH;
            const int h = (row >= 0 && row < 2) ? row : -1;
            if (h != hoverRow)
            {
                hoverRow = h;
                repaint();
            }
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            hoverRow = -1;
            repaint();
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int row = (e.y - padV) / rowH;
            auto* o = owner.getComponent();   // snapshot BEFORE the box tears this component down
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();   // dismiss THIS box first, so two popups never stack
            if (o != nullptr && (row == 0 || row == 1))
                o->onMenuResult (row + 1);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MenuRows)
    };

    void menuClosed()
    {
        menuOpen = false;
        if (menuContent != nullptr)
            menuContent = nullptr;
        menuCallout = nullptr;   // the box is gone with its content
    }

    MenuChevronButton face;
    SearchableModelPicker picker;
    juce::String modelText, effortText, selectedModelIdValue,
        selectedEffortIdValue;
    juce::StringArray modelIds, modelLabels, effortIds, effortLabels;
    int maxWidth = 0;
    bool menuOpen = false;
    juce::Component::SafePointer<MenuRows> menuContent;
    juce::CallOutBox* menuCallout = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelMenuButton)
};

} // namespace ebs
