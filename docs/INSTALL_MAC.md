# Установка Clipline на Mac

Подходит для любого Mac: на Apple Silicon (M1, M2, M3, M4) и на Intel. Нужна macOS 10.13 High Sierra или новее.
Устанавливаются два формата:

| Формат | Куда | Где работает |
|---|---|---|
| **VST3** | `/Library/Audio/Plug-Ins/VST3/Clipline.vst3` | FL Studio, Ableton Live, REAPER, Cubase, Studio One, Bitwig |
| **Audio Unit (AU)** | `/Library/Audio/Plug-Ins/Components/Clipline.component` | Logic Pro, GarageBand, FL Studio, Ableton Live, MainStage |

Терминал не нужен.

## 1. Установка

1. Закройте все DAW (FL Studio, Logic и т. д.).
2. Найдите файл **Clipline-0.1.0-macOS.pkg** в «Загрузках».
3. **Первый запуск:** установщик не подписан сертификатом Apple, поэтому обычный двойной клик macOS заблокирует («не удаётся проверить разработчика»). Откройте так:
   * **macOS 15 Sequoia и новее:** дважды кликните по файлу → в окне с предупреждением нажмите **Готово** → откройте **Системные настройки → Конфиденциальность и безопасность** → прокрутите вниз до сообщения «Clipline-0.1.0-macOS.pkg заблокирован…» → **Всё равно открыть** → введите пароль Mac.
   * **macOS 14 Sonoma и старше:** кликните по файлу **правой кнопкой** (или с зажатой Control) → **Открыть** → в окне ещё раз **Открыть**.
4. В установщике: **Продолжить → Установить**, введите пароль от Mac. Кнопкой «Настроить» можно выбрать только VST3 или только AU; по умолчанию ставятся оба.
5. Увидите «Установка прошла успешно» — готово.

Плагины, установленные через `.pkg`, macOS сама не блокирует: повторных предупреждений при загрузке в DAW не будет.

## 2. Подключение в DAW

**FL Studio (Mac)**
1. **Options → Manage plugins**.
2. Нажмите **Find more plugins** (при первой настройке **Start scan**; включите «Verify plugins»).
3. Clipline появится в двух видах: VST3 и AU. Хватит одного; удобнее VST3, тогда проекты одинаково открываются на Mac и на Windows.
4. Микшер → слот эффекта → **Select → Clipline**.

**Logic Pro / GarageBand**
Ничего сканировать не нужно: при запуске Logic сам проверяет новые Audio Units. Слот Audio FX → **Audio Units → Clipline Audio → Clipline**.
Если плагина нет в списке: **Logic Pro → Настройки → Менеджер плагинов** → найдите Clipline → **Сбросить и пересканировать выбранное**.

**Ableton Live**
**Settings → Plug-Ins** → включите «Use Audio Units» и/или «Use VST3 Plug-In System Folders» → **Rescan**.

## 3. Пресеты

Свои пресеты Clipline сохраняет в
`~/Library/Application Support/Clipline/Presets`.
Чтобы открыть эту папку: меню **⋯** в правом верхнем углу плагина → **Show preset folder**.
Файлы пресетов (`.clpreset`) совместимы с Windows-версией: их можно копировать между компьютерами.

## 4. Удаление

1. Закройте DAW.
2. В Finder: меню **Переход → Переход к папке…** (⇧⌘G), вставьте `/Library/Audio/Plug-Ins/VST3` → перенесите **Clipline.vst3** в Корзину (попросит пароль).
3. Так же: `/Library/Audio/Plug-Ins/Components` → **Clipline.component** в Корзину.
4. Пресеты (если не нужны): `~/Library/Application Support/Clipline`.

## 5. Если что-то не так

| Симптом | Что сделать |
|---|---|
| «Не удаётся открыть, так как разработчик не подтверждён» | Это про сам `.pkg`, см. шаг 3 установки. |
| FL Studio не видит плагин | Manage plugins → проверьте, что в путях есть `/Library/Audio/Plug-Ins/VST3`, и нажмите Find more plugins ещё раз. |
| Logic пишет, что Audio Unit не прошёл проверку | Пришлите снимок окна Менеджера плагинов: там видна причина. |
| Плагин раньше ставился вручную в `~/Library/Audio/Plug-Ins` (папка пользователя) | Удалите оттуда старую копию, чтобы DAW не путала версии. |
