#include "Module.h"
#include "Widgets.h"

namespace eido
{
using LnF = EidoLookAndFeel;

namespace
{
constexpr float kTopDb = 0.0f, kRangeDb = 84.0f, kFMin = 20.0f, kFMax = 20000.0f;
float windowGain = 0.0f;   // когерентное усиление окна (сумма отсчётов / N)

juce::String noteName (float f)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
    const int n = juce::roundToInt (midi);
    const int cents = juce::roundToInt ((midi - (float) n) * 100.0f);
    const int oct = (int) std::floor ((float) n / 12.0f) - 1;
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (oct) + " ";
    s << (cents < 0 ? minus() : juce::String ("+")) << std::abs (cents) << " ct";
    return s;
}

juce::String freqText (float f)
{
    if (f >= 1000.0f) return juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (f, f >= 100.0f ? 1 : 2) + " Hz";
}
} // namespace

SpectrumModule::SpectrumModule (const SignalBuffer& s, bool ms)
    : Module (ms ? ModuleType::MidSide : ModuleType::Spectrum, s), midSide (ms)
{
    if (windowGain <= 0.0f)
    {
        std::vector<float> ones ((size_t) kSize, 1.0f);
        window.multiplyWithWindowingTable (ones.data(), (size_t) kSize);
        double sum = 0.0;
        for (float v : ones) sum += v;
        windowGain = (float) (sum / kSize);
    }
}

void SpectrumModule::tick()
{
    analyse (false, smooth);
    if (midSide) analyse (true, side);
}

void SpectrumModule::analyse (bool sideSignal, std::vector<float>& into)
{
    // каждый кадр — последние 8192 сэмпла; середина (L+R)/2 или сторона (L−R)/2
    const juce::int64 start = sig.written - kSize;
    if (start < 0) return;
    for (int i = 0; i < kSize; ++i)
        buf[(size_t) i] = sideSignal ? 0.5f * (sig.left (start + i) - sig.right (start + i)) : sig.mono (start + i);
    std::fill (buf.begin() + kSize, buf.end(), 0.0f);
    window.multiplyWithWindowingTable (buf.data(), (size_t) kSize);
    fft.performFrequencyOnlyForwardTransform (buf.data(), true);

    // синус с амплитудой A даёт A дБFS на своей частоте
    const float norm = 2.0f / ((float) kSize * windowGain);
    const float release = 30.0f / 60.0f;   // спад 30 дБ/с
    for (int b = 0; b < kSize / 2; ++b)
    {
        const float db = juce::Decibels::gainToDecibels (buf[(size_t) b] * norm, -120.0f);
        auto& v = into[(size_t) b];
        v = db > v ? v + (db - v) * 0.7f : juce::jmax (db, v - release);
    }
}

float SpectrumModule::freqAt (float x) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
    return kFMin * std::pow (kFMax / kFMin, t);
}

float SpectrumModule::dbAt (float f, const std::vector<float>& smooth) const
{
    const float bf = f / (float) sig.sampleRate * (float) kSize;
    const int b0 = juce::jlimit (0, kSize / 2 - 2, (int) bf);
    const float frac = juce::jlimit (0.0f, 1.0f, bf - (float) b0);
    return smooth[(size_t) b0] + (smooth[(size_t) b0 + 1] - smooth[(size_t) b0]) * frac;
}

float SpectrumModule::peakFrequency() const
{
    const float binHz = (float) sig.sampleRate / (float) kSize;
    int best = 2;
    for (int b = 2; b < kSize / 2 - 1; ++b)
        if (smooth[(size_t) b] > smooth[(size_t) best]) best = b;
    // параболическое уточнение по соседним бинам
    const float a = smooth[(size_t) best - 1], c = smooth[(size_t) best], d = smooth[(size_t) best + 1];
    const float denom = a - 2.0f * c + d;
    const float off = std::abs (denom) > 1.0e-6f ? 0.5f * (a - d) / denom : 0.0f;
    return ((float) best + off) * binHz;
}

juce::String SpectrumModule::readout (float x) const
{
    const float f = freqAt (x);
    if (midSide)
        return "Mid " + formatDb (dbAt (f, smooth), 1, -119.0f) + midDot() + "Side " + formatDb (dbAt (f, side), 1, -119.0f)
               + " dB" + midDot() + freqText (f);
    return formatDb (dbAt (f), 1, -119.0f) + " dB" + midDot() + freqText (f) + midDot() + noteName (f);
}

void SpectrumModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    const bool small = r.getHeight() < 120.0f;
    plot = r.reduced (1.0f).withTrimmedTop (small ? 18.0f : 24.0f).withTrimmedBottom (small ? 4.0f : 10.0f);
    const auto mapX = [this] (float f) { return plot.getX() + plot.getWidth() * std::log (f / kFMin) / std::log (kFMax / kFMin); };
    const auto mapY = [this] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, (kTopDb - db) / kRangeDb); };

    // сетка: частоты и шаг 12 дБ
    g.setColour (Colours::outline);
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine (juce::roundToInt (mapX (f)), plot.getY(), plot.getBottom());
    for (float db = -12.0f; db > -kRangeDb; db -= 12.0f)
        g.drawHorizontalLine (juce::roundToInt (mapY (db)), plot.getX(), plot.getRight());
    g.setFont (LnF::mono (LnF::Type::footnote));
    g.setColour (Colours::ink2);
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (mapX (f) + 4.0f, plot.getY() - 16.0f, 40.0f, 14.0f), juce::Justification::centredLeft);

    // кривая с наклоном tilt дБ/окт вокруг 1 кГц; заливка — рампа по высоте, 10 корзин
    juce::RectangleList<float> buckets[10];
    const auto curve = curvePath (smooth, buckets);
    const auto bg = background();
    for (int b = 0; b < 10; ++b)
    {
        if (buckets[b].isEmpty()) continue;
        g.setColour (bg.interpolatedWith (rampBucketColour (Colours::carrierRamp, b), 0.32f));
        g.fillRectList (buckets[b]);
    }
    g.setColour (Colours::ink.withAlpha (0.9f));
    g.strokePath (curve, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Mid / Side: сторона — линией цвета Modulator, без заливки; подписи сверху слева
    if (midSide)
    {
        g.setColour (Colours::modulator);
        g.strokePath (curvePath (side, nullptr), juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (r.getWidth() >= 220.0f)
        {
            auto key = juce::Rectangle<float> (plot.getX() + 8.0f, plot.getY() + 6.0f, 120.0f, 32.0f);
            g.setFont (LnF::font (LnF::Type::caption, true));
            for (auto [name, c] : { std::pair<const char*, juce::Colour> { "Mid", Colours::ink }, { "Side", Colours::modulator } })
            {
                auto row = key.removeFromTop (16.0f);
                g.setColour (c);
                g.fillRect (row.removeFromLeft (12.0f).withSizeKeepingCentre (12.0f, 2.0f));
                row.removeFromLeft (6.0f);
                g.setColour (Colours::ink2);
                g.drawText (name, row, juce::Justification::centredLeft);
            }
        }
    }

    // курсор: тонкая линия и точка на кривой
    if (isHovering() && plot.contains (getPointer()))
    {
        const float x = getPointer().x, f = freqAt (x);
        g.setColour (Colours::ink2.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        const float y = mapY (dbAt (f) + tiltPerOct * std::log2 (f / 1000.0f));
        g.setColour (Colours::signal);
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }
}

juce::Path SpectrumModule::curvePath (const std::vector<float>& curve, juce::RectangleList<float>* buckets) const
{
    const auto mapY = [this] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, (kTopDb - db) / kRangeDb); };
    const int W = juce::jmax (2, (int) plot.getWidth());
    const float binPerHz = (float) kSize / (float) sig.sampleRate;
    juce::Path path;
    for (int i = 0; i <= W; ++i)
    {
        const float x = plot.getX() + (float) i;
        const float f = freqAt (x);
        const float bA = freqAt (x - 0.5f) * binPerHz, bB = freqAt (x + 0.5f) * binPerHz;
        float db;
        if (bB - bA < 1.0f) db = dbAt (f, curve);
        else
        {
            db = -120.0f;
            for (int b = juce::jmax (1, (int) std::ceil (bA)); b <= juce::jmin (kSize / 2 - 1, (int) bB); ++b)
                db = juce::jmax (db, curve[(size_t) b]);
        }
        db += tiltPerOct * std::log2 (f / 1000.0f);
        const float y = mapY (db);
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        const float t = 1.0f - (y - plot.getY()) / plot.getHeight();
        if (buckets != nullptr && y < plot.getBottom() - 0.5f)
            buckets[juce::jlimit (0, 9, (int) (t * 10.0f))].addWithoutMerging ({ x, y, 1.0f, plot.getBottom() - y });
    }
    return path;
}

void SpectrumModule::paintTopRight (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (! isHovering() || ! plot.contains (getPointer())) return;
    const auto text = readout (getPointer().x);
    const auto f = LnF::mono (LnF::Type::footnote);
    const float w = (float) juce::GlyphArrangement::getStringWidth (f, text) + 24.0f;
    auto plate = juce::Rectangle<float> (area.getRight() - w, area.getY(), w, area.getHeight());
    if (plate.getX() < 8.0f) plate.setX (8.0f);
    EidoLookAndFeel::drawPlate (g, plate, 8.0f, plateFill());
    g.setFont (f);
    g.setColour (Colours::ink);
    g.drawText (text, plate, juce::Justification::centred);
}

void SpectrumModule::addMenuItems (juce::PopupMenu& m)
{
    juce::PopupMenu tilt;
    tilt.addItem (1000, "0 dB/oct", true, tiltPerOct == 0.0f);
    tilt.addItem (1001, "3 dB/oct", true, tiltPerOct == 3.0f);
    tilt.addItem (1002, "4.5 dB/oct", true, tiltPerOct == 4.5f);
    m.addSubMenu ("Slope", tilt);
}

void SpectrumModule::handleMenuResult (int id)
{
    if (id == 1000) setTilt (0.0f);
    if (id == 1001) setTilt (3.0f);
    if (id == 1002) setTilt (4.5f);
}
} // namespace eido
