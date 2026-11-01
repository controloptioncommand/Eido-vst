#include "Module.h"
#include "Widgets.h"

namespace eido
{
using LnF = EidoLookAndFeel;

// Высота поля волны: как в Mist (262 px вида, поля по 46), в полосе — меньше
static juce::Rectangle<float> waveArea (juce::Rectangle<float> r)
{
    const float m = juce::jlimit (6.0f, 46.0f, r.getHeight() * 0.17f);
    return r.reduced (1.0f, m);
}

// ===========================================================================
// Scope
ScopeModule::ScopeModule (const SignalBuffer& s) : Module (ModuleType::Scope, s) {}

void ScopeModule::tick()
{
    const int samplesPerCol = 96;
    cursor = sig.clampCursor (cursor);
    const int maxCols = juce::jmax (1, getWidth() - 2);
    while (cursor + samplesPerCol <= sig.written)
    {
        Col c;
        float pk = 0.0f;
        for (int i = 0; i < samplesPerCol; ++i)
        {
            const float v = sig.mono (cursor + i);
            c.mn = juce::jmin (c.mn, v);
            c.mx = juce::jmax (c.mx, v);
            pk = juce::jmax (pk, std::abs (v));
        }
        c.db = juce::Decibels::gainToDecibels (pk, -100.0f);
        cursor += samplesPerCol;
        cols.push_back (c);
    }
    while ((int) cols.size() > maxCols) cols.pop_front();
}

void ScopeModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto area = waveArea (r);
    const float mid = area.getCentreY(), hh = area.getHeight() * 0.5f;

    // пунктирные направляющие −6 и −12 дБ (обе полярности) с подписями
    const float dashes[] = { 3.0f, 4.0f };
    const bool labels = r.getHeight() >= 110.0f;
    g.setFont (LnF::mono (LnF::Type::footnote));
    for (float db : { -6.0f, -12.0f })
    {
        const float a = juce::Decibels::decibelsToGain (db) * hh;
        for (float y : { mid - a, mid + a })
        {
            g.setColour (Colours::ink2.withAlpha (0.45f));
            g.drawDashedLine ({ area.getX() + 12.0f, y, area.getRight() - (labels ? 48.0f : 12.0f), y }, dashes, 2, 1.0f);
        }
        if (labels)
        {
            g.setColour (Colours::ink2);
            g.drawText (minus() + juce::String ((int) -db),
                        juce::Rectangle<float> (area.getRight() - 44.0f, mid - a - 7.0f, 30.0f, 14.0f), juce::Justification::centredRight);
        }
    }

    // полоски группируются в 10 цветовых корзин
    juce::RectangleList<float> buckets[10];
    float x = area.getRight() - (float) cols.size();
    for (auto& c : cols)
    {
        if (x >= area.getX())
        {
            const float y0 = mid - juce::jlimit (-1.0f, 1.0f, c.mx) * hh;
            const float y1 = mid - juce::jlimit (-1.0f, 1.0f, c.mn) * hh;
            buckets[rampBucket (c.db)].addWithoutMerging ({ x, y0, 1.0f, juce::jmax (1.0f, y1 - y0) });
        }
        x += 1.0f;
    }
    for (int b = 0; b < 10; ++b)
    {
        if (buckets[b].isEmpty()) continue;
        g.setColour (rampBucketColour (Colours::carrierRamp, b));
        g.fillRectList (buckets[b]);
    }
}

// ===========================================================================
// Waves — ridgelines
WavesModule::WavesModule (const SignalBuffer& s) : Module (ModuleType::Waves, s) { reset(); }

void WavesModule::reset()
{
    lines.clear();
    for (int i = 0; i < kMaxLines; ++i) lines.push_back (std::vector<float> (kPoints, 0.0f));
}

void WavesModule::tick()
{
    if ((++ticks % 4) != 0) return;   // 15 линий в секунду
    const int span = 2048, search = 1024;
    juce::int64 start = sig.written - span - search;
    if (start < 0) return;
    for (int i = 0; i < search; ++i)
        if (sig.mono (start + i) <= 0.0f && sig.mono (start + i + 1) > 0.0f) { start += i; break; }

    std::vector<float> line ((size_t) kPoints);
    float peak = 0.0f;
    for (int p = 0; p < kPoints; ++p)
    {
        line[(size_t) p] = sig.mono (start + (juce::int64) p * span / kPoints);
        peak = juce::jmax (peak, std::abs (line[(size_t) p]));
    }
    const float target = peak > 0.01f ? juce::jlimit (1.0f, 6.0f, 0.8f / peak) : gain;
    gain += (target - gain) * 0.15f;

    // «Уменьшить движение»: линии не уезжают вверх, обновляется только передняя
    if (Accessibility::reduceMotion()) lines.front() = std::move (line);
    else
    {
        lines.push_front (std::move (line));
        while ((int) lines.size() > kMaxLines) lines.pop_back();
    }
}

void WavesModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto area = r.reduced (1.0f).withTrimmedTop (juce::jmin (36.0f, r.getHeight() * 0.12f));
    const int n = juce::jlimit (6, kMaxLines, (int) (area.getHeight() / 9.0f));
    const float spacing = area.getHeight() / (float) (n + 3);
    const float amp = spacing * 2.4f;
    const auto bg = background();

    for (int k = juce::jmin (n, (int) lines.size()) - 1; k >= 0; --k)
    {
        const auto& line = lines[(size_t) k];
        const float depth = (float) k / (float) juce::jmax (1, n - 1);
        const float baseY = area.getBottom() - spacing * 2.0f - (float) k * spacing;
        const float x0 = area.getX(), x1 = area.getRight();
        const float a = amp * (1.0f - 0.35f * depth);

        juce::Path p;
        for (int i = 0; i < kPoints; ++i)
        {
            const float t = (float) i / (float) (kPoints - 1);
            const float edge = std::sin (t * juce::MathConstants<float>::pi);
            const float v = std::tanh (line[(size_t) i] * gain);
            const float x = x0 + (x1 - x0) * t, y = baseY - v * a * (0.25f + 0.75f * edge);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::Path fill (p);
        fill.lineTo (x1, r.getBottom());
        fill.lineTo (x0, r.getBottom());
        fill.closeSubPath();
        g.setColour (bg);
        g.fillPath (fill);

        if (k == 0) { g.setColour (Colours::signal); g.strokePath (p, juce::PathStrokeType (1.8f)); }
        else        { g.setColour (Colours::ink.withAlpha (0.7f - 0.45f * depth)); g.strokePath (p, juce::PathStrokeType (1.0f)); }
    }
}

// ===========================================================================
// Spectrogram
// Картинка сдвигается влево; на каждом тике дорисовываются только новые столбцы, цвета — из таблицы на 256 значений.
SpectrogramModule::SpectrogramModule (const SignalBuffer& s) : Module (ModuleType::Spectrogram, s) {}

void SpectrogramModule::reset()
{
    history.clear();
    if (image.isValid()) image.clear (image.getBounds());
}

void SpectrogramModule::prepare (int width, int height)
{
    const int rowsNeeded = juce::jmax (8, height);
    width = juce::jmax (8, width);
    if (rowsNeeded != rows || sig.sampleRate != rate)
    {
        rows = rowsNeeded;
        rate = sig.sampleRate;
        bins.resize ((size_t) rows);
        tilt.resize ((size_t) rows);
        const float nyq = (float) rate * 0.5f;
        for (int y = 0; y < rows; ++y)
        {
            const float t0 = 1.0f - (float) (y + 1) / (float) rows, t1 = 1.0f - (float) y / (float) rows;
            const float f0 = 20.0f * std::pow (1000.0f, t0), f1 = 20.0f * std::pow (1000.0f, t1);
            const int b0 = juce::jlimit (1, kSize / 2 - 1, (int) (f0 / nyq * (kSize / 2)));
            const int b1 = juce::jlimit (b0, kSize / 2 - 1, (int) (f1 / nyq * (kSize / 2)));
            bins[(size_t) y] = { b0, b1 };
            tilt[(size_t) y] = 3.0f * std::log2 (juce::jmax (20.0f, (f0 + f1) * 0.5f) / 1000.0f);
        }
        history.clear();
        image = {};
    }
    if (! image.isValid() || image.getWidth() != width || image.getHeight() != rows)
    {
        image = juce::Image (juce::Image::ARGB, width, rows, true);
        lutKey = {};   // перерисовать из истории
    }
}

void SpectrogramModule::updateLut()
{
    const auto bg = background();
    const auto key = bg.toString() + Colours::carrierRamp[0].toString() + Colours::carrierRamp[1].toString() + Colours::carrierRamp[2].toString();
    if (key == lutKey) return;
    lutKey = key;
    for (int i = 0; i < 256; ++i)
    {
        const float v = (float) i / 255.0f;
        const auto c = v < 0.05f ? juce::Colours::transparentBlack
                                 : bg.interpolatedWith (rampColour (Colours::carrierRamp, v), juce::jmin (1.0f, v * 1.8f));
        lut[(size_t) i] = c.getPixelARGB();
    }
    image.clear (image.getBounds());
    const int n = (int) history.size();
    for (int k = 0; k < n; ++k)
        drawColumn (image.getWidth() - (n - k) * kColW, history[(size_t) k]);
}

void SpectrogramModule::drawColumn (int x, const std::vector<juce::uint8>& col)
{
    if (x + kColW <= 0 || x >= image.getWidth()) return;
    juce::Image::BitmapData bd (image, juce::Image::BitmapData::writeOnly);
    const int n = juce::jmin ((int) col.size(), image.getHeight());
    for (int y = 0; y < n; ++y)
    {
        const auto px = lut[col[(size_t) y]];
        for (int dx = 0; dx < kColW; ++dx)
            if (x + dx >= 0 && x + dx < image.getWidth())
                bd.setPixelColour (x + dx, y, juce::Colour (px));
    }
}

void SpectrogramModule::tick()
{
    prepare (getWidth() - 2, getHeight() - 16);
    updateLut();
    cursor = sig.clampCursor (cursor);
    int columns = 0;
    while (cursor + kHop <= sig.written && columns < 16)
    {
        cursor += kHop;
        const juce::int64 start = cursor - kSize;
        if (start < 0) continue;
        for (int i = 0; i < kSize; ++i) buf[(size_t) i] = sig.mono (start + i);
        std::fill (buf.begin() + kSize, buf.end(), 0.0f);
        window.multiplyWithWindowingTable (buf.data(), (size_t) kSize);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);

        std::vector<juce::uint8> col ((size_t) rows);
        const float norm = 1.0f / (float) (kSize / 4);
        for (int y = 0; y < rows; ++y)
        {
            const auto [b0, b1] = bins[(size_t) y];
            float mag = 0.0f;
            for (int b = b0; b <= b1; ++b) mag = juce::jmax (mag, buf[(size_t) b]);
            const float db = juce::Decibels::gainToDecibels (mag * norm, -120.0f);
            col[(size_t) y] = (juce::uint8) juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + tilt[(size_t) y] + 90.0f) / 84.0f) * 255.0f);
        }
        const int w = image.getWidth();
        image.moveImageSection (0, 0, kColW, 0, w - kColW, image.getHeight());
        drawColumn (w - kColW, col);
        history.push_back (std::move (col));
        ++columns;
    }
    while ((int) history.size() > image.getWidth() / kColW + 1) history.pop_front();
}

void SpectrogramModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    prepare (getWidth() - 2, getHeight() - 16);
    updateLut();
    auto area = r.reduced (1.0f, 8.0f);
    g.drawImage (image, area, juce::RectanglePlacement::stretchToFit);
    if (area.getHeight() < 60.0f) return;
    g.setFont (LnF::mono (LnF::Type::footnote));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float t = std::log (f / 20.0f) / std::log (1000.0f);
        const float y = area.getBottom() - t * area.getHeight();
        g.setColour (Colours::ink2);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (area.getX() + 6.0f, y - 7.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    }
}
} // namespace eido
