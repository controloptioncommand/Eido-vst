#pragma once
#include <juce_dsp/juce_dsp.h>
#include "SignalBuffer.h"

namespace eido
{
// Определение высоты тона: YIN (de Cheveigné & Kawahara, 2002). Разностная функция считается через FFT,
// поэтому кадр 4096 сэмплов обходится дёшево. Диапазон ~25 Гц … 5 кГц.
class PitchDetector
{
public:
    struct Result { float freq = 0.0f, clarity = 0.0f; bool found = false; };

    // анализ последних kN сэмплов моно-сигнала
    Result analyse (const SignalBuffer& sig);
    Result analyse (const float* x, double sampleRate);   // x — kN сэмплов

    static constexpr int kN = 4096, kW = 2048;   // кадр и окно интегрирования
    static constexpr float kMinFreq = 25.0f, kMaxFreq = 5000.0f;

    // нота по частоте: «A4», отклонение в центах; a4 — эталон (по умолчанию 440 Гц)
    static juce::String noteName (float freq, float a4 = 440.0f);
    static float centsOff (float freq, float a4 = 440.0f);

private:
    static constexpr int kOrder = 13, kFft = 1 << kOrder;
    juce::dsp::FFT fft { kOrder };
    std::vector<float> frame = std::vector<float> (kN, 0.0f);
    std::vector<float> a = std::vector<float> (kFft * 2, 0.0f), b = std::vector<float> (kFft * 2, 0.0f);
    std::vector<double> d = std::vector<double> (kW, 0.0), raw = std::vector<double> (kW, 0.0), prefix = std::vector<double> (kN + 1, 0.0);
};
} // namespace eido
