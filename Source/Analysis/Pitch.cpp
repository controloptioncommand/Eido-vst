#include "Pitch.h"

namespace eido
{
PitchDetector::Result PitchDetector::analyse (const SignalBuffer& sig)
{
    const juce::int64 start = sig.written - kN;
    if (start < 0) return {};
    for (int i = 0; i < kN; ++i) frame[(size_t) i] = sig.mono (start + i);
    return analyse (frame.data(), sig.sampleRate);
}

PitchDetector::Result PitchDetector::analyse (const float* x, double sr)
{
    Result res;
    if (sr <= 0) return res;

    prefix[0] = 0.0;
    for (int i = 0; i < kN; ++i) prefix[(size_t) i + 1] = prefix[(size_t) i] + (double) x[i] * x[i];
    const double e1 = prefix[kW];
    if (e1 / kW < 1.0e-7) return res;   // тише −70 дБ — нот нет

    // взаимная корреляция окна W со всем кадром: r(τ) = IFFT (conj (A) · B)
    std::fill (a.begin(), a.end(), 0.0f);
    std::fill (b.begin(), b.end(), 0.0f);
    std::copy (x, x + kW, a.begin());
    std::copy (x, x + kN, b.begin());
    fft.performRealOnlyForwardTransform (a.data(), false);
    fft.performRealOnlyForwardTransform (b.data(), false);
    for (int k = 0; k < kFft; ++k)
    {
        const float ar = a[(size_t) (2 * k)], ai = a[(size_t) (2 * k + 1)];
        const float br = b[(size_t) (2 * k)], bi = b[(size_t) (2 * k + 1)];
        b[(size_t) (2 * k)] = ar * br + ai * bi;
        b[(size_t) (2 * k + 1)] = ar * bi - ai * br;
    }
    fft.performRealOnlyInverseTransform (b.data());
    // масштаб обратного FFT зависит от реализации — калибруем по r(0) = E1
    const double scale = std::abs (b[0]) > 1.0e-20f ? e1 / (double) b[0] : 1.0;

    // разностная функция и её нормировка (CMNDF)
    const int tauMin = juce::jmax (2, (int) std::floor (sr / kMaxFreq));
    const int tauMax = juce::jmin (kW - 2, (int) std::ceil (sr / kMinFreq));
    d[0] = 1.0;
    double running = 0.0;
    const int last = kW - 2;   // разностная функция нужна и дальше tauMax — для уточнения по k периодам
    for (int t = 1; t <= last; ++t)
    {
        const double e2 = prefix[(size_t) (t + kW)] - prefix[(size_t) t];
        const double diff = juce::jmax (0.0, e1 + e2 - 2.0 * scale * (double) b[(size_t) t]);
        raw[(size_t) t] = diff;
        running += diff;
        d[(size_t) t] = running > 0.0 ? diff * (double) t / running : 1.0;
    }

    // первая впадина ниже порога, затем спуск до её дна
    int tau = -1;
    for (int t = tauMin; t <= tauMax; ++t)
        if (d[(size_t) t] < 0.15)
        {
            while (t + 1 <= tauMax && d[(size_t) t + 1] < d[(size_t) t]) ++t;
            tau = t;
            break;
        }
    if (tau < 0)
    {
        int best = tauMin;
        for (int t = tauMin; t <= tauMax; ++t) if (d[(size_t) t] < d[(size_t) best]) best = t;
        if (d[(size_t) best] > 0.3) return res;
        tau = best;
    }

    // параболическое уточнение
    double tf = tau;
    if (tau > 1 && tau < tauMax)
    {
        const double y0 = d[(size_t) tau - 1], y1 = d[(size_t) tau], y2 = d[(size_t) tau + 1];
        const double den = y0 - 2.0 * y1 + y2;
        if (std::abs (den) > 1.0e-12) tf += juce::jlimit (-1.0, 1.0, 0.5 * (y0 - y2) / den);
    }
    // высокие ноты: период — всего несколько сэмплов, поэтому уточняем по впадине на k-м периоде
    // (глубина та же, а относительная точность в k раз выше)
    double periods = 1.0;
    if (tf < 200.0)
    {
        const int k = juce::jmin ((int) (400.0 / tf), (int) ((kW - 4) / tf));
        if (k >= 2)
        {
            const int centre = (int) std::round (tf * k), half = juce::jmax (1, (int) (tf * 0.4));
            int best = centre;
            for (int t = juce::jmax (2, centre - half); t <= juce::jmin (kW - 3, centre + half); ++t)
                if (raw[(size_t) t] < raw[(size_t) best]) best = t;
            double tk = best;
            const double y0 = raw[(size_t) best - 1], y1 = raw[(size_t) best], y2 = raw[(size_t) best + 1];
            const double den = y0 - 2.0 * y1 + y2;
            if (std::abs (den) > 1.0e-12) tk += juce::jlimit (-1.0, 1.0, 0.5 * (y0 - y2) / den);
            tf = tk;
            periods = k;
        }
    }
    res.freq = (float) (sr * periods / tf);
    res.clarity = (float) juce::jlimit (0.0, 1.0, 1.0 - d[(size_t) tau]);
    res.found = res.freq >= kMinFreq && res.freq <= kMaxFreq * 1.05f;
    return res;
}

juce::String PitchDetector::noteName (float f, float a4)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (f <= 0.0f) return {};
    const int n = juce::roundToInt (69.0f + 12.0f * std::log2 (f / a4));
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String ((int) std::floor ((float) n / 12.0f) - 1);
}

float PitchDetector::centsOff (float f, float a4)
{
    if (f <= 0.0f) return 0.0f;
    const float midi = 69.0f + 12.0f * std::log2 (f / a4);
    return (midi - (float) juce::roundToInt (midi)) * 100.0f;
}
} // namespace eido
