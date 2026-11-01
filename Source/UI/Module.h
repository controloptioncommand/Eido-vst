#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include "Analysis/SignalBuffer.h"
#include "Analysis/Loudness.h"
#include "Analysis/Pitch.h"
#include <deque>

namespace eido
{
// Новые типы добавлять только в конец: ключи модулей хранятся в проектах
enum class ModuleType { Scope = 0, Waves, Spectrogram, Spectrum, Loudness, VU, Stereo,
                        Tuner, Oscilloscope, MidSide, LoudnessHistory, PeakHistory, Count };
juce::String moduleName (ModuleType t);
juce::String moduleKey (ModuleType t);                 // для сохранения в состоянии
ModuleType moduleFromKey (const juce::String& key, ModuleType fallback);

// Группы в меню ▲ (порядок групп и модулей внутри — как в меню)
struct ModuleGroup { const char* name; std::vector<ModuleType> types; };
const std::vector<ModuleGroup>& moduleGroups();

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
    // свои настройки модуля (наклон спектра, эталон тюнера) — хранятся в состоянии проекта
    virtual void loadSettings (const juce::ValueTree&) {}
    virtual void saveSettings (juce::ValueTree&) const {}

    void setCorners (bool tl, bool tr, bool bl, bool br) { corners = { tl, tr, bl, br }; repaint(); }
    void setCompact (bool c) { compact = c; resized(); repaint(); }   // режим полосы: плашки только при наведении
    bool isCompact() const { return compact; }

    std::function<void (Module&, int dir)> onMove;   // ‹ ›
    std::function<void (Module&)> onMenu;            // ▲
    // перетаскивание модуля на другой слот: позиция — в координатах родителя
    std::function<void (Module&, juce::Point<int>)> onDrag, onDrop;

    void paint (juce::Graphics& g) override final;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseMove (const juce::MouseEvent& e) override { pointer = e.position; mouseMoved(); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
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
    bool compact = false, hover = false, dragging = false, pressOnControls = false;
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
// Mid / Side — тот же анализатор, две кривые: середина (L+R)/2 и сторона (L−R)/2
class SpectrumModule : public Module
{
public:
    explicit SpectrumModule (const SignalBuffer& s, bool midSide = false);
    void tick() override;
    void reset() override { std::fill (smooth.begin(), smooth.end(), -120.0f); std::fill (side.begin(), side.end(), -120.0f); }
    void addMenuItems (juce::PopupMenu&) override;
    void handleMenuResult (int) override;
    void loadSettings (const juce::ValueTree& v) override { tiltPerOct = (float) (double) v.getProperty ("tilt", 4.5); }
    void saveSettings (juce::ValueTree& v) const override { v.setProperty ("tilt", tiltPerOct, nullptr); }
    float getTilt() const { return tiltPerOct; }
    void setTilt (float t) { tiltPerOct = t; repaint(); }
    // для тестов: частота самого громкого пика
    float peakFrequency() const;
    float levelAt (float freq, bool sideCurve = false) const { return dbAt (freq, sideCurve ? side : smooth); }
    juce::String readoutForTest (float x) const { return readout (x); }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
    void paintTopRight (juce::Graphics&, juce::Rectangle<float>) override;
    void mouseMoved() override { repaint(); }

private:
    static constexpr int kOrder = 13, kSize = 1 << kOrder;
    juce::String readout (float x) const;
    float freqAt (float x) const;
    float dbAt (float f, const std::vector<float>& curve) const;   // сглаженный уровень без наклона
    float dbAt (float f) const { return dbAt (f, smooth); }
    void analyse (bool sideSignal, std::vector<float>& into);
    juce::Path curvePath (const std::vector<float>& curve, juce::RectangleList<float>* buckets) const;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::blackmanHarris };
    std::vector<float> buf = std::vector<float> (kSize * 2, 0.0f);
    std::vector<float> smooth = std::vector<float> (kSize / 2, -120.0f);   // дБ по бинам (в Mid / Side — середина)
    std::vector<float> side = std::vector<float> (kSize / 2, -120.0f);
    bool midSide = false;
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
// ===========================================================================
// Tuner — нота (C5), частота целым числом (4853 Hz) и отклонение в центах −50…+50
class TunerModule : public Module
{
public:
    explicit TunerModule (const SignalBuffer& s);
    void tick() override;
    void reset() override { history.clear(); freq = 0.0f; heldFor = 0; }
    void addMenuItems (juce::PopupMenu&) override;
    void handleMenuResult (int) override;
    void loadSettings (const juce::ValueTree& v) override { a4 = (float) (double) v.getProperty ("a4", 440.0); }
    void saveSettings (juce::ValueTree& v) const override { v.setProperty ("a4", a4, nullptr); }
    float getFrequency() const { return freq; }
    bool hasPitch() const { return live; }
    juce::String getNote() const { return PitchDetector::noteName (freq, a4); }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    PitchDetector detector;
    std::deque<float> history;   // последние оценки (медиана убирает случайные октавные скачки)
    float freq = 0.0f, a4 = 440.0f, shownCents = 0.0f;
    bool live = false;
    int ticks = 0, heldFor = 0;
};

// Oscilloscope — несколько периодов волны, синхронизированных по высоте тона: картинка стоит на месте
class OscilloscopeModule : public Module
{
public:
    explicit OscilloscopeModule (const SignalBuffer& s);
    void tick() override;
    void reset() override { trace.clear(); }
    float getSpanMs() const { return spanMs; }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    PitchDetector detector;
    std::vector<float> trace;
    float spanMs = 20.0f, pitch = 0.0f;
    int ticks = 0;
};

// Loudness History — Short-term за последние 30 с, Momentary тонко, Integrated пунктиром, LRA
class LoudnessHistoryModule : public Module
{
public:
    LoudnessHistoryModule (const SignalBuffer& s, const LoudnessMeter& m);
    void tick() override;
    void reset() override { points.clear(); }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    static constexpr int kPoints = 300;   // 30 с по 100 мс
    const LoudnessMeter& meter;
    struct Pt { float s, m; };
    std::deque<Pt> points;
};

// Peak History — лента пиков и RMS: L сверху, R снизу, шкала в дБ
class PeakHistoryModule : public Module
{
public:
    explicit PeakHistoryModule (const SignalBuffer& s);
    void tick() override;
    void reset() override { cols.clear(); }

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    struct Col { float peakL, peakR, rmsL, rmsR; };
    std::deque<Col> cols;
};
} // namespace eido
