#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include "Analysis/SignalBuffer.h"
#include "Analysis/Loudness.h"
#include <deque>

namespace eido
{
enum class ModuleType { Scope = 0, Waves, Spectrogram, Spectrum, Loudness, VU, Stereo, Count };
juce::String moduleName (ModuleType t);
juce::String moduleKey (ModuleType t);                 // для сохранения в состоянии
ModuleType moduleFromKey (const juce::String& key, ModuleType fallback);

// Модуль визуализации — одна ячейка полосы или сетки.
// Фон, скругления, плашки (имя, легенда) и стрелки ‹ ▲ › рисует база; содержимое — наследник.
class Module : public juce::Component
{
public:
    Module (ModuleType t, const SignalBuffer& s);
    ModuleType getType() const { return type; }

    virtual void tick() = 0;          // ~60 раз в секунду: забрать новые сэмплы
    virtual void reset() {}           // кнопка Reset: пики, история
    virtual void addMenuItems (juce::PopupMenu&) {}   // свои пункты в меню ▲ (id 1000+)
    virtual void handleMenuResult (int) {}

    void setCorners (bool tl, bool tr, bool bl, bool br) { corners = { tl, tr, bl, br }; repaint(); }
    void setCompact (bool c) { compact = c; resized(); repaint(); }   // режим полосы: плашки только при наведении
    bool isCompact() const { return compact; }

    std::function<void (Module&, int dir)> onMove;   // ‹ ›
    std::function<void (Module&)> onMenu;            // ▲

    void paint (juce::Graphics& g) override final;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseMove (const juce::MouseEvent& e) override { pointer = e.position; mouseMoved(); }
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void resized() override;

    juce::Colour background() const;
    juce::Colour plateFill() const;   // плашки на виде: фон Surface — плашки Well, и наоборот

protected:
    virtual void paintContent (juce::Graphics& g, juce::Rectangle<float> area) = 0;
    virtual bool wantsLegend() const { return false; }
    virtual void paintTopRight (juce::Graphics&, juce::Rectangle<float>) {}   // вместо легенды (например, подсказка спектра)
    virtual void mouseMoved() {}

    bool controlsVisible() const { return hover || hasKeyboardFocus (false); }
    bool chipsVisible() const { return ! compact || controlsVisible(); }
    bool isHovering() const { return hover; }
    juce::Point<float> getPointer() const { return pointer; }
    juce::Rectangle<float> legendBounds() const { return legendArea; }
    void paintLegend (juce::Graphics& g);

    const SignalBuffer& sig;
    juce::int64 cursor = 0;

private:
    ModuleType type;
    std::array<bool, 4> corners { true, true, true, true };
    bool compact = false, hover = false;
    juce::Point<float> pointer;
    juce::Rectangle<float> controlsArea, nameChip, legendArea;
};

std::unique_ptr<Module> createModule (ModuleType t, const SignalBuffer& s, const LoudnessMeter& loud);

// ===========================================================================
// Scope — плотная волна: одна полоска на колонку пикселей, цвет по громкости
class ScopeModule : public Module
{
public:
    explicit ScopeModule (const SignalBuffer& s);
    void tick() override;
    void reset() override { cols.clear(); }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
    bool wantsLegend() const override { return true; }

private:
    struct Col { float mn = 0, mx = 0, db = -100.0f; };
    std::deque<Col> cols;
};

// Waves — история волны «стопкой», передняя линия — сигнальным
class WavesModule : public Module
{
public:
    explicit WavesModule (const SignalBuffer& s);
    void tick() override;
    void reset() override;

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    static constexpr int kPoints = 220, kMaxLines = 26;
    std::deque<std::vector<float>> lines;
    int ticks = 0;
    float gain = 1.0f;
};

// Spectrogram — по горизонтали время, по вертикали частота, цвет — громкость
class SpectrogramModule : public Module
{
public:
    explicit SpectrogramModule (const SignalBuffer& s);
    void tick() override;
    void reset() override;

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
    bool wantsLegend() const override { return true; }

private:
    static constexpr int kOrder = 12, kSize = 1 << kOrder, kHop = 512, kColW = 2;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> buf = std::vector<float> (kSize * 2, 0.0f);
    std::deque<std::vector<juce::uint8>> history;
    juce::Image image;
    int rows = 0;
    double rate = 0.0;
    std::vector<std::pair<int, int>> bins;
    std::vector<float> tilt;
    std::array<juce::PixelARGB, 256> lut {};
    juce::String lutKey;
    void prepare (int w, int h);
    void updateLut();
    void drawColumn (int x, const std::vector<juce::uint8>& col);
};

// Spectrum — FFT-кривая; под курсором — уровень, частота, нота и центы
class SpectrumModule : public Module
{
public:
    explicit SpectrumModule (const SignalBuffer& s);
    void tick() override;
    void reset() override { std::fill (smooth.begin(), smooth.end(), -120.0f); }
    void addMenuItems (juce::PopupMenu&) override;
    void handleMenuResult (int) override;
    float getTilt() const { return tiltPerOct; }
    void setTilt (float t) { tiltPerOct = t; repaint(); }
    // для тестов: частота самого громкого пика
    float peakFrequency() const;
    juce::String readoutForTest (float x) const { return readout (x); }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
    void paintTopRight (juce::Graphics&, juce::Rectangle<float>) override;
    void mouseMoved() override { repaint(); }

private:
    static constexpr int kOrder = 13, kSize = 1 << kOrder;
    juce::String readout (float x) const;
    float freqAt (float x) const;
    float dbAt (float f) const;          // сглаженный уровень без наклона
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::blackmanHarris };
    std::vector<float> buf = std::vector<float> (kSize * 2, 0.0f);
    std::vector<float> smooth = std::vector<float> (kSize / 2, -120.0f);   // дБ по бинам
    float tiltPerOct = 4.5f;
    juce::Rectangle<float> plot;
};

// Loudness — пики L/R, Momentary и Short-term столбиками, цифры M / S / I / Peak
class LoudnessModule : public Module
{
public:
    LoudnessModule (const SignalBuffer& s, const LoudnessMeter& m);
    void tick() override;
    void reset() override;
    float getPeakHoldDb() const { return maxPeak; }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    const LoudnessMeter& meter;
    std::array<float, 2> peak { -100.0f, -100.0f }, hold { -100.0f, -100.0f };
    std::array<double, 2> holdTime { 0, 0 };
    float maxPeak = -100.0f;
    double lastTick = 0;
    int ticks = 0;
};

// VU — стрелочный индикатор, 0 VU = −18 dBFS (RMS)
class VuModule : public Module
{
public:
    explicit VuModule (const SignalBuffer& s);
    void tick() override;
    void reset() override { ms = 0; needle = -40.0f; }
    float getVu() const { return needle; }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    double ms = 0.0;
    float needle = -40.0f;
};

// Stereometer — полярный гониометр (полукруг) и корреляция −1…+1
class StereoModule : public Module
{
public:
    explicit StereoModule (const SignalBuffer& s);
    void tick() override;
    void reset() override;
    float getCorrelation() const { return corr; }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    // след: точки последних кадров (координаты в долях радиуса, x — сторона, y — середина), старые бледнеют
    struct Dot { float x, y; juce::uint8 bucket; };
    static constexpr int kTrailFrames = 10;
    std::deque<std::vector<Dot>> trail;
    float corr = 0.0f;
    bool hasCorr = false;
};
} // namespace eido
