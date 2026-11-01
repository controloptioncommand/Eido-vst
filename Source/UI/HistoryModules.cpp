#include "Module.h"
#include "Widgets.h"

namespace eido
{
using LnF = EidoLookAndFeel;

// ===========================================================================
// Loudness History
namespace
{
constexpr float kHistTop = 0.0f, kHistBottom = -48.0f;
}

LoudnessHistoryModule::LoudnessHistoryModule (const SignalBuffer& s, const LoudnessMeter& m)
    : Module (ModuleType::LoudnessHistory, s), meter (m) {}

void LoudnessHistoryModule::tick()
{
    // точка каждые 100 мс сигнала — как обновляется Short-term
    const auto hop = (juce::int64) (sig.sampleRate * 0.1);
    cursor = sig.clampCursor (cursor, (int) hop * 4);
    if (sig.written - cursor < hop) return;
    cursor += hop;
    points.push_back ({ meter.shortTerm(), meter.momentary() });
    while ((int) points.size() > kPoints) points.pop_front();
    setDescription ("Loudness history. Short-term " + formatDb (meter.shortTerm()) + " LUFS, integrated "
                    + formatDb (meter.integrated()) + " LUFS, loudness range " + juce::String (meter.loudnessRange(), 1) + " LU.");
}

void LoudnessHistoryModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto area = r.reduced (12.0f, isCompact() ? 8.0f : 12.0f).withTrimmedBottom (isCompact() ? 0.0f : 40.0f);   // в сетке снизу — чип
    auto header = area.removeFromTop (16.0f);
    area.removeFromTop (6.0f);
    auto scale = area.removeFromLeft (30.0f);
    area.removeFromLeft (4.0f);
    const auto yOf = [&] (float l) { return area.getY() + area.getHeight() * juce::jlimit (0.0f, 1.0f, (kHistTop - l) / (kHistTop - kHistBottom)); };

    // сетка и шкала LUFS
    g.setFont (LnF::mono (LnF::Type::footnote));
    for (float l = 0.0f; l >= kHistBottom; l -= 12.0f)
    {
        const float y = yOf (l);
        g.setColour (Colours::outline);
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
        if (area.getHeight() >= 60.0f || l == 0.0f || l == -24.0f)
        {
            g.setColour (Colours::ink2);
            g.drawText (l == 0.0f ? juce::String ("0") : minus() + juce::String ((int) -l),
                        juce::Rectangle<float> (scale.getX(), y - 7.0f, scale.getWidth() - 4.0f, 14.0f), juce::Justification::centredRight);
        }
    }

    // Short-term — линия Carrier и заливка рампой; Momentary — тонкая Ink 2; новые точки справа
    const float step = area.getWidth() / (float) (kPoints - 1);
    const int n = (int) points.size();
    juce::Path sPath, mPath;
    juce::RectangleList<float> buckets[10];
    bool sOpen = false, mOpen = false;
    for (int k = 0; k < n; ++k)
    {
        const auto& p = points[(size_t) k];
        const float x = area.getRight() - (float) (n - 1 - k) * step;
        if (p.s > kHistBottom)
        {
            const float y = yOf (p.s);
            if (! sOpen) { sPath.startNewSubPath (x, y); sOpen = true; } else sPath.lineTo (x, y);
            buckets[rampBucket (p.s)].addWithoutMerging ({ x - step * 0.5f, y, step + 0.5f, area.getBottom() - y });
        }
        else sOpen = false;
        if (p.m > kHistBottom)
        {
            const float y = yOf (p.m);
            if (! mOpen) { mPath.startNewSubPath (x, y); mOpen = true; } else mPath.lineTo (x, y);
        }
        else mOpen = false;
    }
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (area.toNearestInt());
        const auto bg = background();
        for (int b = 0; b < 10; ++b)
            if (! buckets[b].isEmpty())
            {
                g.setColour (bg.interpolatedWith (rampBucketColour (Colours::carrierRamp, b), 0.32f));
                g.fillRectList (buckets[b]);
            }
        g.setColour (Colours::ink2.withAlpha (0.6f));
        g.strokePath (mPath, juce::PathStrokeType (1.0f));
        g.setColour (Colours::carrier);
        g.strokePath (sPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Integrated — пунктир Signal
        const float il = meter.integrated();
        if (il > kHistBottom)
        {
            const float dashes[] = { 4.0f, 4.0f };
            const float y = yOf (il);
            g.setColour (Colours::signal);
            g.drawDashedLine ({ area.getX(), y, area.getRight(), y }, dashes, 2, 1.5f);
        }
    }

    // цифры сверху
    g.setFont (LnF::mono (LnF::Type::footnote));
    juce::String txt;
    txt << "S " << formatDb (meter.shortTerm()) << midDot() << "I " << formatDb (meter.integrated()) << " LUFS";
    if (header.getWidth() >= 240.0f) txt << midDot() << "LRA " << juce::String (meter.loudnessRange(), 1) << " LU";
    g.setColour (Colours::ink);
    g.drawText (txt, header, juce::Justification::centredLeft);
    if (header.getWidth() >= 330.0f)
    {
        g.setColour (Colours::ink2);
        g.drawText ("30 s", header, juce::Justification::centredRight);
    }
}

// ===========================================================================
// Peak History
PeakHistoryModule::PeakHistoryModule (const SignalBuffer& s) : Module (ModuleType::PeakHistory, s) {}

void PeakHistoryModule::tick()
{
    const int perCol = juce::jmax (64, (int) (sig.sampleRate * 0.02));   // колонка — 20 мс
    cursor = sig.clampCursor (cursor);
    while (cursor + perCol <= sig.written)
    {
        float pl = 0.0f, pr = 0.0f;
        double sl = 0.0, sr = 0.0;
        for (int i = 0; i < perCol; ++i)
        {
            const float l = sig.left (cursor + i), r = sig.right (cursor + i);
            pl = juce::jmax (pl, std::abs (l));
            pr = juce::jmax (pr, std::abs (r));
            sl += (double) l * l;
            sr += (double) r * r;
        }
        cursor += perCol;
        auto db = [] (double v) { return juce::Decibels::gainToDecibels ((float) v, -100.0f); };
        cols.push_back ({ db (pl), db (pr), db (std::sqrt (sl / perCol)), db (std::sqrt (sr / perCol)) });
    }
    while ((int) cols.size() > juce::jmax (1, getWidth())) cols.pop_front();
}

void PeakHistoryModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float m = juce::jlimit (6.0f, 46.0f, r.getHeight() * 0.17f);
    const auto area = r.reduced (1.0f, m);
    const float mid = area.getCentreY(), hh = area.getHeight() * 0.5f;
    const auto hOf = [hh] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f) * hh; };   // шкала −48…0 дБ

    // L сверху, R снизу: пик — светлее, RMS — полным цветом рампы
    juce::RectangleList<float> peaks[10], rms[10];
    float x = area.getRight() - (float) cols.size();
    for (auto& c : cols)
    {
        if (x >= area.getX())
        {
            const float hl = hOf (c.peakL), hr = hOf (c.peakR), rl = hOf (c.rmsL), rr = hOf (c.rmsR);
            peaks[rampBucket (c.peakL)].addWithoutMerging ({ x, mid - hl, 1.0f, hl });
            peaks[rampBucket (c.peakR)].addWithoutMerging ({ x, mid, 1.0f, hr });
            rms[rampBucket (c.rmsL)].addWithoutMerging ({ x, mid - rl, 1.0f, rl });
            rms[rampBucket (c.rmsR)].addWithoutMerging ({ x, mid, 1.0f, rr });
        }
        x += 1.0f;
    }
    const auto bg = background();
    for (int b = 0; b < 10; ++b)
    {
        if (! peaks[b].isEmpty()) { g.setColour (bg.interpolatedWith (rampBucketColour (Colours::carrierRamp, b), 0.45f)); g.fillRectList (peaks[b]); }
        if (! rms[b].isEmpty())   { g.setColour (rampBucketColour (Colours::carrierRamp, b)); g.fillRectList (rms[b]); }
    }

    // центр и пунктир −6 / −12 / −24 дБ с подписями
    g.setColour (Colours::outline);
    g.drawHorizontalLine (juce::roundToInt (mid), area.getX(), area.getRight());
    const float dashes[] = { 3.0f, 4.0f };
    const bool labels = r.getHeight() >= 110.0f;
    g.setFont (LnF::mono (LnF::Type::footnote));
    for (float db : { -6.0f, -12.0f, -24.0f })
    {
        const float h = hOf (db);
        for (float y : { mid - h, mid + h })
        {
            g.setColour (Colours::ink2.withAlpha (0.45f));
            g.drawDashedLine ({ area.getX() + 12.0f, y, area.getRight() - (labels ? 48.0f : 12.0f), y }, dashes, 2, 1.0f);
        }
        if (labels)
        {
            g.setColour (Colours::ink2);
            g.drawText (minus() + juce::String ((int) -db), juce::Rectangle<float> (area.getRight() - 44.0f, mid - h - 7.0f, 30.0f, 14.0f),
                        juce::Justification::centredRight);
        }
    }
    if (r.getHeight() >= 90.0f)
    {
        g.setFont (LnF::font (LnF::Type::caption, true));
        g.setColour (Colours::ink2);
        g.drawText ("L", juce::Rectangle<float> (area.getX() + 12.0f, area.getY(), 20.0f, 14.0f), juce::Justification::centredLeft);
        g.drawText ("R", juce::Rectangle<float> (area.getX() + 12.0f, area.getBottom() - 14.0f, 20.0f, 14.0f), juce::Justification::centredLeft);
    }
}
} // namespace eido
