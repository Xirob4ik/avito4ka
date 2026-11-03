# Тестирование

## Что есть

| Набор | Где | Что проверяет |
|---|---|---|
| `ClipperTests --category Clipline-DSP` | `Tests/ClippingFunctionTests.cpp` | Передаточные функции (симметрия, наклон, ограниченность, точные формулы), ядра клиппера и LINK, lock-free измерители, линия задержки |
| `Clipline-Engine` | `Tests/EngineTests.cpp` | Целочисленная задержка и выравнивание dry/wet, порядок усиления, MIX, моно/стерео/LINK, DC/тишина/NaN/экстремальные уровни, отсутствие щелчков при любых переключениях, независимость от размера блока, алиасинг, измерители, 10 минут случайной автоматизации, CPU |
| `Clipline-Plugin` | `Tests/PluginTests.cpp` | 8 параметров (ID, диапазоны, значения по умолчанию, текст), состояние DAW (круговой тест, мусор, значения вне диапазона), шины, bypass с сохранением задержки, сообщение задержки хосту, пресеты (таблица, применение, пользовательские файлы, защита от мусора), CLIP как настоящий потолок во всех режимах |
| `Clipline-UI` | `Tests/UiTests.cpp` | Настоящий редактор, управляемый синтезированными событиями мыши: перетаскивание (диапазон, точный режим, ровно один жест для хоста), двойной клик, колесо, ввод текста, сегменты, LINK/CEILING, перетаскивание линии порога, измерители от реального движка, масштабирование окна |
| `ClipperHostCheck <путь к .vst3>` | `Tests/HostCheck/HostCheck.cpp` | Мини-хост: загружает **собранный .vst3 через интерфейсы VST3**, как это делает DAW. Сканирование, имя/категория, 8 параметров видны хосту, звук ограничен потолком, смена задержки доходит до хоста (`restartComponent`), сохранение/восстановление состояния, открытие и закрытие окна редактора |
| `ClipperTests --snapshot out.png` | `Tests/SnapshotTool.cpp` | Рендер окна плагина в PNG на синтетическом материале (для визуальной проверки) |

## Что реально запускалось (3 ноября 2026)

### Настоящие Windows и macOS (GitHub Actions, ветка `clipline`, прогон 41659229207)

| | Windows Server (windows-latest), MSVC | macOS 14 (Apple Silicon), Apple clang, universal arm64 + x86_64 |
|---|---|---|
| Юнит-тесты `ClipperTests` | 79 групп, 4111 проверок, 0 ошибок | 79 групп, 4111 проверок, 0 ошибок |
| `ClipperHostCheck` (VST3 из папки сборки и после установки) | пройдено, оба раза | пройдено, оба раза |
| Установщик | `Clipline-0.1.0-Win64-Setup.exe /S` → плагин в `C:\Program Files\Common Files\VST3` | `sudo installer -pkg …` → VST3 и AU в `/Library/Audio/Plug-Ins` |
| pluginval, строгость 10 | VST3: SUCCESS | VST3: SUCCESS, AU: SUCCESS |
| `auval` (валидатор Apple для AU) | — | AU VALIDATION SUCCEEDED |
| Linux (ubuntu-24.04) | все тесты | |

Установщики, которые получил пользователь, — это ровно эти файлы из прогона: они переданы через лог сборки в base64, и SHA-256 совпала с суммой, посчитанной на сервере сборки.

### Среда разработки (Linux)

1. **Нативная Linux-сборка:** 79 групп, 4111 проверок, 0 ошибок.
2. **Кросс-сборка Windows (llvm-mingw / clang 20) под Wine 10.10:** все тесты, мини-хост, установка/удаление. Это запасной путь сборки. Wine не воспроизводит часть отличий Windows: так была пропущена ошибка с comctl32 (см. [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md)).

## Что НЕ проверялось

* Загрузка в настоящих DAW: FL Studio, Ableton Live, REAPER, Logic Pro. pluginval и `auval` загружают плагин так же, как хост, но это не DAW.
* Отрисовка Direct2D на реальной видеокарте: на серверах GitHub нет GPU, и pluginval открывает окно только для проверки, не сравнивая картинку. На случай проблем есть меню ⋯ → Software rendering.
* Steinberg VST3 Validator (из VST3 SDK) не запускался; pluginval покрывает большую часть тех же проверок.

## Как запустить самому

Windows (после `scripts\build-windows.ps1` или ручной сборки):

```powershell
build\Tests\ClipperTests_artefacts\Release\ClipperTests.exe
build\Tests\ClipperHostCheck_artefacts\Release\ClipperHostCheck.exe "C:\Program Files\Common Files\VST3\Clipline.vst3"
```

Также можно прогнать [pluginval](https://github.com/Tracktion/pluginval) (`pluginval --strictness-level 10 --validate "C:\Program Files\Common Files\VST3\Clipline.vst3"`) и VST3 validator из VST3 SDK.
