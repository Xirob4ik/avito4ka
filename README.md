# Clipline

VST3-клиппер для Windows x64 (C++17, JUCE 8, CMake). Четыре алгоритма клиппинга, настоящий оверсэмплинг 1×/2×/4×/8× с точной компенсацией задержки, MIX с выровненным по времени сухим сигналом, «честный» потолок (CEILING), осциллограф с перетаскиваемым порогом и измерители, показывающие реальный сигнал движка.

Рабочее название **Clipline**: как поменять имя, логотип и коды плагина, описано в [docs/BRANDING.md](docs/BRANDING.md).

| Документ | Что внутри |
|---|---|
| [docs/INSTALL_FL_STUDIO.md](docs/INSTALL_FL_STUDIO.md) | Windows: установка и подключение в FL Studio (а также Ableton Live и REAPER) |
| [docs/INSTALL_MAC.md](docs/INSTALL_MAC.md) | macOS: установка `.pkg`, подключение в FL Studio, Logic Pro, Ableton Live, удаление |
| [docs/FEATURES.md](docs/FEATURES.md) | Полный список функций и элементов интерфейса |
| [docs/ALGORITHMS.md](docs/ALGORITHMS.md) | Формулы, схема сигнала, оверсэмплинг, CEILING, измерения |
| [docs/TESTING.md](docs/TESTING.md) | Какие тесты есть, что и где реально запускалось, результаты |
| [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md) | Известные ограничения и что не проверено |
| [docs/BRANDING.md](docs/BRANDING.md) | Переименование продукта |

---

## Установка (готовый установщик)

1. Запустите `Clipline-0.1.0-Win64-Setup.exe` и подтвердите запрос прав администратора: установщик пишет в `C:\Program Files\Common Files\VST3`.
2. Нажмите **Установить**. Плагин окажется в `C:\Program Files\Common Files\VST3\Clipline.vst3`.
3. Пересканируйте плагины в DAW. Подробно для FL Studio: [docs/INSTALL_FL_STUDIO.md](docs/INSTALL_FL_STUDIO.md).

Удаление: «Параметры → Приложения → Clipline (VST3)». Ваши пресеты в `%APPDATA%\Clipline\Presets` при удалении сохраняются.
Тихая установка для скриптов: `Clipline-0.1.0-Win64-Setup.exe /S`, тихое удаление: `"C:\Program Files\Common Files\Clipline\Uninstall.exe" /uninstall /S`.

Ручная установка без установщика: скопируйте папку `Clipline.vst3` целиком в `C:\Program Files\Common Files\VST3\`.

Требования: Windows 10 или 11, 64-битная DAW с поддержкой VST3. Visual C++ Redistributable не нужен.

---

## Сборка из исходников

### Вариант A. Windows + Visual Studio 2022 (основной)

Нужны Visual Studio 2022 с компонентом «Разработка классических приложений на C++», CMake ≥ 3.22 (входит в VS) и Git.

```powershell
git clone <ваш репозиторий> Clipline
cd Clipline
.\scripts\build-windows.ps1            # Release: плагин, тесты, установщик; затем запускает тесты
```

Или вручную:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
build\Tests\ClipperTests_artefacts\Release\ClipperTests.exe
build\Tests\ClipperHostCheck_artefacts\Release\ClipperHostCheck.exe build\Clipline_artefacts\Release\VST3\Clipline.vst3
```

Результаты:

* `build\Clipline_artefacts\Release\VST3\Clipline.vst3`: плагин;
* `build\Installer\Windows\Release\Clipline-0.1.0-Win64-Setup.exe`: установщик.

JUCE 8.0.8 скачивается автоматически (FetchContent), и к нему применяется небольшой патч `cmake/patches/juce-8.0.8-clipline.patch`. Он исправляет зависание при закрытии окна плагина, когда DXGI не может ждать кадровую развёртку (см. [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md)). Чтобы использовать свою копию JUCE: `-DCLIPLINE_JUCE_DIR=C:\path\to\JUCE`, патч к ней примените сами (`git apply`).

Флаг `-DCLIPLINE_COPY_AFTER_BUILD=ON` копирует плагин в системную папку VST3 после сборки (нужны права администратора).

### Вариант B. Кросс-сборка Windows-версии на Linux (так собран приложенный установщик)

```bash
./scripts/build-windows-cross.sh          # скачает llvm-mingw, соберёт плагин, тесты и установщик
```

Используются [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (clang 20) и `cmake/toolchains/llvm-mingw-x86_64.cmake`. Официально JUCE 8 MinGW не поддерживает; патч снимает этот запрет, а недостающие в mingw-w64 константы Windows SDK подставляет `cmake/mingw-compat/`. Тесты получившихся `.exe` можно запустить под Wine (скрипт делает это, если Wine найден).

### macOS (VST3 + Audio Unit, универсальный arm64 + x86_64)

Нужны Xcode (или Command Line Tools), CMake и Ninja.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
build/Tests/ClipperTests_artefacts/Release/ClipperTests
Installer/macOS/build-pkg.sh build Release          # -> build/Clipline-0.1.0-macOS.pkg
```

То же самое выполняет задание `macos` в `.github/workflows/build.yml` на облачном Mac GitHub, плюс `auval` и pluginval после установки `.pkg`.

### Linux (разработка и тесты)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ClipperTests
xvfb-run -a build/Tests/ClipperTests_artefacts/Release/ClipperTests        # все тесты
build/Tests/ClipperTests_artefacts/Release/ClipperTests --category Clipline-DSP -v
xvfb-run -a build/Tests/ClipperTests_artefacts/Release/ClipperTests --snapshot ui.png --preset "Punchy Drums"
```

Для GUI-части на Linux нужны заголовки X11/freetype (`libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libasound2-dev`).

---

## Структура проекта

```
Source/
  PluginProcessor.*      AudioProcessor: параметры, состояние, задержка, bypass
  PluginEditor.*         окно плагина (сетка 900×560, масштабирование)
  Parameters.*, ParameterIds.h   параметры: стабильные ID, диапазоны, форматирование
  Branding.h             имя продукта (одно место)
  DSP/                   движок без GUI: ClipperEngine, ClippingFunctions, OversamplingProcessor,
                         DelayLine, Metering (lock-free), WaveformTap, NumericSafety
  Presets/               заводские пресеты и PresetManager (пользовательские *.clpreset)
  UI/                    тема, шрифты, LookAndFeel, ручки, сегментные переключатели, измерители,
                         осциллограф, панель пресетов, настройки интерфейса
Tests/                   юнит-тесты (DSP, движок, плагин, интерфейс), снимки интерфейса,
                         HostCheck: мини-хост, загружающий собранный .vst3 через интерфейсы VST3
Installer/Windows/       установщик (Win32, плагин встроен ресурсом)
cmake/                   поиск/патч JUCE, тулчейн llvm-mingw, compat-заголовки
Resources/Fonts/         Geist / Geist Mono (SIL OFL 1.1)
scripts/                 сборка под Windows (PowerShell) и кросс-сборка (bash)
.github/workflows/       CI: Windows (MSVC) + Linux, артефакты .vst3 и установщик
```

## Лицензии

* JUCE используется на условиях AGPLv3 или коммерческой лицензии JUCE. Для закрытого коммерческого распространения нужна лицензия JUCE.
* Шрифты Geist и Geist Mono распространяются по SIL Open Font License 1.1 (`Resources/Fonts/OFL-Geist.txt`).
* VST® является товарным знаком Steinberg Media Technologies GmbH.
