#include "Module.h"
#include "Widgets.h"

namespace eido
{
using LnF = EidoLookAndFeel;

// В сетке плашка с именем модуля стоит снизу слева — приборы её не перекрывают
static float bottomReserve (const Module& m) { return m.isCompact() ? 0.0f : 40.0f; }

// ===========================================================================
// Loudness
namespace
{
// шкала −60…0 дБ: верхние 24 дБ растянуты — там рабочая зона
float levelT (float db)
{
    if (db >= -24.0f) return 0.45f + 0.55f * juce::jlimit (0.0f, 1.0f, (db + 24.0f) / 24.0f);
    return 0.45f * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 36.0f);
}

// столбик в рампе громкости: 10 корзин по 2.4 дБ от −24 до 0, всё тише −24 — первой корзиной
void drawBar (juce::Graphics& g, juce::Rectangle<float> bar, float db, float holdDb)
{
    g.setColour (Colours::outline);
    g.fillRoundedRectangle (bar, 2.0f);
    auto yOf = [&] (float d) { return bar.getBottom() - levelT (d) * bar.getHeight(); };
    if (db > -60.0f)
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (bar, 2.0f);
        g.reduceClipRegion (clip);
        for (int b = 0; b < 10; ++b)
        {
            const float lo = b == 0 ? -60.0f : -24.0f + 2.4f * (float) b, hi = -24.0f + 2.4f * (float) (b + 1);
            if (db <= lo) break;
            const float top = yOf (juce::jmin (db, hi)), bottom = yOf (lo);
            g.setColour (rampBucketColour (Colours::carrierRamp, b));
            g.fillRect (juce::Rectangle<float> (bar.getX(), top, bar.getWidth(), bottom - top + (b == 0 ? 0.0f : 0.5f)));
        }
    }
    if (holdDb > -60.0f)
    {
        g.setColour (holdDb >= -0.1f ? Colours::signal : Colours::ink);
        g.fillRect (juce::Rectangle<float> (bar.getX(), yOf (holdDb) - 1.0f, bar.getWidth(), 2.0f));
    }
}
} // namespace

LoudnessModule::LoudnessModule (const SignalBuffer& s, const LoudnessMeter& m) : Module (ModuleType::Loudness, s), meter (m) {}

void LoudnessModule::reset()
{
    peak = hold = { -100.0f, -100.0f };
    maxPeak = -100.0f;
}

void LoudnessModule::tick()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = lastTick > 0 ? (float) juce::jlimit (0.0, 0.2, now - lastTick) : 1.0f / 60.0f;
    lastTick = now;
    cursor = sig.clampCursor (cursor);
    std::array<float, 2> pk { 0.0f, 0.0f };
    for (auto i = cursor; i < sig.written; ++i)
    {
        pk[0] = juce::jmax (pk[0], std::abs (sig.left (i)));
        pk[1] = juce::jmax (pk[1], std::abs (sig.right (i)));
    }
    cursor = sig.written;
    for (size_t c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (pk[c], -100.0f);
        peak[c] = juce::jmax (db, peak[c] - 20.0f * dt);    // спад 20 дБ/с
        if (db >= hold[c] || now - holdTime[c] > 1.5) { hold[c] = db; holdTime[c] = now; }
        maxPeak = juce::jmax (maxPeak, db);
    }
    if ((++ticks % 30) == 0)
        setDescription ("Integrated " + formatDb (meter.integrated()) + " LUFS, short-term " + formatDb (meter.shortTerm())
                        + " LUFS, momentary " + formatDb (meter.momentary()) + " LUFS, peak " + formatDb (maxPeak) + " dB.");
}

void LoudnessModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto area = r.reduced (isCompact() ? 10.0f : 14.0f).withTrimmedBottom (bottomReserve (*this));
    const auto monoF = LnF::mono (LnF::Type::footnote);

    // цифры справа, если хватает места
    const bool numbers = area.getWidth() >= 190.0f;
    auto nums = numbers ? area.removeFromRight (juce::jmin (150.0f, area.getWidth() * 0.5f)) : juce::Rectangle<float>();

    auto labels = area.removeFromBottom (16.0f);
    auto scale = area.removeFromLeft (30.0f);
    area.removeFromLeft (4.0f);
    auto yOf = [&] (float d) { return area.getBottom() - levelT (d) * area.getHeight(); };

    // шкала
    g.setFont (monoF);
    float lastY = -100.0f;
    for (float d : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f })
    {
        const float y = yOf (d);
        if (std::abs (y - lastY) < 13.0f) continue;
        lastY = y;
        g.setColour (Colours::ink2);
        g.drawText (d == 0.0f ? juce::String ("0") : minus() + juce::String ((int) -d),
                    juce::Rectangle<float> (scale.getX(), y - 7.0f, scale.getWidth() - 4.0f, 14.0f), juce::Justification::centredRight);
        g.setColour (Colours::outline);
        g.drawHorizontalLine (juce::roundToInt (y), scale.getRight() - 2.0f, scale.getRight());
    }

    // столбики: L R | M S
    const float unit = juce::jlimit (6.0f, 18.0f, area.getWidth() / 7.0f);
    const float gap = juce::jmax (3.0f, unit * 0.35f);
    float x = area.getX();
    auto column = [&] (float w, const juce::String& letter, float db, float holdDb)
    {
        auto bar = juce::Rectangle<float> (x, area.getY(), w, area.getHeight());
        drawBar (g, bar, db, holdDb);
        g.setColour (Colours::ink2);
        g.setFont (LnF::font (LnF::Type::caption));
        g.drawText (letter, juce::Rectangle<float> (x - 6.0f, labels.getY() + 2.0f, w + 12.0f, 14.0f), juce::Justification::centred);
        x += w + gap;
    };
    column (unit, "L", peak[0], hold[0]);
    column (unit, "R", peak[1], hold[1]);
    x += gap;
    column (unit * 1.3f, "M", meter.momentary(), -100.0f);
    column (unit * 1.3f, "S", meter.shortTerm(), -100.0f);

    if (! numbers) return;
    // цифры: Integrated крупно, ниже Short-term, Momentary, Peak
    auto n = nums.withTrimmedLeft (12.0f);
    struct Row { const char* name; float v; const char* unit; };
    const Row rows[] = { { "Short-term", meter.shortTerm(), "LUFS" }, { "Momentary", meter.momentary(), "LUFS" },
                         { "Peak", maxPeak, "dB" } };
    {
        auto top = n.removeFromTop (juce::jmin (52.0f, n.getHeight() * 0.4f));
        g.setColour (Colours::ink2);
        g.setFont (LnF::font (LnF::Type::subheadline));
        g.drawText ("Integrated", top.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.setColour (Colours::ink);
        g.setFont (LnF::mono (LnF::Type::title2, true));
        const auto v = formatDb (meter.integrated());
        g.drawText (v, top, juce::Justification::centredLeft);
        const float vw = (float) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), v);
        g.setColour (Colours::ink2);
        g.setFont (LnF::font (LnF::Type::caption));
        g.drawText ("LUFS", top.withTrimmedLeft (vw + 6.0f), juce::Justification::centredLeft);
    }
    const float rowH = juce::jlimit (16.0f, 34.0f, n.getHeight() / 3.0f);
    for (auto& row : rows)
    {
        if (n.getHeight() < 16.0f) break;
        auto rr = n.removeFromTop (rowH);
        g.setColour (Colours::ink2);
        g.setFont (LnF::font (LnF::Type::subheadline));
        g.drawText (row.name, rr.removeFromLeft (rr.getWidth() * 0.5f), juce::Justification::centredLeft);
        const bool over = juce::String (row.name) == "Peak" && row.v >= -0.1f;
        g.setColour (over ? Colours::signal : Colours::ink);
        g.setFont (monoF);
        g.drawText (formatDb (row.v), rr, juce::Justification::centredRight);
    }
}

// ===========================================================================
// VU
VuModule::VuModule (const SignalBuffer& s) : Module (ModuleType::VU, s) {}

void VuModule::tick()
{
    cursor = sig.clampCursor (cursor);
    const double a = 1.0 - std::exp (-1.0 / (0.065 * sig.sampleRate));   // 99% за ~300 мс
    for (auto i = cursor; i < sig.written; ++i)
    {
        const double l = sig.left (i), r = sig.right (i);
        ms += a * (0.5 * (l * l + r * r) - ms);
    }
    cursor = sig.written;
    const float vu = (float) (10.0 * std::log10 (juce::jmax (1.0e-10, ms)) + 18.0);   // 0 VU = −18 dBFS RMS
    needle += (juce::jlimit (-40.0f, 6.0f, vu) - needle) * 0.5f;
    setDescription ("VU " + formatDb (needle, 1, -30.0f));
}

void VuModule::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto area = r.reduced (12.0f, isCompact() ? 8.0f : 14.0f).withTrimmedBottom (bottomReserve (*this));
    const float span = juce::degreesToRadians (50.0f);
    const float R = juce::jmax (20.0f, juce::jmin (area.getWidth() * 0.5f / std::sin (span) * 0.86f, area.getHeight() - 26.0f));
    const juce::Point<float> pivot (area.getCentreX(), area.getY() + 26.0f + R);
    auto angleOf = [&] (float vu)   // настоящая шкала VU — линейна по напряжению
    {
        const float p = juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain (vu) / juce::Decibels::decibelsToGain (3.0f));
        return -span + 2.0f * span * p;
    };
    auto at = [&] (float ang, float rad) { return pivot + juce::Point<float> (std::sin (ang), -std::cos (ang)) * rad; };

    // дуга: рабочая часть Ink, зона выше 0 — сигнальная полоса
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleOf (-20.0f), angleOf (0.0f), true);
    g.setColour (Colours::ink);
    g.strokePath (arc, juce::PathStrokeType (1.5f));
    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, R + 2.0f, R + 2.0f, 0.0f, angleOf (0.0f), angleOf (3.0f), true);
    g.setColour (Colours::signal);
    g.strokePath (red, juce::PathStrokeType (5.0f));

    // деления и подписи
    const bool full = R >= 70.0f;
    const auto marks = full ? std::vector<float> { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 } : std::vector<float> { -20, -10, -5, 0, 3 };
    g.setFont (LnF::mono (full ? LnF::Type::footnote : 9.0f));
    for (float m : marks)
    {
        const float ang = angleOf (m);
        g.setColour (m > 0 ? Colours::signal : Colours::ink);
        g.drawLine ({ at (ang, R), at (ang, R + 7.0f) }, 1.5f);
        const auto c = at (ang, R + 17.0f);
        g.drawText (juce::String ((int) std::abs (m)), juce::Rectangle<float> (c.x - 12.0f, c.y - 7.0f, 24.0f, 14.0f), juce::Justification::centred);
    }
    // проценты (0 VU = 100%)
    if (R >= 90.0f)
    {
        g.setFont (LnF::mono (9.0f));
        for (int pct = 0; pct <= 100; pct += 20)
        {
            const float p = (float) pct / 100.0f / juce::Decibels::decibelsToGain (3.0f);
            const float ang = -span + 2.0f * span * p;
            g.setColour (Colours::ink2);
            g.drawLine ({ at (ang, R - 4.0f), at (ang, R - 9.0f) }, 1.0f);
            const auto c = at (ang, R - 20.0f);
            g.drawText (juce::String (pct), juce::Rectangle<float> (c.x - 14.0f, c.y - 6.0f, 28.0f, 12.0f), juce::Justification::centred);
        }
    }
    g.setColour (Colours::ink2);
    g.setFont (LnF::font (LnF::Type::subheadline, true));
    g.drawText ("VU", juce::Rectangle<float> (pivot.x - 30.0f, pivot.y - R * 0.45f - 8.0f, 60.0f, 16.0f), juce::Justification::centred);

    // стрелка — сигнальным
    const float ang = angleOf (needle);
    g.setColour (Colours::signal);
    g.drawLine ({ at (ang, R * 0.2f), at (ang, R + 10.0f) }, 2.0f);
}

// ===========================================================================
// Stereometer
namespace
{
struct Disc { juce::Point<float> c; float R; juce::Rectangle<float> bar; };
Disc discOf (const Module& m)
{
    auto area = m.getLocalBounds().toFloat().reduced (12.0f, m.isCompact() ? 8.0f : 14.0f).withTrimmedBottom (bottomReserve (m));
    auto bar = area.removeFromBottom (22.0f);
    area.removeFromBottom (6.0f);
    const float R = juce::jmax (10.0f, juce::jmin (area.getWidth() * 0.5f - 14.0f, area.getHeight() - 14.0f));
    return { { area.getCentreX(), area.getBottom() }, R, bar };
}
} // namespace

StereoModule::StereoModule (const SignalBuffer& s) : Module (ModuleType::Stereo, s) {}

void StereoModule::reset()
{
    trail.clear();
    corr = 0.0f;
    hasCorr = false;
}

void StereoModule::tick()
{
    cursor = sig.clampCursor (cursor, 8192);
    const auto n = sig.written - cursor;
    const int step = (int) juce::jmax ((juce::int64) 1, n / 1200);
    std::vector<Dot> frame;
    frame.reserve (1300);
    double lr = 0, ll = 0, rr = 0;
    for (auto i = cursor; i < sig.written; ++i)
    {
        const float l = sig.left (i), r = sig.right (i);
        lr += (double) l * r; ll += (double) l * l; rr += (double) r * r;
        if (((i - cursor) % step) != 0) continue;
        float x = (r - l) * 0.70710678f, y = (l + r) * 0.70710678f;
        if (y < 0.0f) { x = -x; y = -y; }   // полярный режим: нижняя половина зеркалится вверх
        const float mag = std::sqrt (x * x + y * y);
        if (mag < 1.0e-5f) continue;
        const float db = juce::Decibels::gainToDecibels (mag * 0.70710678f, -100.0f);
        const float rad = juce::jlimit (0.0f, 1.0f, (db + 40.0f) / 40.0f);   // радиус: −40…0 дБ
        frame.push_back ({ rad * x / mag, rad * y / mag, (juce::uint8) juce::jlimit (0, 9, (int) (rad * 10.0f)) });
    }
    cursor = sig.written;
    if (ll > 1.0e-9 && rr > 1.0e-9)
    {
        const float c = (float) (lr / std::sqrt (ll * rr));
        corr = hasCorr ? corr + (c - corr) * 0.25f : c;
        hasCorr = true;
    }

    trail.push_front (std::move (frame));
    while ((int) trail.size() > kTrailFrames) trail.pop_back();
    setDescription ("Correlation " + (corr < 0 ? minus() : juce::String ("+")) + juce::String (std::abs (corr), 2));
}

void StereoModule::paintContent (juce::Graphics& g, juce::Rectangle<float>)
{
    const auto d = discOf (*this);
    const float pi = juce::MathConstants<float>::pi;

    // сетка: полуокружности (0 и −20 дБ) и лучи через 45°
    g.setColour (Colours::outline);
    for (float k : { 1.0f, 0.5f })
    {
        juce::Path p;
        p.addCentredArc (d.c.x, d.c.y, d.R * k, d.R * k, 0.0f, -pi * 0.5f, pi * 0.5f, true);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    for (int i = 0; i <= 4; ++i)
    {
        const float a = -pi * 0.5f + pi * 0.25f * (float) i;
        g.drawLine ({ d.c, d.c + juce::Point<float> (std::sin (a), -std::cos (a)) * d.R }, 1.0f);
    }
    // след: свежие точки плотные, старые бледнеют; цвет — рампа по радиусу (громкости)
    for (int age = (int) trail.size() - 1; age >= 0; --age)
    {
        juce::RectangleList<int> buckets[10];
        for (auto& dot : trail[(size_t) age])
            buckets[dot.bucket].addWithoutMerging ({ juce::roundToInt (d.c.x + d.R * dot.x) - 1, juce::roundToInt (d.c.y - d.R * dot.y) - 1, 2, 2 });
        const float alpha = 1.0f - (float) age / (float) kTrailFrames;
        for (int b = 0; b < 10; ++b)
            if (! buckets[b].isEmpty())
            {
                g.setColour (rampColour (Colours::carrierRamp, 0.2f + 0.08f * (float) b).withMultipliedAlpha (alpha));
                g.fillRectList (buckets[b]);
            }
    }

    g.setFont (LnF::font (LnF::Type::caption, true));
    g.setColour (Colours::ink2);
    for (auto [label, a] : { std::pair<const char*, float> { "L", -pi * 0.25f }, { "M", 0.0f }, { "R", pi * 0.25f } })
    {
        const auto p = d.c + juce::Point<float> (std::sin (a), -std::cos (a)) * (d.R + 8.0f);
        g.drawText (label, juce::Rectangle<float> (p.x - 8.0f, p.y - 7.0f, 16.0f, 14.0f), juce::Justification::centred);
    }

    // корреляция −1…+1
    auto bar = d.bar;
    g.setFont (LnF::mono (LnF::Type::footnote));
    const auto value = (corr < 0 ? minus() : juce::String ("+")) + juce::String (std::abs (corr), 2);
    auto valueArea = bar.removeFromRight (40.0f);
    g.setColour (hasCorr ? Colours::ink : Colours::ink2);
    g.drawText (value, valueArea, juce::Justification::centredRight);
    g.setColour (Colours::ink2);
    g.drawText (minus() + "1", bar.removeFromLeft (18.0f), juce::Justification::centredLeft);
    g.drawText ("+1", bar.removeFromRight (22.0f), juce::Justification::centredRight);
    auto track = bar.reduced (4.0f, 0.0f).withSizeKeepingCentre (bar.getWidth() - 8.0f, 4.0f);
    g.setColour (Colours::track);
    g.fillRoundedRectangle (track, 2.0f);
    g.fillRect (juce::Rectangle<float> (1.0f, 10.0f).withCentre (track.getCentre()));
    const float mx = track.getX() + track.getWidth() * (corr + 1.0f) * 0.5f;
    g.setColour (corr < 0.0f ? Colours::signal : Colours::live);
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, 14.0f).withCentre ({ mx, track.getCentreY() }), 2.0f);
}
} // namespace eido
