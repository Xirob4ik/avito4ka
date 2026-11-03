#include "PresetBar.h"

#include "Fonts.h"
#include "Theme.h"

namespace clipline::ui
{
    using namespace theme;

    namespace
    {
        constexpr int kSaveId = 100000, kFolderId = 100001;
    }

    PresetBar::PresetBar (PresetManager& m) : manager (m)
    {
        setTooltip ("Presets. Click the name for the list, arrows step through it. A dot after the name means the settings differ from the preset.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        refresh();
    }

    void PresetBar::refresh()
    {
        const auto currentName = manager.getCurrentName();
        const bool isModified = manager.isModified();
        if (currentName != name || isModified != modified)
        {
            name = currentName;
            modified = isModified;
            repaint();
        }
    }

    PresetBar::Zone PresetBar::zoneAt (juce::Point<float> p) const noexcept
    {
        if (! getLocalBounds().toFloat().contains (p))
            return none;
        if (p.x < 30.0f)
            return previous;
        if (p.x > (float) getWidth() - 30.0f)
            return next;
        return label;
    }

    void PresetBar::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (surface);
        g.fillRoundedRectangle (b, 7.0f);
        g.setColour (hot == label ? hairline.brighter (0.25f) : hairline);
        g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 1.0f);

        auto chevron = [&] (juce::Rectangle<float> area, bool left, bool lit)
        {
            juce::Path p;
            const float cx = area.getCentreX(), cy = area.getCentreY(), s = left ? -1.0f : 1.0f;
            p.startNewSubPath (cx - 2.0f * s, cy - 4.5f);
            p.lineTo (cx + 2.0f * s, cy);
            p.lineTo (cx - 2.0f * s, cy + 4.5f);
            g.setColour (lit ? text : textDim);
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        chevron (b.withWidth (30.0f), true,  hot == previous);
        chevron (b.withLeft (b.getRight() - 30.0f), false, hot == next);

        const auto textArea = b.reduced (34.0f, 0.0f);
        const auto font = fonts::sans (12.5f, fonts::Weight::Medium);
        g.setFont (font);
        g.setColour (text);

        const float textW = std::min (textArea.getWidth() - 14.0f, fonts::textWidth (font, name));
        const float startX = textArea.getCentreX() - (textW + (modified ? 14.0f : 0.0f)) * 0.5f;
        g.drawText (name, juce::Rectangle<float> (startX, b.getY(), textW + 2.0f, b.getHeight()), juce::Justification::centredLeft, true);

        if (modified)
        {
            g.setColour (accent);
            g.fillEllipse (startX + textW + 8.0f, b.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
    }

    void PresetBar::mouseMove (const juce::MouseEvent& e)
    {
        const int z = zoneAt (e.position);
        if (z != hot)
        {
            hot = z;
            repaint();
        }
    }

    void PresetBar::mouseExit (const juce::MouseEvent&)
    {
        hot = none;
        repaint();
    }

    void PresetBar::mouseDown (const juce::MouseEvent& e)
    {
        switch (zoneAt (e.position))
        {
            case previous: manager.step (-1); refresh(); break;
            case next:     manager.step (+1); refresh(); break;
            case label:    showMenu();         break;
            case none:     break;
        }
    }

    void PresetBar::showMenu()
    {
        manager.refresh();

        juce::PopupMenu menu;
        const auto& entries = manager.getEntries();
        const int current = manager.getCurrentIndex();

        juce::String lastCategory;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].category != lastCategory)
            {
                lastCategory = entries[i].category;
                menu.addSectionHeader (lastCategory);
            }
            menu.addItem ((int) i + 1, entries[i].name, true, (int) i == current);
        }

        menu.addSeparator();
        menu.addItem (kSaveId, "Save as user preset...");
        menu.addItem (kFolderId, "Show preset folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                            [safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;

                                if (result == kSaveId)
                                {
                                    safe->promptForName();
                                }
                                else if (result == kFolderId)
                                {
                                    const auto dir = PresetManager::getUserPresetDirectory();
                                    dir.createDirectory();
                                    dir.revealToUser();
                                }
                                else
                                {
                                    safe->manager.apply (result - 1);
                                    safe->refresh();
                                }
                            });
    }

    void PresetBar::promptForName()
    {
        auto* window = new juce::AlertWindow ("Save preset", "Name for the new user preset:", juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor ("name", manager.isModified() ? juce::String() : manager.getCurrentName());
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        window->enterModalState (true,
                                 juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PresetBar> (this), window] (int result)
                                 {
                                     if (result == 1 && safe != nullptr)
                                     {
                                         const auto entered = window->getTextEditorContents ("name");
                                         if (! safe->manager.saveUserPreset (entered))
                                             juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset not saved",
                                                                                      "Choose a name that is not empty and not the name of a factory preset.");
                                         safe->refresh();
                                     }
                                 }),
                                 true);
    }
}
