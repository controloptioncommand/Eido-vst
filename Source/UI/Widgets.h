#pragma once
#include "LookAndFeel.h"

namespace eido
{
// Сегментированный переключатель (капсула)
class Segmented : public juce::Component
{
public:
    struct Item { juce::String text; int value; };
    Segmented();
    void setItems (std::vector<Item> it) { items = std::move (it); repaint(); }
    void setSelectedValue (int v) { if (v != selected) { selected = v; repaint(); } }
    int getSelectedValue() const { return selected; }
    std::function<void (int)> onChange;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    void choose (int index);
    std::vector<Item> items;
    int selected = 0;
};

// Логотип Eido (Medeo Pixel) — открывает About
class LogoButton : public juce::Button
{
public:
    LogoButton() : juce::Button ("Eido") { setTitle ("About Eido"); }
    void paintButton (juce::Graphics& g, bool over, bool down) override;
};

class IconButton : public juce::Button
{
public:
    enum Kind { Settings, Pause, Play, Reset };
    IconButton (Kind k, const juce::String& title) : juce::Button (title), kind (k) { setTitle (title); }
    void setKind (Kind k) { kind = k; repaint(); }
    void paintButton (juce::Graphics& g, bool over, bool down) override;

private:
    Kind kind;
};

void disableClickFocus (juce::Component& root);
void repaintTree (juce::Component& root);
juce::String minus();                       // типографский минус
juce::String formatDb (float db, int decimals = 1, float silent = -99.0f);   // «−14.2», «−∞»
} // namespace eido
