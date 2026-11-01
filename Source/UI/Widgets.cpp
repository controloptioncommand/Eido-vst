#include "Widgets.h"

namespace eido
{
using LnF = EidoLookAndFeel;

juce::String minus() { return juce::String::fromUTF8 ("\xe2\x88\x92"); }

juce::String formatDb (float db, int decimals, float silent)
{
    if (! std::isfinite (db) || db <= silent) return minus() + juce::String::fromUTF8 ("\xe2\x88\x9e");
    auto s = juce::String (std::abs (db), decimals);
    if (db < 0.0f && s != juce::String (0.0f, decimals)) s = minus() + s;
    return s;
}

// ===========================================================================
Segmented::Segmented() { setWantsKeyboardFocus (true); }

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (Colours::well);
    g.fillRoundedRectangle (r, rad);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    if (items.empty()) return;
    const float inset = 2.0f;
    const float w = (r.getWidth() - inset * 2) / (float) items.size();
    for (size_t i = 0; i < items.size(); ++i)
    {
        // концентрично: радиус сегмента = радиус дорожки − отступ
        auto cell = juce::Rectangle<float> (r.getX() + inset + w * (float) i, r.getY() + inset, w, r.getHeight() - inset * 2);
        const bool sel = items[i].value == selected;
        if (sel)
        {
            g.setColour (Colours::surface);
            g.fillRoundedRectangle (cell, rad - inset);
            g.setColour (Colours::outline);
            g.drawRoundedRectangle (cell.reduced (0.5f), rad - inset, 1.0f);
        }
        g.setColour (sel ? Colours::ink : Colours::ink2);
        g.setFont (LnF::font (LnF::Type::callout, sel));
        g.drawText (items[i].text, cell, juce::Justification::centred);
    }
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, r, rad);
}

void Segmented::choose (int i)
{
    if (items.empty()) return;
    i = juce::jlimit (0, (int) items.size() - 1, i);
    if (items[(size_t) i].value == selected) return;
    selected = items[(size_t) i].value;
    repaint();
    if (onChange) onChange (selected);
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    choose ((int) ((float) e.x / (float) getWidth() * (float) items.size()));
}

bool Segmented::keyPressed (const juce::KeyPress& k)
{
    int cur = 0;
    for (size_t i = 0; i < items.size(); ++i) if (items[i].value == selected) cur = (int) i;
    if (k.getKeyCode() == juce::KeyPress::leftKey)  { choose (cur - 1); return true; }
    if (k.getKeyCode() == juce::KeyPress::rightKey) { choose (cur + 1); return true; }
    return false;
}

// ===========================================================================
void LogoButton::paintButton (juce::Graphics& g, bool over, bool)
{
    // пиксель шрифта = 1 px окна (на Retina — 2 физических): логотип читается именно пиксельным
    // фокус — без обводки: логотип просто становится сигнальным (оранжевым)
    const auto c = hasKeyboardFocus (false) ? Colours::signal : over ? Colours::ink.withAlpha (0.75f) : Colours::ink;
    LnF::drawPixelText (g, "Eido", getLocalBounds(), 1, c);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (over || down)
    {
        g.setColour (Colours::ink.withAlpha (down ? 0.10f : 0.05f));
        g.fillRoundedRectangle (r, 8.0f);
    }
    g.setColour (Colours::ink);
    const auto c = r.getCentre();
    const auto stroke = juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    switch (kind)
    {
        case Settings:
        {
            auto ir = juce::Rectangle<float> (16.0f, 11.0f).withCentre (c);
            for (int i = 0; i < 2; ++i)
            {
                const float y = ir.getY() + (float) i * ir.getHeight();
                g.drawLine (ir.getX(), y, ir.getRight(), y, 1.4f);
                const float kx = i == 0 ? ir.getX() + 5.0f : ir.getRight() - 5.0f;
                g.setColour (Colours::surface);
                g.fillEllipse (kx - 2.6f, y - 2.6f, 5.2f, 5.2f);
                g.setColour (Colours::ink);
                g.drawEllipse (kx - 2.6f, y - 2.6f, 5.2f, 5.2f, 1.3f);
            }
            break;
        }
        case Pause:
            g.fillRoundedRectangle (c.x - 5.0f, c.y - 6.0f, 3.2f, 12.0f, 1.2f);
            g.fillRoundedRectangle (c.x + 1.8f, c.y - 6.0f, 3.2f, 12.0f, 1.2f);
            break;
        case Play:
        {
            juce::Path p;
            p.addTriangle (c.x - 4.0f, c.y - 6.0f, c.x - 4.0f, c.y + 6.0f, c.x + 6.0f, c.y);
            g.setColour (Colours::signal);   // на паузе — сигнальный: вид заморожен
            g.fillPath (p);
            break;
        }
        case Reset:
        {
            // круговая стрелка
            juce::Path p;
            const float rr = 6.0f;
            p.addCentredArc (c.x, c.y, rr, rr, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.2f, true);
            g.strokePath (p, stroke);
            const float a = 0.6f;
            const juce::Point<float> tip (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
            juce::Path head;
            head.startNewSubPath (tip.x - 3.5f, tip.y - 2.5f);
            head.lineTo (tip);
            head.lineTo (tip.x - 0.5f, tip.y + 4.0f);
            g.strokePath (head, stroke);
            break;
        }
    }
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, r, 8.0f);
}

// ===========================================================================
void disableClickFocus (juce::Component& root)
{
    root.setMouseClickGrabsKeyboardFocus (false);
    for (auto* c : root.getChildren()) disableClickFocus (*c);
}

void repaintTree (juce::Component& root)
{
    for (auto* c : root.getChildren()) repaintTree (*c);
    root.repaint();
}
} // namespace eido
