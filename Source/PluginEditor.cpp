#include "PluginEditor.h"

namespace eido
{
using LnF = EidoLookAndFeel;

namespace
{
const ModuleType kDefaultSlots[EidoEditor::kSlots] = { ModuleType::Spectrogram, ModuleType::Scope, ModuleType::Oscilloscope,
                                                       ModuleType::Spectrum, ModuleType::Loudness, ModuleType::Stereo,
                                                       ModuleType::VU, ModuleType::Tuner };

// относительная ширина модуля в полосе
float stripWeight (ModuleType t)
{
    switch (t)
    {
        case ModuleType::Loudness: return 0.85f;
        case ModuleType::VU:       return 1.0f;
        case ModuleType::Stereo:   return 0.9f;
        case ModuleType::Spectrum: return 1.7f;
        case ModuleType::MidSide:  return 1.7f;
        case ModuleType::Tuner:    return 1.0f;
        case ModuleType::LoudnessHistory: return 1.6f;
        default:                   return 1.5f;
    }
}

std::unique_ptr<juce::PropertiesFile> openPrefs()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Eido";
    o.filenameSuffix = "settings";
    o.folderName = "Eido";
    o.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (o);
}
} // namespace

EidoEditor::EidoEditor (EidoProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    Accessibility::refresh();
    themeMode = (ThemeMode) juce::jlimit (0, 2, openPrefs()->getIntValue ("themeMode", 0));
    applyThemeMode();
    setLookAndFeel (&lnf);
    juce::Desktop::getInstance().addDarkModeSettingListener (this);
    setTitle ("Eido");

    // тулбар
    for (juce::Component* c : std::initializer_list<juce::Component*> { &logo, &layoutSwitch, &pauseButton, &resetButton, &settingsButton })
        addAndMakeVisible (c);
    logo.onClick = [this] { showAbout (true); };
    layoutSwitch.setTitle ("Layout");
    layoutSwitch.setItems ({ { "Strip", (int) Layout::Strip }, { "Grid", (int) Layout::Grid } });
    layoutSwitch.onChange = [this] (int v) { setLayout ((Layout) v); };
    pauseButton.setDescription ("Freezes the meters. Audio is not affected.");
    pauseButton.onClick = [this] { setPaused (! paused); };
    resetButton.setDescription ("Resets integrated loudness, peak holds and history.");
    resetButton.onClick = [this] { resetMeters(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    // модули из состояния проекта
    auto& st = proc.uiState;
    const auto keys = juce::StringArray::fromTokens (st.getProperty ("slots").toString(), ",", "");
    for (int i = 0; i < kSlots; ++i)
        rebuildModule (i, i < keys.size() ? moduleFromKey (keys[i], kDefaultSlots[i]) : kDefaultSlots[i]);
    layout = (Layout) juce::jlimit (0, 1, (int) st.getProperty ("layout", 0));
    layoutSwitch.setSelectedValue ((int) layout);

    addChildComponent (about);
    about.onClose = [this] { showAbout (false); };
    disableClickFocus (*this);

    settingSize = true;   // ограничения размера сами вызывают resized() — это не выбор пользователя
    setResizable (true, true);
    setResizeLimits (560, 170, 4000, 2400);
    const bool strip = layout == Layout::Strip;
    setSize ((int) st.getProperty (strip ? "stripW" : "gridW", strip ? kStripW : kGridW),
             (int) st.getProperty (strip ? "stripH" : "gridH", strip ? kStripH : kGridH));
    settingSize = false;
    startTimerHz (60);
}

EidoEditor::~EidoEditor()
{
    stopTimer();
    juce::Desktop::getInstance().removeDarkModeSettingListener (this);
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
// Тема: по системе (светлая/тёмная macOS) или вручную
void EidoEditor::applyThemeMode()
{
    ThemeId id = ThemeId::Oxygen;
    if (themeMode == ThemeMode::Rubber) id = ThemeId::Rubber;
    else if (themeMode == ThemeMode::System) id = juce::Desktop::getInstance().isDarkModeActive() ? ThemeId::Rubber : ThemeId::Oxygen;
    applyTheme (id);
    lnf.refreshColours();
    sendLookAndFeelChange();
    repaintTree (*this);
}

void EidoEditor::setThemeMode (ThemeMode m)
{
    themeMode = m;
    if (auto p = openPrefs()) { p->setValue ("themeMode", (int) m); p->saveIfNeeded(); }
    applyThemeMode();
}

void EidoEditor::darkModeSettingChanged()
{
    if (themeMode == ThemeMode::System) applyThemeMode();
}

// ---------------------------------------------------------------------------
void EidoEditor::rebuildModule (int slot, ModuleType t)
{
    auto& m = modules[(size_t) slot];
    if (m != nullptr) removeChildComponent (m.get());
    m = createModule (t, signal, proc.loudness);
    m->loadSettings (proc.uiState);
    m->onMove = [this] (Module& mod, int dir) { moveModule (slotOf (mod), dir); };
    m->onDrag = [this] (Module& mod, juce::Point<int> p)
    {
        const int t = slotAt (p);
        const int target = t == slotOf (mod) ? -1 : t;
        if (target != dropTarget) { dropTarget = target; repaint(); }
    };
    m->onDrop = [this] (Module& mod, juce::Point<int> p)
    {
        const int from = slotOf (mod), to = slotAt (p);
        dropTarget = -1;
        if (to >= 0 && to != from)
        {
            std::swap (modules[(size_t) from], modules[(size_t) to]);
            saveSlots();
            layoutModules();
        }
        repaint();
    };
    m->onMenu = [this] (Module& mod) { showModuleMenu (mod); };
    m->setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (*m);
    if (about.isVisible()) about.toFront (false);
}

int EidoEditor::slotAt (juce::Point<int> p) const
{
    for (int i = 0; i < kSlots; ++i) if (modules[(size_t) i]->getBounds().contains (p)) return i;
    return -1;
}

int EidoEditor::slotOf (const Module& m) const
{
    for (int i = 0; i < kSlots; ++i) if (modules[(size_t) i].get() == &m) return i;
    return 0;
}

void EidoEditor::saveSlots()
{
    juce::StringArray keys;
    for (auto& m : modules) keys.add (moduleKey (m->getType()));
    proc.uiState.setProperty ("slots", keys.joinIntoString (","), nullptr);
}

void EidoEditor::setModule (int slot, ModuleType t)
{
    if (modules[(size_t) slot]->getType() == t) return;
    rebuildModule (slot, t);
    saveSlots();
    layoutModules();
}

void EidoEditor::moveModule (int slot, int dir)
{
    const int other = slot + dir;
    if (other < 0 || other >= kSlots) return;
    std::swap (modules[(size_t) slot], modules[(size_t) other]);
    saveSlots();
    layoutModules();
    modules[(size_t) other]->grabKeyboardFocus();
}

void EidoEditor::setLayout (Layout l)
{
    if (l == layout) return;
    layout = l;
    layoutSwitch.setSelectedValue ((int) l);
    proc.uiState.setProperty ("layout", (int) l, nullptr);
    if (about.isVisible()) showAbout (false);
    const bool strip = l == Layout::Strip;
    setSize ((int) proc.uiState.getProperty (strip ? "stripW" : "gridW", strip ? kStripW : kGridW),
             (int) proc.uiState.getProperty (strip ? "stripH" : "gridH", strip ? kStripH : kGridH));
    resized();
}

void EidoEditor::setPaused (bool p)
{
    paused = p;
    pauseButton.setKind (p ? IconButton::Play : IconButton::Pause);
    pauseButton.setTitle (p ? "Resume" : "Pause");
}

void EidoEditor::resetMeters()
{
    proc.loudness.requestReset();
    for (auto& m : modules) { m->reset(); m->repaint(); }
}

// ---------------------------------------------------------------------------
void EidoEditor::showAbout (bool show)
{
    if (show == about.isVisible()) return;
    if (show)
    {
        // About просторная: если окно — узкая полоса, на время About окно увеличивается
        if (getWidth() < 900 || getHeight() < 600)
        {
            sizeBeforeAbout = getLocalBounds();
            sizeOverridden = true;
            setSize (juce::jmax (getWidth(), 900), juce::jmax (getHeight(), 600));
        }
        about.setBounds (getLocalBounds());
        about.setBackLogoBounds (logo.getBounds());
        about.setVisible (true);
        about.toFront (true);
        about.grabKeyboardFocus();   // чтобы Escape сразу возвращал назад
    }
    else
    {
        about.showGame (false);
        about.setVisible (false);
        if (sizeOverridden)
        {
            setSize (sizeBeforeAbout.getWidth(), sizeBeforeAbout.getHeight());
            sizeOverridden = false;
        }
    }
}

void EidoEditor::showModuleMenu (Module& mod)
{
    const int slot = slotOf (mod);
    juce::PopupMenu m;
    // модули по группам; текущий отмечен
    bool first = true;
    for (auto& grp : moduleGroups())
    {
        if (! first) m.addSeparator();
        first = false;
        m.addSectionHeader (grp.name);
        for (auto t : grp.types)
            m.addItem ((int) t + 1, moduleName (t), true, mod.getType() == t);
    }
    juce::PopupMenu extra;
    mod.addMenuItems (extra);
    if (extra.getNumItems() > 0) { m.addSeparator(); m.addSubMenu ("Options", extra); }
    m.addSeparator();
    m.addItem (50, "Move left", slot > 0);
    m.addItem (51, "Move right", slot < kSlots - 1);

    juce::Component::SafePointer<Module> safe (&mod);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mod), [this, safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        const int s = slotOf (*safe);
        if (r <= (int) ModuleType::Count) setModule (s, (ModuleType) (r - 1));
        else if (r == 50) moveModule (s, -1);
        else if (r == 51) moveModule (s, 1);
        else
        {
            safe->handleMenuResult (r);
            safe->saveSettings (proc.uiState);
        }
    });
}

void EidoEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    juce::PopupMenu appearance;
    appearance.addItem (201, "Match system", true, themeMode == ThemeMode::System);
    appearance.addSeparator();
    appearance.addItem (202, "Oxygen (light)", true, themeMode == ThemeMode::Oxygen);
    appearance.addItem (203, "Rubber (dark)", true, themeMode == ThemeMode::Rubber);
    m.addSubMenu ("Appearance", appearance);
    juce::PopupMenu lay;
    lay.addItem (301, "Strip", true, layout == Layout::Strip);
    lay.addItem (302, "Grid", true, layout == Layout::Grid);
    m.addSubMenu ("Layout", lay);
    m.addSeparator();
    m.addItem (1, "Reset meters");
    m.addItem (2, "Default modules");
    m.addSeparator();
    m.addItem (5, "About Eido");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton), [this] (int r)
    {
        if (r == 1) resetMeters();
        else if (r == 2) { for (int i = 0; i < kSlots; ++i) rebuildModule (i, kDefaultSlots[i]); saveSlots(); layoutModules(); }
        else if (r == 5) showAbout (true);
        else if (r == 201) setThemeMode (ThemeMode::System);
        else if (r == 202) setThemeMode (ThemeMode::Oxygen);
        else if (r == 203) setThemeMode (ThemeMode::Rubber);
        else if (r == 301) setLayout (Layout::Strip);
        else if (r == 302) setLayout (Layout::Grid);
    });
}

// ---------------------------------------------------------------------------
void EidoEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::window);
    LnF::drawPlate (g, toolbarArea.toFloat(), 12.0f);
    g.setColour (Colours::outline);
    const int inset = layout == Layout::Strip ? 10 : 15;
    g.drawVerticalLine (logo.getRight() + 8, (float) toolbarArea.getY() + (float) inset, (float) toolbarArea.getBottom() - (float) inset);

    g.setFont (LnF::mono (LnF::Type::footnote));
    g.setColour (Colours::ink2);
    g.drawText (statusText, statusArea, juce::Justification::centredRight);
}

void EidoEditor::paintOverChildren (juce::Graphics& g)
{
    if (about.isVisible()) return;
    // рамки 1 px: в полосе — одна общая, в сетке — у каждой карточки
    g.setColour (Colours::outline);
    if (layout == Layout::Strip)
        g.drawRoundedRectangle (rackArea.toFloat().reduced (0.5f), 12.0f, 1.0f);
    else
        for (auto& m : modules)
            g.drawRoundedRectangle (m->getBounds().toFloat().reduced (0.5f), 12.0f, 1.0f);

    // куда упадёт перетаскиваемый модуль — рамка 2 px сигнального цвета
    if (dropTarget >= 0)
    {
        g.setColour (Colours::signal);
        g.drawRoundedRectangle (modules[(size_t) dropTarget]->getBounds().toFloat().reduced (1.0f), 11.0f, 2.0f);
    }
}

void EidoEditor::layoutModules()
{
    const bool strip = layout == Layout::Strip;
    if (strip)
    {
        // полоса: модули встык, между ними — 1 px фона окна
        float total = 0;
        for (auto& m : modules) total += stripWeight (m->getType());
        const float avail = (float) rackArea.getWidth() - (float) (kSlots - 1);
        float x = (float) rackArea.getX();
        for (int i = 0; i < kSlots; ++i)
        {
            auto& m = modules[(size_t) i];
            const float w = avail * stripWeight (m->getType()) / total;
            const int x0 = juce::roundToInt (x), x1 = i == kSlots - 1 ? rackArea.getRight() : juce::roundToInt (x + w);
            m->setCompact (true);
            m->setCorners (i == 0, i == kSlots - 1, i == 0, i == kSlots - 1);
            m->setBounds (x0, rackArea.getY(), x1 - x0, rackArea.getHeight());
            x += w + 1.0f;
        }
    }
    else
    {
        // сетка 4 × 2, карточки с радиусом 12
        const int gap = 10, cols = kGridCols, rows = 2;
        const int cw = (rackArea.getWidth() - gap * (cols - 1)) / cols, ch = (rackArea.getHeight() - gap * (rows - 1)) / rows;
        for (int i = 0; i < kSlots; ++i)
        {
            auto& m = modules[(size_t) i];
            const int c = i % cols, r = i / cols;
            m->setCompact (false);
            m->setCorners (true, true, true, true);
            m->setBounds (rackArea.getX() + c * (cw + gap), rackArea.getY() + r * (ch + gap),
                          c == cols - 1 ? rackArea.getRight() - (rackArea.getX() + c * (cw + gap)) : cw,
                          r == rows - 1 ? rackArea.getBottom() - (rackArea.getY() + r * (ch + gap)) : ch);
        }
    }
    repaint();
}

void EidoEditor::resized()
{
    const bool strip = layout == Layout::Strip;
    if (! settingSize && ! sizeOverridden && ! about.isVisible())
    {
        proc.uiState.setProperty (strip ? "stripW" : "gridW", getWidth(), nullptr);
        proc.uiState.setProperty (strip ? "stripH" : "gridH", getHeight(), nullptr);
    }

    // полоса: тулбар 44 (элементы 28); сетка: тулбар 52 (элементы 32), как в Mist
    const int margin = strip ? 8 : 14, barH = strip ? 44 : 52, ctl = strip ? 28 : 32;
    auto r = getLocalBounds().reduced (margin);
    toolbarArea = r.removeFromTop (barH);
    {
        auto h = toolbarArea.reduced (10, (barH - ctl) / 2);
        logo.setBounds (h.removeFromLeft (64));
        h.removeFromLeft (18);
        layoutSwitch.setBounds (h.removeFromLeft (150));
        h.removeFromLeft (10);
        pauseButton.setBounds (h.removeFromLeft (ctl));
        h.removeFromLeft (4);
        resetButton.setBounds (h.removeFromLeft (ctl));
        settingsButton.setBounds (h.removeFromRight (ctl));
        h.removeFromRight (10);
        statusArea = h.removeFromRight (120);
    }
    r.removeFromTop (strip ? 8 : 10);
    rackArea = r;
    layoutModules();

    if (about.isVisible())
    {
        about.setBounds (getLocalBounds());
        about.setBackLogoBounds (logo.getBounds());
    }
}

void EidoEditor::timerCallback()
{
    signal.sampleRate = proc.rate.load();
    const int ready = proc.tapFifo.getNumReady();
    if (ready > 0)
    {
        const auto rd = proc.tapFifo.read (ready);
        if (! paused && ! about.isVisible())
        {
            if (rd.blockSize1 > 0) signal.push (proc.tapL.data() + rd.startIndex1, proc.tapR.data() + rd.startIndex1, rd.blockSize1);
            if (rd.blockSize2 > 0) signal.push (proc.tapL.data() + rd.startIndex2, proc.tapR.data() + rd.startIndex2, rd.blockSize2);
        }
    }

    const auto st = juce::String (signal.sampleRate / 1000.0, signal.sampleRate == std::floor (signal.sampleRate / 1000.0) * 1000.0 ? 0 : 1) + " kHz";
    if (st != statusText) { statusText = st; repaint (statusArea); }

    if (paused || about.isVisible()) return;
    for (auto& m : modules)
    {
        m->tick();
        m->repaint();
    }
}
} // namespace eido
