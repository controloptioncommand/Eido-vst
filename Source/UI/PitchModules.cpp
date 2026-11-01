#include "Module.h"
#include "Widgets.h"

namespace eido
{
using LnF = EidoLookAndFeel;

// ===========================================================================
// Tuner
TunerModule::TunerModule (const SignalBuffer& s) : Module (ModuleType::Tuner, s) {}

void TunerModule::tick()
{
    if ((++ticks % 2) != 0) return;   // 30 оценок в секунду
    const auto r = detector.analyse (sig);
    if (r.found && r.clarity > 0.8f)
    {
        // новая нота (скачок больше 60 центов) — история начинается заново, чтобы тюнер отвечал сразу
        if (! history.empty() && std::abs (1200.0f * std::log2 (r.freq / freq)) > 60.0f) history.clear();
        history.push_back (r.freq);
        while (history.size() > 5) history.pop_front();
        std::vector<float> sorted (history.begin(), history.end());
        std::sort (sorted.begin(), sorted.end());
        freq = sorted[sorted.size() / 2];
        live = true;
        heldFor = 0;
    }
    else if (++heldFor > 30)   // ~1 с без тона — последняя нота остаётся бледной
    {
        live = false;
        history.clear();
    }
    const float target = live ? PitchDetector::centsOff (freq, a4) : 0.0f;
    shownCents += (target - shownCents) * (Accessibility::reduceMotion() ? 1.0f : 0.5f);
    setDescription (freq > 0.0f ? "Tuner " + getNote() + ", " + juce::String (juce::roundToInt (freq)) + " hertz, "
                                    + juce::String (juce::roundToInt (PitchDetector::centsOff (freq, a4))) + " cents"
                                : juce::String ("Tuner, no pitch"));
}

void TunerModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto area = r.reduced (14.0f, isCompact() ? 10.0f : 14.0f).withTrimmedBottom (isCompact() ? 0.0f : 40.0f);
    const bool has = freq > 0.0f;
    const float cents = has ? PitchDetector::centsOff (freq, a4) : 0.0f;
    const bool inTune = live && std::abs (cents) <= 5.0f;

    // нота крупно
    const float noteH = juce::jlimit (20.0f, 72.0f, area.getHeight() * 0.38f);
    const float freqH = 18.0f, scaleH = 30.0f, gap = juce::jmin (10.0f, area.getHeight() * 0.05f);
    const float total = noteH + gap + freqH + gap + scaleH;
    auto col = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (total, area.getHeight()));
    auto noteArea = col.removeFromTop (noteH);
    g.setColour (live ? Colours::ink : Colours::ink2);
    g.setFont (LnF::font (noteH * 0.95f, true));
    g.drawText (has ? getNote() : juce::String::fromUTF8 ("\xe2\x80\x94"), noteArea, juce::Justification::centred);

    // частота целым числом и отклонение
    col.removeFromTop (gap);
    auto freqArea = col.removeFromTop (freqH);
    g.setFont (LnF::mono (LnF::Type::title3));
    g.setColour (live ? Colours::ink : Colours::ink2);
    juce::String line = has ? juce::String (juce::roundToInt (freq)) + " Hz" : juce::String ("No pitch");
    if (has) line << midDot() << (cents < -0.5f ? minus() : juce::String ("+")) << std::abs (juce::roundToInt (cents)) << " ct";
    g.drawText (line, freqArea, juce::Justification::centred);

    // шкала центов −50…+50
    col.removeFromTop (gap);
    auto scale = col.removeFromTop (scaleH);
    scale = scale.withSizeKeepingCentre (juce::jmin (scale.getWidth(), 360.0f), scale.getHeight());
    auto track = scale.removeFromTop (14.0f);
    const float y = track.getCentreY();
    auto xOf = [&] (float c) { return track.getX() + track.getWidth() * (c + 50.0f) / 100.0f; };
    g.setColour (Colours::track);
    g.fillRoundedRectangle (track.withSizeKeepingCentre (track.getWidth(), 2.0f), 1.0f);
    for (float c : { -50.0f, -25.0f, -10.0f, 0.0f, 10.0f, 25.0f, 50.0f })
    {
        const float h = c == 0.0f ? 12.0f : (std::abs (c) == 50.0f ? 8.0f : 6.0f);
        g.setColour (c == 0.0f ? Colours::ink : Colours::ink2);
        g.fillRect (juce::Rectangle<float> (1.0f, h).withCentre ({ xOf (c), y }));
    }
    if (has)
    {
        g.setColour (! live ? Colours::ink2 : inTune ? Colours::live : Colours::signal);
        g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, 16.0f).withCentre ({ xOf (juce::jlimit (-50.0f, 50.0f, shownCents)), y }), 2.0f);
    }
    if (scale.getHeight() >= 12.0f)
    {
        g.setFont (LnF::mono (LnF::Type::footnote));
        g.setColour (Colours::ink2);
        g.drawText (minus() + "50", scale.withWidth (40.0f).translated (-8.0f, 0.0f), juce::Justification::centredLeft);
        g.drawText ("0", scale.withSizeKeepingCentre (20.0f, scale.getHeight()), juce::Justification::centred);
        g.drawText ("+50", scale.withTrimmedLeft (scale.getWidth() - 40.0f).translated (8.0f, 0.0f), juce::Justification::centredRight);
    }

    // эталон, если не стандартный
    if (a4 != 440.0f && r.getHeight() >= 120.0f)
    {
        g.setFont (LnF::mono (LnF::Type::footnote));
        g.setColour (Colours::ink2);
        g.drawText ("A4 = " + juce::String ((int) a4) + " Hz", r.reduced (12.0f).removeFromTop (14.0f), juce::Justification::topLeft);
    }
}

void TunerModule::addMenuItems (juce::PopupMenu& m)
{
    juce::PopupMenu ref;
    const int refs[] = { 432, 440, 442, 444 };
    for (int i = 0; i < 4; ++i)
        ref.addItem (1100 + i, "A4 = " + juce::String (refs[i]) + " Hz", true, std::abs (a4 - (float) refs[i]) < 0.1f);
    m.addSubMenu ("Reference", ref);
}

void TunerModule::handleMenuResult (int id)
{
    const int refs[] = { 432, 440, 442, 444 };
    if (id >= 1100 && id < 1104) { a4 = (float) refs[id - 1100]; repaint(); }
}

// ===========================================================================
// Oscilloscope
OscilloscopeModule::OscilloscopeModule (const SignalBuffer& s) : Module (ModuleType::Oscilloscope, s) {}

void OscilloscopeModule::tick()
{
    if ((++ticks % 2) == 0)
    {
        const auto r = detector.analyse (sig);
        pitch = r.found && r.clarity > 0.7f ? r.freq : 0.0f;
    }
    const double sr = sig.sampleRate;
    // окно: целое число периодов, не короче ~8 мс; без тона — 20 мс
    int periods = 0;
    double span = 0.02 * sr;
    if (pitch > 0.0f)
    {
        periods = juce::jlimit (2, 16, juce::roundToInt (0.008 * pitch));
        span = periods * sr / pitch;
    }
    spanMs = (float) (span / sr * 1000.0);

    // запуск: последний переход через ноль снизу вверх, после которого есть целое окно
    const juce::int64 latest = sig.written - (juce::int64) std::ceil (span) - 2;
    const int search = pitch > 0.0f ? (int) std::ceil (sr / pitch) + 2 : 2048;
    if (latest - search < 0) return;
    double start = (double) latest;
    for (juce::int64 i = latest; i > latest - search; --i)
    {
        const float a = sig.mono (i - 1), b = sig.mono (i);
        if (a <= 0.0f && b > 0.0f) { start = (double) (i - 1) + (double) (-a / (b - a)); break; }
    }
    const int n = 512;
    trace.resize ((size_t) n);
    for (int k = 0; k < n; ++k)
    {
        const double pos = start + span * (double) k / (double) (n - 1);
        const auto i0 = (juce::int64) std::floor (pos);
        const float f = (float) (pos - (double) i0);
        trace[(size_t) k] = sig.mono (i0) * (1.0f - f) + sig.mono (i0 + 1) * f;
    }
    juce::ignoreUnused (periods);
}

void OscilloscopeModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float m = juce::jlimit (6.0f, 46.0f, r.getHeight() * 0.17f);
    const auto area = r.reduced (12.0f, m);
    const float mid = area.getCentreY(), hh = area.getHeight() * 0.5f;

    // центр и пунктир −6 / −12 дБ, как у Scope
    g.setColour (Colours::outline);
    g.drawHorizontalLine (juce::roundToInt (mid), area.getX(), area.getRight());
    const float dashes[] = { 3.0f, 4.0f };
    g.setColour (Colours::ink2.withAlpha (0.45f));
    for (float db : { -6.0f, -12.0f })
    {
        const float a = juce::Decibels::decibelsToGain (db) * hh;
        for (float y : { mid - a, mid + a })
            g.drawDashedLine ({ area.getX(), y, area.getRight(), y }, dashes, 2, 1.0f);
    }

    if (trace.size() > 1)
    {
        juce::Path p;
        float peak = 0.0f;
        for (size_t k = 0; k < trace.size(); ++k)
        {
            const float x = area.getX() + area.getWidth() * (float) k / (float) (trace.size() - 1);
            const float y = mid - juce::jlimit (-1.0f, 1.0f, trace[k]) * hh;
            peak = juce::jmax (peak, std::abs (trace[k]));
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        // цвет линии — рампа по пику окна
        g.setColour (rampColour (Colours::carrierRamp, (juce::Decibels::gainToDecibels (peak, -100.0f) + 24.0f) / 24.0f));
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // подпись: нота и окно
    if (r.getHeight() >= 90.0f)
    {
        g.setFont (LnF::mono (LnF::Type::footnote));
        g.setColour (Colours::ink2);
        const auto txt = pitch > 0.0f ? PitchDetector::noteName (pitch) + midDot() + juce::String (juce::roundToInt (pitch)) + " Hz"
                                        + midDot() + juce::String (spanMs, 1) + " ms"
                                      : juce::String (spanMs, 0) + " ms";
        g.drawText (txt, r.reduced (12.0f, 10.0f).removeFromTop (14.0f), juce::Justification::topLeft);
    }
}
} // namespace eido
