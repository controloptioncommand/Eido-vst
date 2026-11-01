#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace eido
{
// Кольцо последних ~1.3 с стереосигнала на стороне интерфейса. Модули читают из него каждый своим курсором.
struct SignalBuffer
{
    static constexpr int kRing = 1 << 16;
    std::vector<float> l = std::vector<float> (kRing, 0.0f), r = std::vector<float> (kRing, 0.0f);
    juce::int64 written = 0;
    double sampleRate = 48000.0;

    void push (const float* a, const float* b, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((written + i) & (kRing - 1));
            l[idx] = a[i];
            r[idx] = b[i];
        }
        written += n;
    }
    float left (juce::int64 i) const  { return l[(size_t) (i & (kRing - 1))]; }
    float right (juce::int64 i) const { return r[(size_t) (i & (kRing - 1))]; }
    float mono (juce::int64 i) const  { const auto k = (size_t) (i & (kRing - 1)); return 0.5f * (l[k] + r[k]); }

    // курсор не должен отставать больше чем на maxBack сэмплов
    juce::int64 clampCursor (juce::int64 cursor, int maxBack = kRing / 2) const
    {
        return juce::jlimit (written - (juce::int64) maxBack, written, cursor);
    }
};
} // namespace eido
