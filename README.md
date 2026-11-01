<p align="center">
  <img src="docs/assets/ui_rubber_strip.png" alt="Eido в режиме Strip, тёмная тема Rubber: тулбар с пиксельным логотипом, ниже в ряд спектрограмма, волна, громкость, стереометр, VU и спектр." width="100%">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-1-FF6B2C?style=flat-square&labelColor=18191C" alt="version 1">
  <img src="https://img.shields.io/badge/macOS-Apple%20Silicon-EDEDE9?style=flat-square&labelColor=18191C" alt="macOS, Apple Silicon">
  <img src="https://img.shields.io/badge/formats-VST3%20%C2%B7%20AU-8EA0FF?style=flat-square&labelColor=18191C" alt="VST3 and AU">
  <img src="https://img.shields.io/badge/vibe-coded-FF7AA2?style=flat-square&labelColor=18191C" alt="vibe-coded">
</p>

<p align="center">
  <a href="https://github.com/controloptioncommand/Eido-vst/releases/latest/download/Eido-macOS.zip"><b>⬇ Скачать Eido для macOS</b></a>
</p>

---

**Eido** (от греческого «созерцать») — вайбкод-плагин: визуализатор и измеритель для мастер-канала или любой дорожки. Звук проходит без изменений, а вы смотрите на него шестью приборами сразу.

Брат синтезатора [Mist](https://github.com/controloptioncommand/Mist-vst): те же темы Oxygen и Rubber, те же шрифты Medeo и те же виды волны. Компоновка — в духе MiniMeters.

## Приборы

| Модуль | Что показывает |
|---|---|
| **Scope** | Плотная волна: полоска на каждый пиксель, цвет по громкости от −24 до 0 дБ, направляющие −6 и −12 дБ. |
| **Waves** | История волны «стопкой». Передняя линия — текущий момент. |
| **Spectrogram** | Спектрограмма: по горизонтали время, по вертикали частота, цвет — громкость. |
| **Spectrum** | Спектр с наклоном 0 / 3 / 4.5 дБ на октаву. Наведите курсор — увидите уровень, частоту, ноту и отклонение в центах. |
| **Loudness** | Пики L / R с удержанием, Momentary и Short-term столбиками, Integrated в LUFS по EBU R128. |
| **VU** | Стрелочный VU-метр, 0 VU = −18 dBFS. |
| **Stereo** | Полярный стереометр и корреляция фаз от −1 до +1. |

У каждого модуля снизу справа стрелки **‹ ▲ ›**: ‹ › переставляют модуль, ▲ открывает меню, где можно заменить его другим. Шесть слотов, любые модули в любом порядке. Раскладка сохраняется в проекте.

Integrated считается прямо в аудиопотоке, поэтому продолжает копиться, даже когда окно плагина закрыто. Кнопка со стрелкой по кругу сбрасывает его вместе с пиками, пауза замораживает картинку.

## Два режима окна

**Strip** — длинная полоса, как у MiniMeters. Удобно держать над треками.

**Grid** — окно как у Mist: сетка 3 × 2 из крупных модулей.

<p align="center">
  <img src="docs/assets/ui_rubber_grid.png" alt="Режим Grid в тёмной теме: спектрограмма, Scope, Loudness, стереометр, VU и спектр в сетке 3 на 2." width="100%">
</p>

Окно меняет размер как угодно, размер запоминается для каждого режима.

## Две темы

**Oxygen** — светлая, **Rubber** — тёмная. Можно следовать системной теме macOS или выбрать вручную: иконка настроек → Appearance. Один резкий оранжевый сигнальный цвет, плотные поверхности без стекла и теней. Интерфейс набран шрифтами Medeo: Sans, Mono для чисел и Pixel для логотипа.

<p align="center">
  <img src="docs/assets/ui_oxygen_grid.png" alt="Режим Grid в светлой теме Oxygen." width="100%">
</p>

## Установка

1. Скачайте [Eido-macOS.zip](https://github.com/controloptioncommand/Eido-vst/releases/latest/download/Eido-macOS.zip) и распакуйте.
2. В Терминале: `bash ~/Downloads/Eido-macOS/install-mac.sh`
3. В Ableton: Settings → Plug-Ins → Rescan. Eido появится среди аудиоэффектов.

Плагин подписан ad-hoc (без Apple Developer ID), скрипт снимает карантин macOS.

## Сборка

```bash
cmake -B build -DEIDO_BUILD_TESTS=ON && cmake --build build --config Release --target Eido_VST3 Eido_AU EidoTest
```

JUCE 8 скачивается автоматически. Тест `EidoTest` проверяет громкость по эталонным сигналам, показания приборов и снимает скриншоты интерфейса.

## Лицензии

Шрифты Medeo — SIL Open Font License, см. `Resources/Fonts/OFL.txt`.
