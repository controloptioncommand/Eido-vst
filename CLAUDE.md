# Правила проекта Eido

Eido (от греч. «созерцать») — вайбкод VST3/AU-визуализатор для macOS, C++ / JUCE 8. Брат синтезатора Mist: те же темы, шрифты и страница About.
Репозиторий публичный: https://github.com/controloptioncommand/Eido-vst

Перед любой работой с интерфейсом прочитай **[docs/DESIGN.md](docs/DESIGN.md)** — это обязательная дизайн-система.

## Общение
- Отвечать по-русски, коротко и по делу. Не начинать ответ с похвалы.
- Менять мнение только на аргументах, а не потому что пользователь не согласен.
- Если просьба неоднозначна (особенно по дизайну) — уточнить, а не придумывать от себя.

## Приватность (репозиторий открытый)
- Никаких личных данных в файлах, коммитах и патчноутах: почт, паролей, токенов, API-ключей, локальных путей, ссылок на переписку с ИИ.
- Коммиты: автор `nnn <controloptioncommand@users.noreply.github.com>`, коммиттер `Claude <noreply@anthropic.com>`. Строку `Claude-Session: …` в сообщения коммитов **не добавлять**.
  ```bash
  GIT_COMMITTER_NAME=Claude GIT_COMMITTER_EMAIL=noreply@anthropic.com \
    git -c user.name="nnn" -c user.email="controloptioncommand@users.noreply.github.com" commit -m "…"
  ```
- Перед каждым push: `bash scripts/privacy-check.sh` (CI тоже его запускает и остановит сборку при утечке).
- Не делать force push и не переписывать опубликованную историю без явной просьбы.
- Публично оставлены намеренно: ник «nnn» и ссылка на Telegram автора на странице About.

## Пасхалки — не раскрывать
- В README, патчноутах и любой другой публичной документации **не упоминать** эффект «Туман» на логотипе About, сову, игру и скрытые ссылки на странице About (версия → GitHub, «made by nnn» → Telegram). Это пасхалки, их находят сами.
- Скриншоты игры и эффекта «Туман» в документацию не добавлять.

## Версии и патчноуты
- Текущая версия — в файле `VERSION` (схема: v1, v1.01 … v2, v2.01 …). На странице About она же в `Source/UI/About.h` (`AboutInfo::version`, `AboutInfo::date`).
- **У каждой версии обязателен патчноут** `docs/releases/<версия>.md` — без него CI не соберёт релиз.
- Новая версия: поменять `VERSION` и `AboutInfo` → добавить патчноут → коммит в `main`. CI сам соберёт, проверит (pluginval, auval, тест) и опубликует релиз с архивом `Eido-macOS.zip`.
- Пока `VERSION` не меняется, новые сборки перезаписывают текущий релиз.
- Ссылка на последнюю сборку: https://github.com/controloptioncommand/Eido-vst/releases/latest/download/Eido-macOS.zip
- После публикации дать пользователю эту ссылку и команду установки:
  ```bash
  cd ~/Downloads && rm -rf Eido-macOS Eido-macOS.zip && gh release download -R controloptioncommand/Eido-vst -p Eido-macOS.zip && unzip -q Eido-macOS.zip -d Eido-macOS && bash Eido-macOS/install-mac.sh
  ```

## Сборка и проверка
```bash
cmake -B build -DEIDO_BUILD_TESTS=ON && cmake --build build --target EidoTest
./build/EidoTest_artefacts/Release/EidoTest ./render   # на Linux — через xvfb-run -a
```
Тест: звук проходит бит-в-бит, LUFS по эталонным синусам (44.1 / 48 / 96 кГц, ±0.1), стробирование и сброс Integrated, LRA по EBU Tech 3342 (10 ± 1 LU), высота тона 30 Гц…4853 Гц (синус и пила, ≤ 3 цента) и названия нот, показания модулей (пик спектра, корреляция ±1, 0 VU = −18 dBFS RMS, пик сэмпла, тюнер, окно осциллографа, Mid / Side), перестановка и перетаскивание модулей, сохранение состояния, About («Туман», игра, возврат размера полосы), скриншоты обеих раскладок в обеих темах. Перед коммитом тест должен печатать `ALL OK`. После изменений интерфейса — посмотреть скриншоты.

## Решения пользователя по дизайну (не менять)
- Логотип Eido (тулбар и About) в фокусе **без обводки** — текст просто становится оранжевым (Signal).

## Что нельзя ломать
- Звук не меняется: плагин только копирует сигнал в интерфейс (FIFO) и считает громкость на аудиопотоке без выделений памяти.
- Громкость — BS.1770-4 / EBU R128: коэффициенты K-фильтра пересчитываются под частоту дискретизации; стробирование −70 LUFS и −10 LU.
- Новые типы модулей добавлять только в конец `ModuleType`: в проекте хранятся ключи модулей (`moduleKey`), их не переименовывать.
- Состояние интерфейса (раскладка, 8 слотов, размеры окна для Strip и Grid, настройки модулей — `tilt`, `a4`) хранится в проекте (`EidoProcessor::uiState`). Тема — в общих настройках (`~/Library/Application Support/Eido`).

## Структура
- `Source/PluginProcessor` — сквозной звук, FIFO для интерфейса, громкость
- `Source/Analysis/` — `Loudness` (BS.1770, LRA), `Pitch` (YIN для тюнера и осциллографа), `SignalBuffer` (кольцо сигнала на стороне интерфейса)
- `Source/PluginEditor` — тулбар, раскладки Strip / Grid, меню модулей, About
- `Source/UI/` — `LookAndFeel` (темы, шрифты Medeo, пиксельный логотип), `Module` (база модуля и стрелки ‹ ▲ ›), `WaveModules` (Scope, Waves, Spectrogram), `SpectrumModule` (Spectrum, Mid / Side), `MeterModules` (Loudness, VU, Stereo), `PitchModules` (Tuner, Oscilloscope), `HistoryModules` (Loudness History, Peak History), `About` (страница About, «Туман», игра), `Widgets`
- `Tests/EidoTest.cpp` — офлайн-тест; `scripts/` — установка и проверка приватности; `docs/` — руководство (`MANUAL.md`), дизайн, картинки README, патчноуты
- При изменении модулей или управления — обновлять `docs/MANUAL.md` (пасхалки там тоже не упоминать).
