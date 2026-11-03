# Переименование продукта

«Clipline» — рабочее имя. Чтобы выпустить плагин под другим именем, измените:

| Где | Что |
|---|---|
| `CMakeLists.txt` | `CLIPLINE_PRODUCT_NAME`, `CLIPLINE_COMPANY_NAME`, `CLIPLINE_BUNDLE_ID`, `CLIPLINE_MANUFACTURER_CODE` (4 символа, минимум одна заглавная), `CLIPLINE_PLUGIN_CODE` (4 символа, уникален для продукта); версия в `project(... VERSION x.y.z)` |
| `Source/Branding.h` | `kProductName`, `kWordmark` (надпись в окне), `kCompanyName`, `kUserDataFolder` (папка пресетов в `%APPDATA%`), `kPresetExtension` |
| `Source/PluginProcessor.h` | `getName()` |
| `Source/UI/Logo.h` | Глиф логотипа (сейчас: синусоида со срезанными вершинами) |
| `Installer/Windows/Installer.cpp` | `kProduct`, `kPublisher`, `kUninstKey`, имена папок (`Clipline.vst3`, `Common Files\Clipline`) |
| `Installer/Windows/clipline.ico` | Иконка установщика |
| `Installer/Windows/README.txt` | Текст, который устанавливается рядом с деинсталлятором |

**Важно.** Смена `PLUGIN_CODE`, `MANUFACTURER_CODE` или имени делает плагин для DAW **другим** плагином: старые проекты его не найдут. Выберите окончательные значения до первого публичного релиза и больше их не меняйте. То же касается ID параметров (`Source/ParameterIds.h`).
