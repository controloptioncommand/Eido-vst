// Офлайн-тест Eido: сквозной звук, LUFS по эталонным сигналам, показания модулей, скриншоты интерфейса.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace eido;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "  FAIL: " << msg << "\n"; } } while (0)

// ---------------------------------------------------------------------------
// Генераторы
struct Gen
{
    enum Kind { Sine, SineInverted, Music, Silence };
    Kind kind = Sine;
    double sr = 48000.0, phase = 0.0, freq = 1000.0, t = 0.0;
    float amp = 0.1f;
    juce::Random rng { 7 };
    std::array<double, 6> saw {};

    void fill (juce::AudioBuffer<float>& b)
    {
        auto* l = b.getWritePointer (0);
        auto* r = b.getWritePointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float a = 0.0f, c = 0.0f;
            switch (kind)
            {
                case Sine:         a = c = amp * (float) std::sin (phase); break;
                case SineInverted: a = amp * (float) std::sin (phase); c = -a; break;
                case Silence:      break;
                case Music:
                {
                    // пэд: расстроенные пилы (A2 C#3 E3), бочка каждые 0.5 с, хэт на долях, медленная панорама
                    const double f[] = { 110.0, 110.6, 138.6, 139.3, 164.8, 165.5 };
                    float pad = 0.0f;
                    for (size_t k = 0; k < 6; ++k)
                    {
                        saw[k] += f[k] / sr;
                        saw[k] -= std::floor (saw[k]);
                        pad += (float) (saw[k] * 2.0 - 1.0) * 0.05f;
                    }
                    const double beat = std::fmod (t, 0.5);
                    const float kick = (float) (std::sin (juce::MathConstants<double>::twoPi * (50.0 + 120.0 * std::exp (-beat * 30.0)) * beat)
                                                * std::exp (-beat * 9.0)) * 0.45f;
                    const double hb = std::fmod (t + 0.25, 0.5);
                    const float hat = (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-hb * 60.0) * 0.12f;
                    const float pan = 0.5f + 0.35f * (float) std::sin (t * 0.9);
                    // широкая стереотекстура: независимый шум слева и справа, медленно дышит
                    const float air = 0.03f * (0.6f + 0.4f * (float) std::sin (t * 0.37));
                    a = kick + pad * (1.2f - pan) + hat * 0.7f + air * (rng.nextFloat() * 2.0f - 1.0f);
                    c = kick + pad * (0.4f + pan) + hat + air * (rng.nextFloat() * 2.0f - 1.0f);
                    break;
                }
            }
            l[i] = a;
            r[i] = c;
            phase += juce::MathConstants<double>::twoPi * freq / sr;
            if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
            t += 1.0 / sr;
        }
    }
};

static void run (EidoProcessor& p, Gen& g, double seconds, EidoEditor* ed = nullptr, int block = 512)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const int blocks = (int) (seconds * g.sr / block);
    for (int k = 0; k < blocks; ++k)
    {
        g.fill (buf);
        p.processBlock (buf, midi);
        if (ed != nullptr && (k % 2) == 1) ed->pumpForTest();
    }
}

static bool near (float v, float target, float tol) { return std::abs (v - target) <= tol; }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("render");
    outDir.createDirectory();

    // 1. Звук проходит без изменений
    {
        EidoProcessor p;
        p.prepareToPlay (48000.0, 512);
        Gen g; g.kind = Gen::Music;
        juce::AudioBuffer<float> buf (2, 512), copy (2, 512);
        juce::MidiBuffer midi;
        bool same = true;
        for (int k = 0; k < 200; ++k)
        {
            g.fill (buf);
            copy.makeCopyOf (buf);
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    if (buf.getSample (c, i) != copy.getSample (c, i)) same = false;
        }
        std::cout << "passthrough: " << (same ? "bit-exact" : "CHANGED") << "\n";
        CHECK (same, "audio must pass through unchanged");
    }

    // 2. LUFS: стереосинус 1 кГц с пиком −20 и −23 дБFS даёт −20 и −23 LUFS (±0.1)
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (float db : { -20.0f, -23.0f })
        {
            EidoProcessor p;
            p.prepareToPlay (sr, 512);
            Gen g; g.sr = sr; g.amp = juce::Decibels::decibelsToGain (db);
            run (p, g, 6.0);
            const float m = p.loudness.momentary(), s = p.loudness.shortTerm(), i = p.loudness.integrated();
            std::cout << "LUFS @" << sr << " sine " << db << " dBFS: M " << m << "  S " << s << "  I " << i << "\n";
            CHECK (near (m, db, 0.1f) && near (s, db, 0.1f) && near (i, db, 0.1f), "LUFS mismatch at " << sr << " / " << db);
        }

    // 3. Стробирование: 6 с сигнала и 6 с тишины — Integrated не падает; Reset сбрасывает
    {
        EidoProcessor p;
        p.prepareToPlay (48000.0, 512);
        Gen g; g.amp = juce::Decibels::decibelsToGain (-20.0f);
        run (p, g, 6.0);
        g.kind = Gen::Silence;
        run (p, g, 6.0);
        std::cout << "gating: I after silence " << p.loudness.integrated() << ", M " << p.loudness.momentary() << "\n";
        CHECK (near (p.loudness.integrated(), -20.0f, 0.15f), "integrated must ignore silence");
        CHECK (p.loudness.momentary() <= LoudnessMeter::kSilent, "momentary must fall to silence");
        // относительный порог: 6 с на −20 и 6 с на −40 → тихая часть ниже −10 LU отбрасывается
        g.kind = Gen::Sine; g.amp = juce::Decibels::decibelsToGain (-40.0f);
        run (p, g, 6.0);
        std::cout << "relative gate: I " << p.loudness.integrated() << "\n";
        CHECK (near (p.loudness.integrated(), -20.0f, 0.3f), "relative gate");
        p.loudness.requestReset();
        run (p, g, 1.0);
        CHECK (near (p.loudness.integrated(), -40.0f, 0.2f), "reset");
    }

    // 3б. LRA по EBU Tech 3342: 20 с синуса на −20 дБFS и 20 с на −30 → 10 LU (±1)
    {
        EidoProcessor p;
        p.prepareToPlay (48000.0, 512);
        Gen g; g.amp = juce::Decibels::decibelsToGain (-20.0f);
        run (p, g, 20.0);
        g.amp = juce::Decibels::decibelsToGain (-30.0f);
        run (p, g, 20.0);
        std::cout << "LRA " << p.loudness.loudnessRange() << " LU\n";
        CHECK (near (p.loudness.loudnessRange(), 10.0f, 1.0f), "LRA must be 10 LU");
    }

    // 3в. Высота тона: синусы и пила, 44.1 и 48 кГц — ошибка меньше 3 центов, без октавных ошибок
    {
        PitchDetector det;
        for (double sr : { 44100.0, 48000.0 })
            for (float f : { 30.0f, 55.0f, 110.0f, 220.0f, 440.0f, 1000.0f, 2500.0f, 4853.0f })
                for (int saw = 0; saw < 2; ++saw)
                {
                    std::vector<float> x ((size_t) PitchDetector::kN);
                    double ph = 0.0;
                    for (auto& v : x)
                    {
                        v = saw ? (float) (ph * 2.0 - 1.0) * 0.3f : (float) std::sin (ph * juce::MathConstants<double>::twoPi) * 0.3f;
                        ph += f / sr;
                        ph -= std::floor (ph);
                    }
                    if (saw && f > 2000.0f) continue;   // пила выше 2 кГц без антиалиасинга — не честный тест
                    const auto r = det.analyse (x.data(), sr);
                    const float err = r.found ? 1200.0f * std::log2 (r.freq / f) : 9999.0f;
                    if (std::abs (err) > 3.0f)
                        std::cout << "  pitch " << (saw ? "saw " : "sine ") << f << " Hz @" << sr << ": " << r.freq << " (" << err << " ct)\n";
                    CHECK (std::abs (err) <= 3.0f, "pitch " << f << " Hz");
                }
        std::cout << "pitch: notes " << PitchDetector::noteName (440.0f) << " " << PitchDetector::noteName (523.25f) << " "
                  << PitchDetector::noteName (73.42f) << " " << PitchDetector::noteName (4853.0f) << "\n";
        CHECK (PitchDetector::noteName (440.0f) == "A4" && PitchDetector::noteName (523.25f) == "C5"
               && PitchDetector::noteName (73.42f) == "D2" && PitchDetector::noteName (4853.0f) == "D#8", "note names");
        std::vector<float> silence ((size_t) PitchDetector::kN, 0.0f);
        CHECK (! det.analyse (silence.data(), 48000.0).found, "silence has no pitch");
    }

    // 4. Модули и скриншоты
    {
        EidoProcessor p;
        p.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> base (p.createEditor());
        auto* ed = dynamic_cast<EidoEditor*> (base.get());
        ed->setThemeForTest (ThemeId::Oxygen);
        ed->setLayout (EidoEditor::Layout::Grid);
        ed->setSize (EidoEditor::kGridW, EidoEditor::kGridH);
        ed->setModule (0, ModuleType::Spectrum);
        ed->setModule (1, ModuleType::Stereo);
        ed->setModule (2, ModuleType::VU);
        ed->setModule (3, ModuleType::Loudness);
        ed->setModule (4, ModuleType::Tuner);
        ed->setModule (5, ModuleType::Oscilloscope);
        ed->setModule (6, ModuleType::MidSide);
        ed->setModule (7, ModuleType::PeakHistory);
        (void) ed->createComponentSnapshot (ed->getLocalBounds());   // разметка графиков считается при отрисовке

        Gen g;
        g.amp = juce::Decibels::decibelsToGain (-15.0f);   // синус с пиком −15 дБFS = −18 дБFS RMS = 0 VU
        run (p, g, 2.0, ed);
        auto* sp = dynamic_cast<SpectrumModule*> (ed->getModule (0));
        auto* stereo = dynamic_cast<StereoModule*> (ed->getModule (1));
        auto* vu = dynamic_cast<VuModule*> (ed->getModule (2));
        auto* loud = dynamic_cast<LoudnessModule*> (ed->getModule (3));
        std::cout << "spectrum peak " << sp->peakFrequency() << " Hz, correlation " << stereo->getCorrelation()
                  << ", VU " << vu->getVu() << ", peak hold " << loud->getPeakHoldDb() << " dB\n";
        CHECK (near (sp->peakFrequency(), 1000.0f, 3.0f), "spectrum peak frequency");
        CHECK (stereo->getCorrelation() > 0.98f, "correlation of mono signal");
        CHECK (near (vu->getVu(), 0.0f, 0.3f), "VU: -18 dBFS RMS must read 0 VU");
        CHECK (near (loud->getPeakHoldDb(), -15.0f, 0.1f), "sample peak");
        {
            const auto txt = sp->readoutForTest (300.0f);
            std::cout << "spectrum readout: " << txt << "\n";
            CHECK (txt.contains ("Hz") && txt.contains ("ct"), "spectrum readout");
        }
        auto* tuner = dynamic_cast<TunerModule*> (ed->getModule (4));
        auto* osc = dynamic_cast<OscilloscopeModule*> (ed->getModule (5));
        auto* ms = dynamic_cast<SpectrumModule*> (ed->getModule (6));
        std::cout << "tuner " << tuner->getNote() << " " << tuner->getFrequency() << " Hz, oscilloscope " << osc->getSpanMs() << " ms\n";
        CHECK (tuner->hasPitch() && tuner->getNote() == "B5" && near (tuner->getFrequency(), 1000.0f, 1.0f), "tuner 1 kHz");
        CHECK (near (osc->getSpanMs(), 8.0f, 0.05f), "oscilloscope: 8 periods of 1 kHz");
        CHECK (ms->levelAt (1000.0f) > -20.0f && ms->levelAt (1000.0f, true) < -80.0f, "mid/side: mono signal has no side");
        g.kind = Gen::SineInverted;
        run (p, g, 3.0, ed);
        std::cout << "inverted correlation " << stereo->getCorrelation() << ", mid " << ms->levelAt (1000.0f) << " side " << ms->levelAt (1000.0f, true) << "\n";
        CHECK (stereo->getCorrelation() < -0.98f, "correlation of inverted signal");
        CHECK (ms->levelAt (1000.0f, true) > -20.0f && ms->levelAt (1000.0f) < -80.0f, "mid/side: inverted signal is all side");

        // перетаскивание: модуль 0 бросили на модуль 3 — поменялись местами
        {
            auto* m0 = ed->getModule (0);
            const auto t3 = ed->getModule (3)->getType();
            m0->onDrop (*m0, ed->getModule (3)->getBounds().getCentre());
            CHECK (ed->getModule (3) == m0 && ed->getModule (0)->getType() == t3, "drag and drop swap");
            ed->getModule (0)->onDrop (*ed->getModule (0), ed->getModule (3)->getBounds().getCentre());
        }

        // смена модуля, перестановка, сохранение состояния
        ed->moveModule (0, 1);
        CHECK (ed->getModule (1)->getType() == ModuleType::Spectrum, "move module");
        juce::MemoryBlock state;
        p.getStateInformation (state);
        {
            EidoProcessor p2;
            p2.setStateInformation (state.getData(), (int) state.getSize());
            CHECK (p2.uiState.getProperty ("slots").toString() == p.uiState.getProperty ("slots").toString(), "state: slots");
            CHECK ((int) p2.uiState.getProperty ("layout") == 1, "state: layout");
        }
        // раскладка по умолчанию для скриншотов
        const ModuleType def[] = { ModuleType::Spectrogram, ModuleType::Scope, ModuleType::Oscilloscope, ModuleType::Spectrum,
                                   ModuleType::Loudness, ModuleType::Stereo, ModuleType::VU, ModuleType::Tuner };
        for (int i = 0; i < EidoEditor::kSlots; ++i) ed->setModule (i, def[i]);
        for (int i = 0; i < EidoEditor::kSlots; ++i) ed->getModule (i)->reset();
        p.loudness.requestReset();

        auto shoot = [&] (const juce::String& name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (name + ".png");
            f.deleteFile();
            juce::FileOutputStream fos (f);
            juce::PNGImageFormat().writeImageToStream (img, fos);
            std::cout << "  wrote " << f.getFullPathName() << "\n";
        };

        g = Gen(); g.kind = Gen::Music;
        run (p, g, 4.0, ed);
        for (auto theme : { ThemeId::Oxygen, ThemeId::Rubber })
        {
            const juce::String tn = theme == ThemeId::Oxygen ? "oxygen" : "rubber";
            ed->setThemeForTest (theme);
            ed->setLayout (EidoEditor::Layout::Grid);
            run (p, g, 3.0, ed);
            shoot ("ui_" + tn + "_grid");
            {
                // скорость отрисовки сетки
                double paintMs = 0;
                for (int k = 0; k < 20; ++k)
                {
                    run (p, g, 0.02, ed);
                    const auto a = juce::Time::getMillisecondCounterHiRes();
                    (void) ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                    paintMs += juce::Time::getMillisecondCounterHiRes() - a;
                }
                std::cout << "  grid paint: " << juce::String (paintMs / 20.0, 2) << " ms/frame\n";
            }
            {
                // остальные модули
                const ModuleType more[] = { ModuleType::Waves, ModuleType::PeakHistory, ModuleType::MidSide, ModuleType::LoudnessHistory };
                const int slots[] = { 1, 2, 3, 4 };
                for (int k = 0; k < 4; ++k) ed->setModule (slots[k], more[k]);
                run (p, g, 8.0, ed);
                shoot ("ui_" + tn + "_grid_more");
                for (int k = 0; k < 4; ++k) ed->setModule (slots[k], def[slots[k]]);
                run (p, g, 2.0, ed);
            }
            ed->setLayout (EidoEditor::Layout::Strip);
            run (p, g, 3.0, ed);
            shoot ("ui_" + tn + "_strip");

            ed->showAboutForTest (true, false);
            CHECK (ed->getWidth() >= 900 && ed->getHeight() >= 600, "about: window must grow from strip");
            shoot ("about_" + tn);
            {
                auto& fog = ed->getAboutForTest().getFogForTest();
                std::cout << "  fog particles: " << fog.getNumParticles() << "\n";
                CHECK (fog.getNumParticles() > 150, "fog: too few particles");
                const auto c = fog.getLocalBounds().getCentre().toFloat();
                fog.blowForTest ({ c.x - 160.0f, c.y }, { c.x + 40.0f, c.y + 10.0f }, 12);
                CHECK (fog.isAnimating(), "fog: did not start");
                fog.stepForTest (8);
                shoot ("about_" + tn + "_fog");
                fog.stepForTest (90);
                CHECK (! fog.isAnimating(), "fog: did not settle within ~1.5 s");
            }
            ed->showAboutForTest (true, true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            shoot ("game_" + tn);
            ed->showAboutForTest (false, false);
            {
                // Tab пропускает логотип; Escape закрывает игру, затем About
                auto& about = ed->getAboutForTest();
                bool logoFocusable = false;
                std::function<void (juce::Component&)> scan = [&] (juce::Component& c)
                {
                    if (dynamic_cast<juce::Button*> (&c) != nullptr && c.getName() == "Eido" && c.getWantsKeyboardFocus()) logoFocusable = true;
                    for (auto* ch : c.getChildren()) scan (*ch);
                };
                scan (*ed);
                CHECK (! logoFocusable, "logo must not take keyboard focus");
                ed->showAboutForTest (true, true);
                about.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                CHECK (about.isVisible(), "escape in game closes only the game");
                about.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                CHECK (! about.isVisible() && ed->getHeight() == EidoEditor::kStripH, "escape closes About and restores size");
            }
            CHECK (ed->getHeight() == EidoEditor::kStripH, "about: strip size must be restored, got " << ed->getWidth() << "x" << ed->getHeight());
        }
        ed->setThemeForTest (ThemeId::Oxygen);
        base.reset();
    }

    std::cout << (failures == 0 ? "ALL OK\n" : juce::String (failures) + " FAILURE(S)\n");
    return failures == 0 ? 0 : 1;
}
