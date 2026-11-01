#include "Loudness.h"
#include <cmath>

namespace eido
{
void LoudnessMeter::prepare (double sr)
{
    if (sr <= 0) sr = 48000.0;
    const double pi = juce::MathConstants<double>::pi;

    // ступень 1: полка +4 дБ (коэффициенты BS.1770, пересчитанные под частоту дискретизации)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (pi * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        for (auto& b : shelf)
        {
            b.b0 = (Vh + Vb * K / Q + K * K) / a0;
            b.b1 = 2.0 * (K * K - Vh) / a0;
            b.b2 = (Vh - Vb * K / Q + K * K) / a0;
            b.a1 = 2.0 * (K * K - 1.0) / a0;
            b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    // ступень 2: срез низа (RLB)
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (pi * f0 / sr);
        const double a0 = 1.0 + K / Q + K * K;
        for (auto& b : highpass)
        {
            b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0;
            b.a1 = 2.0 * (K * K - 1.0) / a0;
            b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    subLen = juce::jmax (1, (int) std::round (sr * 0.1));
    clear();
}

void LoudnessMeter::clear()
{
    for (auto& b : shelf) b.clear();
    for (auto& b : highpass) b.clear();
    subPos = 0;
    subSum = 0.0;
    subs.fill (0.0);
    subCount = subWrite = 0;
    histCount.fill (0);
    histEnergy.fill (0.0);
    stCount.fill (0);
    stEnergy.fill (0.0);
    mLufs = sLufs = iLufs = mMax = kSilent;
    lraLu = 0.0f;
}

void LoudnessMeter::process (const float* l, const float* r, int n)
{
    if (resetRequested.exchange (false)) clear();
    for (int i = 0; i < n; ++i)
    {
        const double zl = highpass[0].run (shelf[0].run ((double) l[i]));
        double e = zl * zl;
        if (r != nullptr)
        {
            const double zr = highpass[1].run (shelf[1].run ((double) r[i]));
            e += zr * zr;
        }
        subSum += e;
        if (++subPos >= subLen) finishSubBlock();
    }
}

void LoudnessMeter::finishSubBlock()
{
    subs[(size_t) subWrite] = subSum / (double) subLen;
    subWrite = (subWrite + 1) % (int) subs.size();
    subCount = juce::jmin (subCount + 1, (int) subs.size());
    subSum = 0.0;
    subPos = 0;

    auto meanOfLast = [this] (int k)
    {
        double s = 0.0;
        for (int j = 1; j <= k; ++j) s += subs[(size_t) ((subWrite - j + (int) subs.size()) % (int) subs.size())];
        return s / (double) k;
    };

    if (subCount >= 4)
    {
        const double m = meanOfLast (4);
        const float lm = toLufs (m);
        mLufs = lm;
        if (lm > mMax.load()) mMax = lm;

        // 400-мс блок с шагом 100 мс (перекрытие 75%) — в гистограмму, если громче −70 LUFS
        if (lm > -70.0f)
        {
            const int bin = juce::jlimit (0, kBins - 1, (int) ((lm + 70.0f) * 10.0f));
            ++histCount[(size_t) bin];
            histEnergy[(size_t) bin] += m;
        }

        // Integrated: среднее по блокам выше −70, затем относительный порог −10 LU
        double sum = 0.0;
        juce::uint64 cnt = 0;
        for (int b = 0; b < kBins; ++b) { sum += histEnergy[(size_t) b]; cnt += histCount[(size_t) b]; }
        if (cnt > 0)
        {
            const float gate = toLufs (sum / (double) cnt) - 10.0f;
            const int gb = juce::jlimit (0, kBins - 1, (int) std::ceil ((gate + 70.0f) * 10.0f));
            double s2 = 0.0;
            juce::uint64 c2 = 0;
            for (int b = gb; b < kBins; ++b) { s2 += histEnergy[(size_t) b]; c2 += histCount[(size_t) b]; }
            iLufs = c2 > 0 ? toLufs (s2 / (double) c2) : kSilent;
        }
    }
    if (subCount < 30) { sLufs = kSilent; return; }
    const double se = meanOfLast (30);
    const float ls = toLufs (se);
    sLufs = ls;

    // LRA: Short-term каждые 100 мс, порог −70 LUFS, относительный −20 LU, затем L95 − L10
    if (ls > -70.0f)
    {
        const int bin = juce::jlimit (0, kBins - 1, (int) ((ls + 70.0f) * 10.0f));
        ++stCount[(size_t) bin];
        stEnergy[(size_t) bin] += se;
    }
    double sum = 0.0;
    juce::uint64 cnt = 0;
    for (int b = 0; b < kBins; ++b) { sum += stEnergy[(size_t) b]; cnt += stCount[(size_t) b]; }
    if (cnt < 2) return;
    const float gate = toLufs (sum / (double) cnt) - 20.0f;
    const int gb = juce::jlimit (0, kBins - 1, (int) std::ceil ((gate + 70.0f) * 10.0f));
    juce::uint64 total = 0;
    for (int b = gb; b < kBins; ++b) total += stCount[(size_t) b];
    if (total < 2) return;
    auto percentile = [&] (double p)
    {
        const auto target = (juce::uint64) std::ceil (p * (double) total);
        juce::uint64 acc = 0;
        for (int b = gb; b < kBins; ++b)
        {
            acc += stCount[(size_t) b];
            if (acc >= juce::jmax ((juce::uint64) 1, target)) return -70.0f + ((float) b + 0.5f) * 0.1f;
        }
        return -70.0f + ((float) kBins - 0.5f) * 0.1f;
    };
    lraLu = juce::jmax (0.0f, percentile (0.95) - percentile (0.10));
}
} // namespace eido
