#include "Module.h"
#include "Widgets.h"

namespace eido
{
using LnF = EidoLookAndFeel;

juce::String moduleName (ModuleType t)
{
    switch (t)
    {
        case ModuleType::Scope:       return "Scope";
        case ModuleType::Waves:       return "Waves";
        case ModuleType::Spectrogram: return "Spectrogram";
        case ModuleType::Spectrum:    return "Spectrum";
        case ModuleType::Loudness:    return "Loudness";
        case ModuleType::VU:          return "VU";
        case ModuleType::Stereo:      return "Stereo";
        case ModuleType::Tuner:       return "Tuner";
        case ModuleType::Oscilloscope: return "Oscilloscope";
        case ModuleType::MidSide:     return "Mid / Side";
        case ModuleType::LoudnessHistory: return "Loudness History";
        case ModuleType::PeakHistory: return "Peak History";
        default:                      return {};
    }
}

juce::String moduleKey (ModuleType t)
{
    switch (t)
    {
        case ModuleType::MidSide:         return "midside";
        case ModuleType::LoudnessHistory: return "loudnesshistory";
        case ModuleType::PeakHistory:     return "peakhistory";
        default:                          return moduleName (t).toLowerCase();
    }
}

const std::vector<ModuleGroup>& moduleGroups()
{
    using T = ModuleType;
    static const std::vector<ModuleGroup> groups {
        { "Wave",     { T::Scope, T::Waves, T::Oscilloscope, T::PeakHistory } },
        { "Spectrum", { T::Spectrum, T::MidSide, T::Spectrogram } },
        { "Loudness", { T::Loudness, T::LoudnessHistory, T::VU } },
        { "Stereo and pitch", { T::Stereo, T::Tuner } },
    };
    return groups;
}

ModuleType moduleFromKey (const juce::String& key, ModuleType fallback)
{
    for (int i = 0; i < (int) ModuleType::Count; ++i)
        if (moduleKey ((ModuleType) i) == key.trim().toLowerCase()) return (ModuleType) i;
    return fallback;
}

static juce::String moduleDescription (ModuleType t)
{
    switch (t)
    {
        case ModuleType::Scope:       return "Dense waveform, one bar per pixel column, coloured by level from -24 to 0 dB. Dashed guides at -6 and -12 dB.";
        case ModuleType::Waves:       return "Waveform history stacked as ridgelines. The front line is the current moment.";
        case ModuleType::Spectrogram: return "Spectrogram: time runs left to right, frequency bottom to top, colour is level.";
        case ModuleType::Spectrum:    return "Spectrum analyser. Hover to read level, frequency, note and cents.";
        case ModuleType::Loudness:    return "Peak meters for left and right, momentary and short-term loudness, integrated loudness in LUFS.";
        case ModuleType::VU:          return "Needle VU meter. 0 VU equals -18 dBFS RMS.";
        case ModuleType::Stereo:      return "Polar stereometer and phase correlation from -1 to +1.";
        case ModuleType::Tuner:       return "Tuner: note name, frequency in hertz and deviation in cents.";
        case ModuleType::Oscilloscope: return "Oscilloscope: a few periods of the waveform locked to the detected pitch.";
        case ModuleType::MidSide:     return "Mid and side spectra on one graph: shows which frequencies are wide.";
        case ModuleType::LoudnessHistory: return "Short-term and momentary loudness over the last 30 seconds, integrated loudness and loudness range.";
        case ModuleType::PeakHistory: return "Scrolling peak and RMS history, left channel above, right below.";
        default:                      return {};
    }
}

std::unique_ptr<Module> createModule (ModuleType t, const SignalBuffer& s, const LoudnessMeter& loud)
{
    switch (t)
    {
        case ModuleType::Scope:       return std::make_unique<ScopeModule> (s);
        case ModuleType::Waves:       return std::make_unique<WavesModule> (s);
        case ModuleType::Spectrogram: return std::make_unique<SpectrogramModule> (s);
        case ModuleType::Spectrum:    return std::make_unique<SpectrumModule> (s);
        case ModuleType::Loudness:    return std::make_unique<LoudnessModule> (s, loud);
        case ModuleType::VU:          return std::make_unique<VuModule> (s);
        case ModuleType::Stereo:      return std::make_unique<StereoModule> (s);
        case ModuleType::Tuner:       return std::make_unique<TunerModule> (s);
        case ModuleType::Oscilloscope: return std::make_unique<OscilloscopeModule> (s);
        case ModuleType::MidSide:     return std::make_unique<SpectrumModule> (s, true);
        case ModuleType::LoudnessHistory: return std::make_unique<LoudnessHistoryModule> (s, loud);
        case ModuleType::PeakHistory: return std::make_unique<PeakHistoryModule> (s);
        default:                      return std::make_unique<ScopeModule> (s);
    }
}

// ===========================================================================
Module::Module (ModuleType t, const SignalBuffer& s) : sig (s), cursor (s.written), type (t)
{
    setWantsKeyboardFocus (true);
    setTitle (moduleName (t));
    setDescription (moduleDescription (t) + " Drag it onto another module to swap them. Option and left or right arrow moves the module, Return opens the module menu.");
    setRepaintsOnMouseActivity (false);
}

juce::Colour Module::background() const
{
    // как главный вид Mist: Oxygen — Surface, Rubber — Well
    return currentTheme() == ThemeId::Rubber ? Colours::well : Colours::surface;
}

juce::Colour Module::plateFill() const
{
    return currentTheme() == ThemeId::Rubber ? Colours::surface : Colours::well;
}

void Module::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();
    const float m = compact ? 8.0f : 12.0f, h = 28.0f;
    controlsArea = { W - m - 88.0f, H - m - h, 88.0f, h };
    const float nameW = (float) juce::GlyphArrangement::getStringWidth (LnF::font (LnF::Type::subheadline), moduleName (type)) + 28.0f;
    nameChip = { m, H - m - h, nameW, h };
    const float lw = compact ? 196.0f : 236.0f;
    legendArea = { W - m - lw, m, lw, h };
}

void Module::paintLegend (juce::Graphics& g)
{
    LnF::drawPlate (g, legendArea, 8.0f, plateFill());
    auto r = legendArea.reduced (12.0f, 0.0f);
    g.setFont (LnF::mono (LnF::Type::footnote));
    g.setColour (Colours::ink);
    g.drawText (minus() + "24 dB", r.removeFromLeft (46.0f), juce::Justification::centredLeft);
    g.drawText ("0 dB", r.removeFromRight (30.0f), juce::Justification::centredRight);
    auto bar = r.reduced (6.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 12.0f, 6.0f);
    const float seg = bar.getWidth() / 10.0f;
    for (int b = 0; b < 10; ++b)
    {
        g.setColour (rampBucketColour (Colours::carrierRamp, b));
        g.fillRect (juce::Rectangle<float> (bar.getX() + seg * (float) b + 0.5f, bar.getY(), seg - 1.0f, bar.getHeight()));
    }
}

void Module::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rad = 12.0f;
    juce::Path shape;
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), rad, rad,
                               corners[0], corners[1], corners[2], corners[3]);
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (shape);
    g.fillAll (background());

    paintContent (g, r);

    // плашки: в сетке всегда, в полосе — при наведении; стрелки ‹ ▲ › — при наведении или фокусе
    const bool roomy = getWidth() >= 300 && getHeight() >= 120;
    if (chipsVisible())
    {
        if (wantsLegend() && roomy) paintLegend (g);
        if (getWidth() >= 200)
        {
            LnF::drawPlate (g, nameChip, 8.0f, plateFill());
            g.setFont (LnF::font (LnF::Type::subheadline));
            g.setColour (Colours::ink);
            g.drawText (moduleName (type), nameChip, juce::Justification::centred);
        }
    }
    paintTopRight (g, legendArea);

    if (controlsVisible())
    {
        LnF::drawPlate (g, controlsArea, controlsArea.getHeight() * 0.5f, plateFill());   // сегменты — капсула
        const float w = controlsArea.getWidth() / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            const auto c = juce::Rectangle<float> (controlsArea.getX() + w * (float) i, controlsArea.getY(), w, controlsArea.getHeight()).getCentre();
            juce::Path p;
            if (i == 1) p.addTriangle (c.x - 4.0f, c.y + 2.5f, c.x + 4.0f, c.y + 2.5f, c.x, c.y - 3.0f);
            else
            {
                const float d = i == 0 ? -1.0f : 1.0f;
                p.addTriangle (c.x + 3.0f * d, c.y, c.x - 2.0f * d, c.y - 4.0f, c.x - 2.0f * d, c.y + 4.0f);
            }
            g.setColour (Colours::ink);
            g.fillPath (p);
        }
        if (hasKeyboardFocus (false))
            LnF::drawFocusRing (g, controlsArea.expanded (2.0f), controlsArea.getHeight() * 0.5f + 2.0f);
    }
}

void Module::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    pressOnControls = controlsArea.contains (e.position) && controlsVisible();
    if (! pressOnControls) return;
    const int i = juce::jlimit (0, 2, (int) ((e.position.x - controlsArea.getX()) / controlsArea.getWidth() * 3.0f));
    if (i == 1) { if (onMenu) onMenu (*this); }
    else if (onMove) onMove (*this, i == 0 ? -1 : 1);
}

// Перетаскивание: тянем модуль за любое место (кроме стрелок) и бросаем на другой — они меняются местами
void Module::mouseDrag (const juce::MouseEvent& e)
{
    if (pressOnControls) return;
    if (! dragging && e.getDistanceFromDragStart() < 6) return;
    if (! dragging) { dragging = true; setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    if (onDrag) onDrag (*this, e.getEventRelativeTo (getParentComponent()).getPosition());
}

void Module::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    if (onDrop) onDrop (*this, e.getEventRelativeTo (getParentComponent()).getPosition());
}

bool Module::keyPressed (const juce::KeyPress& k)
{
    const bool mod = k.getModifiers().isAltDown() || k.getModifiers().isCommandDown();
    if (mod && k.getKeyCode() == juce::KeyPress::leftKey)  { if (onMove) onMove (*this, -1); return true; }
    if (mod && k.getKeyCode() == juce::KeyPress::rightKey) { if (onMove) onMove (*this, 1); return true; }
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { if (onMenu) onMenu (*this); return true; }
    return false;
}
} // namespace eido
