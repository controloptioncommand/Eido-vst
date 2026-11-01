#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Analysis/Loudness.h"

namespace eido
{
// Eido — визуализатор. Звук проходит без изменений; копия сигнала уходит в интерфейс через FIFO,
// громкость (LUFS) считается прямо здесь, на аудиопотоке.
class EidoProcessor : public juce::AudioProcessor
{
public:
    EidoProcessor();

    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Eido"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // копия сигнала для интерфейса
    static constexpr int kTapSize = 1 << 17;
    juce::AbstractFifo tapFifo { kTapSize };
    std::vector<float> tapL = std::vector<float> (kTapSize, 0.0f), tapR = std::vector<float> (kTapSize, 0.0f);
    std::atomic<double> rate { 48000.0 };

    LoudnessMeter loudness;

    // состояние интерфейса (раскладка, модули, размеры окна) — сохраняется в проекте
    juce::ValueTree uiState { "EidoUI" };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EidoProcessor)
};
} // namespace eido
