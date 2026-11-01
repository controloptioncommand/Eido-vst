#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

namespace eido
{
// Громкость по ITU-R BS.1770-4 / EBU R128: K-фильтр, Momentary (400 мс), Short-term (3 с),
// Integrated (стробирование −70 LUFS и −10 LU). Считается на аудиопотоке без выделений памяти,
// поэтому Integrated продолжает копиться и при закрытом окне плагина. Результаты — в атомиках.
class LoudnessMeter
{
public:
    static constexpr float kSilent = -100.0f;   // «−∞»

    void prepare (double sampleRate);
    // r == nullptr — моно
    void process (const float* l, const float* r, int n);
    void requestReset() { resetRequested.store (true); }

    float momentary() const  { return mLufs.load(); }
    float shortTerm() const  { return sLufs.load(); }
    float integrated() const { return iLufs.load(); }
    float maxMomentary() const { return mMax.load(); }
    float loudnessRange() const { return lraLu.load(); }   // LRA, LU (EBU Tech 3342); 0 — пока нет данных

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double run (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() { z1 = z2 = 0; }
    };
    void clear();
    void finishSubBlock();
    static float toLufs (double meanSquare) { return meanSquare > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : kSilent; }

    std::array<Biquad, 2> shelf, highpass;
    int subLen = 4800, subPos = 0;
    double subSum = 0.0;

    // энергии 100-мс подблоков: Momentary — последние 4, Short-term — последние 30
    std::array<double, 30> subs {};
    int subCount = 0, subWrite = 0;

    // гистограмма 400-мс блоков для Integrated: −70…+10 LUFS шагом 0.1
    static constexpr int kBins = 800;
    std::array<juce::uint32, kBins> histCount {};
    std::array<double, kBins> histEnergy {};
    // гистограмма Short-term для LRA
    std::array<juce::uint32, kBins> stCount {};
    std::array<double, kBins> stEnergy {};

    std::atomic<float> mLufs { kSilent }, sLufs { kSilent }, iLufs { kSilent }, mMax { kSilent }, lraLu { 0.0f };
    std::atomic<bool> resetRequested { false };
};
} // namespace eido
