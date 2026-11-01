#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/Module.h"
#include "UI/About.h"

namespace eido
{
class EidoEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::DarkModeSettingListener
{
public:
    explicit EidoEditor (EidoProcessor&);
    ~EidoEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    enum class Layout { Strip = 0, Grid = 1 };
    static constexpr int kSlots = 8;
    static constexpr int kStripW = 1600, kStripH = 240, kGridW = 1400, kGridH = 720, kGridCols = 4;

    void setLayout (Layout l);
    Layout getLayout() const { return layout; }
    void setModule (int slot, ModuleType t);
    void moveModule (int slot, int dir);

    // для тестов
    void setThemeForTest (ThemeId id) { setThemeMode (id == ThemeId::Rubber ? ThemeMode::Rubber : ThemeMode::Oxygen); }
    Module* getModule (int slot) { return modules[(size_t) slot].get(); }
    AboutPage& getAboutForTest() { return about; }
    void showAboutForTest (bool show, bool game) { showAbout (show); if (show) about.showGame (game); }
    void pumpForTest() { timerCallback(); }

private:
    void timerCallback() override;
    void darkModeSettingChanged() override;
    void showAbout (bool show);
    void showSettingsMenu();
    void showModuleMenu (Module& m);
    void setThemeMode (ThemeMode m);
    void applyThemeMode();
    void setPaused (bool p);
    void resetMeters();
    void rebuildModule (int slot, ModuleType t);
    void layoutModules();
    void saveSlots();
    int slotOf (const Module& m) const;
    int slotAt (juce::Point<int> p) const;

    EidoProcessor& proc;
    EidoLookAndFeel lnf;
    ThemeMode themeMode = ThemeMode::System;
    Layout layout = Layout::Strip;
    bool paused = false, sizeOverridden = false, settingSize = false;
    juce::Rectangle<int> sizeBeforeAbout;

    SignalBuffer signal;

    // тулбар
    LogoButton logo;
    Segmented layoutSwitch;
    IconButton pauseButton { IconButton::Pause, "Pause" }, resetButton { IconButton::Reset, "Reset meters" },
               settingsButton { IconButton::Settings, "Settings" };
    juce::Rectangle<int> toolbarArea, rackArea, statusArea;
    juce::String statusText;

    std::array<std::unique_ptr<Module>, kSlots> modules;
    int dropTarget = -1;   // слот под перетаскиваемым модулем
    AboutPage about;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EidoEditor)
};
} // namespace eido
