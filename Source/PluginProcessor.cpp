#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace eido
{
EidoProcessor::EidoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool EidoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void EidoProcessor::prepareToPlay (double sampleRate, int)
{
    rate = sampleRate > 0 ? sampleRate : 48000.0;
    loudness.prepare (rate.load());
}

void EidoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int c = inCh; c < outCh; ++c) buffer.clear (c, 0, n);
    if (n == 0 || inCh == 0) return;

    const float* l = buffer.getReadPointer (0);
    const float* r = inCh > 1 ? buffer.getReadPointer (1) : nullptr;
    loudness.process (l, r, n);

    // в интерфейс; если окно закрыто и FIFO полон — лишнее просто не пишется
    const int toWrite = juce::jmin (n, tapFifo.getFreeSpace());
    if (toWrite > 0)
    {
        const auto w = tapFifo.write (toWrite);
        auto copy = [&] (int start, int size, int offset)
        {
            if (size <= 0) return;
            std::copy (l + offset, l + offset + size, tapL.begin() + start);
            std::copy ((r ? r : l) + offset, (r ? r : l) + offset + size, tapR.begin() + start);
        };
        copy (w.startIndex1, w.blockSize1, 0);
        copy (w.startIndex2, w.blockSize2, w.blockSize1);
    }
}

juce::AudioProcessorEditor* EidoProcessor::createEditor() { return new EidoEditor (*this); }

void EidoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = uiState.createXml()) copyXmlToBinary (*xml, dest);
}

void EidoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (uiState.getType()))
            uiState.copyPropertiesAndChildrenFrom (juce::ValueTree::fromXml (*xml), nullptr);
}
} // namespace eido

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new eido::EidoProcessor(); }
