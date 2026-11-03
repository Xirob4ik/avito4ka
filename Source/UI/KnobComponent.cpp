#include "KnobComponent.h"

#include "Fonts.h"
#include "Theme.h"

namespace clipline::ui
{
    using namespace theme;

    namespace
    {
        constexpr float kStartAngle = -juce::MathConstants<float>::pi * 0.75f;     // -135 deg from 12 o'clock
        constexpr float kEndAngle   =  juce::MathConstants<float>::pi * 0.75f;     // +135 deg
        constexpr float kPixelsForFullRange = 220.0f;
        constexpr float kFineFactor = 0.1f;

        float angleFor (float norm) noexcept { return kStartAngle + norm * (kEndAngle - kStartAngle); }
    }

    KnobComponent::KnobComponent (juce::RangedAudioParameter& p, juce::String t, Style s, juce::String tip)
        : parameter (p),
          title (std::move (t)),
          style (s),
          attachment (p, [this] (float plain)
                      {
                          norm = parameter.convertTo0to1 (plain);
                          repaint();
                      })
    {
        setTooltip (std::move (tip));
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (false);
        attachment.sendInitialUpdate();
    }

    KnobComponent::~KnobComponent() = default;

    //==============================================================================================
    void KnobComponent::resized()
    {
        auto b = getLocalBounds().toFloat();
        // Title and value sit on the same lines for every knob; only the dial diameter differs, which
        // keeps the row on one grid while CLIP clearly dominates.
        titleBounds = b.removeFromTop (14.0f);
        valueBounds = b.removeFromBottom (22.0f);

        auto dial = b.reduced (0.0f, 4.0f);
        const float d = std::min ({ dial.getWidth(), dial.getHeight(), style.primary ? 112.0f : 78.0f });
        dialBounds = juce::Rectangle<float> (d, d).withCentre (dial.getCentre());

        if (textEditor != nullptr)
            textEditor->setBounds (valueBounds.toNearestInt().expanded (6, 1));
    }

    void KnobComponent::paint (juce::Graphics& g)
    {
        const bool active = hovering || dragging;

        // ---- title ---------------------------------------------------------------------------------
        g.setColour (active ? text : textDim);
        g.setFont (fonts::sans (style.primary ? 11.0f : 10.0f, fonts::Weight::Medium, 0.16f));
        g.drawText (title, titleBounds, juce::Justification::centred, false);

        // ---- dial ----------------------------------------------------------------------------------
        const auto c = dialBounds.getCentre();
        const float R = dialBounds.getWidth() * 0.5f;
        const float stroke = style.primary ? 3.5f : 2.5f;
        const float arcR = R - stroke * 0.5f - 0.5f;

        auto arc = [&] (float fromNorm, float toNorm)
        {
            juce::Path p;
            if (toNorm < fromNorm) std::swap (fromNorm, toNorm);
            if (toNorm - fromNorm < 0.0005f)
                return p;
            p.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, angleFor (fromNorm), angleFor (toNorm), true);
            return p;
        };

        const juce::PathStrokeType strokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        g.setColour (hairline);
        g.strokePath (arc (0.0f, 1.0f), strokeType);

        const float from = style.bipolar ? 0.5f : (style.fillFromMax ? 1.0f : 0.0f);
        const auto valueColour = style.primary || active ? accent : text.withAlpha (0.82f);
        g.setColour (valueColour);
        g.strokePath (arc (from, norm), strokeType);

        if (style.bipolar)       // centre mark so 0 dB is easy to find
        {
            g.setColour (textFaint);
            g.fillRect (c.x - 0.5f, c.y - R - 3.0f, 1.0f, 3.0f);
        }

        // body + pointer
        const float bodyR = R * (style.primary ? 0.72f : 0.70f);
        g.setColour (surfaceHi);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (active ? hairline.brighter (0.25f) : hairline);
        g.drawEllipse (c.x - bodyR + 0.5f, c.y - bodyR + 0.5f, bodyR * 2.0f - 1.0f, bodyR * 2.0f - 1.0f, 1.0f);

        const float a = angleFor (norm);
        const float sx = std::sin (a), sy = -std::cos (a);
        g.setColour (style.primary || active ? accent : text);
        g.drawLine (c.x + sx * bodyR * 0.30f, c.y + sy * bodyR * 0.30f,
                    c.x + sx * bodyR * 0.88f, c.y + sy * bodyR * 0.88f,
                    style.primary ? 2.6f : 2.0f);

        // ---- value ---------------------------------------------------------------------------------
        if (textEditor == nullptr)
        {
            g.setColour (active ? text : text.withAlpha (0.9f));
            g.setFont (fonts::mono (style.primary ? 14.0f : 12.5f, true));
            g.drawText (parameter.getText (norm, 0), valueBounds, juce::Justification::centred, false);
        }
    }

    //==============================================================================================
    bool KnobComponent::isOnValue (juce::Point<float> p) const noexcept
    {
        return valueBounds.expanded (8.0f, 2.0f).contains (p);
    }

    void KnobComponent::mouseEnter (const juce::MouseEvent& e)
    {
        hovering = true;
        setMouseCursor (isOnValue (e.position) ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::UpDownResizeCursor);
        repaint();
    }

    void KnobComponent::mouseMove (const juce::MouseEvent& e)
    {
        setMouseCursor (isOnValue (e.position) ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::UpDownResizeCursor);
    }

    void KnobComponent::mouseExit (const juce::MouseEvent&)
    {
        hovering = false;
        repaint();
    }

    void KnobComponent::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            return;

        if (textEditor != nullptr && ! textEditor->getBounds().toFloat().contains (e.position))
            closeTextEntryAsync();

        pressedOnValue = isOnValue (e.position);
        if (pressedOnValue)
            return;                                  // a click on the number opens text entry on mouse-up

        dragging = true;
        dragNorm = norm;
        lastDragDistance = 0.0f;
        attachment.beginGesture();
        e.source.enableUnboundedMouseMovement (true, false);
        repaint();
    }

    void KnobComponent::mouseDrag (const juce::MouseEvent& e)
    {
        if (! dragging)
            return;

        const float distance = (float) -e.getDistanceFromDragStartY();         // up = increase
        const float delta = distance - lastDragDistance;
        lastDragDistance = distance;

        const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
        dragNorm = juce::jlimit (0.0f, 1.0f, dragNorm + delta / kPixelsForFullRange * (fine ? kFineFactor : 1.0f));
        setNormalisedAsPartOfGesture (dragNorm);
    }

    void KnobComponent::mouseUp (const juce::MouseEvent& e)
    {
        if (pressedOnValue)
        {
            pressedOnValue = false;
            if (! e.mouseWasDraggedSinceMouseDown() && isOnValue (e.position) && ! e.mods.isPopupMenu())
                showTextEntry();
            return;
        }

        if (dragging)
        {
            dragging = false;
            attachment.endGesture();
            e.source.enableUnboundedMouseMovement (false);
            repaint();
        }
    }

    void KnobComponent::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            return;

        closeTextEntryAsync();

        if (dragging)       // the second press of the double-click already opened a gesture
            attachment.setValueAsPartOfGesture (parameter.convertFrom0to1 (parameter.getDefaultValue()));
        else
            resetToDefault();
    }

    void KnobComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (dragging || juce::exactlyEqual (wheel.deltaY, 0.0f))
            return;

        const float direction = wheel.isReversed ? -1.0f : 1.0f;
        const float step = wheel.deltaY * direction * 0.15f * (e.mods.isShiftDown() ? 0.2f : 1.0f);

        attachment.beginGesture();
        setNormalisedAsPartOfGesture (norm + step);
        attachment.endGesture();
    }

    //==============================================================================================
    void KnobComponent::setNormalisedAsPartOfGesture (float newNorm)
    {
        const auto& range = parameter.getNormalisableRange();
        const float plain = range.snapToLegalValue (parameter.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, newNorm)));
        attachment.setValueAsPartOfGesture (plain);
    }

    void KnobComponent::resetToDefault()
    {
        attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (parameter.getDefaultValue()));
    }

    bool KnobComponent::commitText (const juce::String& entered)
    {
        // Ignore text without any number in it (typing "abc" must not slam the knob to its minimum);
        // "-inf" is accepted and means "as low as it goes".
        if (! entered.containsAnyOf ("0123456789") && ! entered.containsIgnoreCase ("inf"))
            return false;

        const auto& range = parameter.getNormalisableRange();
        const float plain = range.snapToLegalValue (parameter.convertFrom0to1 (parameter.getValueForText (entered)));
        attachment.setValueAsCompleteGesture (plain);
        return true;
    }

    void KnobComponent::showTextEntry()
    {
        if (textEditor != nullptr)
            return;

        textEditor = std::make_unique<juce::TextEditor>();
        textEditor->setFont (fonts::mono (style.primary ? 14.0f : 12.5f, true));
        textEditor->setJustification (juce::Justification::centred);
        textEditor->setIndents (2, 1);
        textEditor->setBorder ({});
        textEditor->setSelectAllWhenFocused (true);
        textEditor->setText (parameter.getText (norm, 0), false);
        textEditor->setBounds (valueBounds.toNearestInt().expanded (6, 1));

        textEditor->onReturnKey = [this]
        {
            if (textEditor != nullptr)
            {
                textEditor->onFocusLost = nullptr;          // commit exactly once
                commitText (textEditor->getText());
            }
            closeTextEntryAsync();
        };
        textEditor->onEscapeKey = [this]
        {
            if (textEditor != nullptr)
                textEditor->onFocusLost = nullptr;          // Esc cancels: losing focus must not commit
            closeTextEntryAsync();
        };
        textEditor->onFocusLost = [this]
        {
            if (textEditor != nullptr)
                commitText (textEditor->getText());
            closeTextEntryAsync();
        };

        addAndMakeVisible (*textEditor);
        textEditor->grabKeyboardFocus();
        textEditor->selectAll();
        repaint();
    }

    void KnobComponent::closeTextEntryAsync()
    {
        if (textEditor == nullptr)
            return;

        // The editor may be the object currently running one of these callbacks: destroy it later.
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<KnobComponent> (this)]
        {
            if (safe != nullptr)
            {
                safe->textEditor.reset();
                safe->repaint();
            }
        });
    }
}
