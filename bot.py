import telebot
from telebot import apihelper
import requests
import threading
import concurrent.futures
import time
import os

# ==========================================
# НАСТРОЙКИ БОТА
# ==========================================
# Вставьте сюда токен вашего бота от @BotFather (или задайте через переменную окружения)
TOKEN = os.getenv("TELEGRAM_BOT_TOKEN", "8756512607:AAHhfh9RuNC6Ew2wCZ7dGpnbYOjSfxoRzDY")

# Настройки прокси для Telegram (для работы в РФ)
USE_PROXY = False  # Измените на False, если ставите на зарубежный сервер
PROXY_URL = "http://YgD9YD:e7DnFW@161.0.25.220:8000"

if USE_PROXY:
    apihelper.proxy = {'http': PROXY_URL, 'https': PROXY_URL}
    print("🌐 Включен прокси для Telegram.")

bot = telebot.TeleBot(TOKEN)

# Состояния пользователей (чтобы бот мог работать с несколькими людьми одновременно)
# Структура: { chat_id: {"is_parsing": False, "target_artists": [], "current_id": 0, "step": "IDLE"} }
user_states = {}

def get_state(chat_id):
    if chat_id not in user_states:
        user_states[chat_id] = {"is_parsing": False, "target_artists": [], "current_id": 0, "step": "IDLE"}
    return user_states[chat_id]

# ==========================================
# ОБРАБОТЧИКИ КОМАНД
# ==========================================
@bot.message_handler(commands=['start'])
def send_welcome(message):
    chat_id = message.chat.id
    get_state(chat_id)
    text = (
        "👋 Привет! Я бот-парсер альбомов Slacker.\n\n"
        "Доступные команды:\n"
        "/artists - Задать список нужных артистов\n"
        "/parse - Начать парсинг\n"
        "/stop - Остановить парсинг\n"
        "/status - Текущий статус"
    )
    bot.send_message(chat_id, text)

@bot.message_handler(commands=['artists'])
def ask_artists(message):
    chat_id = message.chat.id
    state = get_state(chat_id)
    state["step"] = "WAITING_FOR_ARTISTS"
    bot.send_message(chat_id, "Отправьте мне список артистов, которых нужно искать.\nКаждого артиста пишите с новой строки:")

@bot.message_handler(commands=['parse'])
def ask_start_id(message):
    chat_id = message.chat.id
    state = get_state(chat_id)
    
    if not state["target_artists"]:
        bot.send_message(chat_id, "❌ Сначала задайте список артистов с помощью команды /artists")
        return
        
    if state["is_parsing"]:
        bot.send_message(chat_id, "⚠️ Парсинг уже запущен! Остановите его командой /stop")
        return
        
    state["step"] = "WAITING_FOR_START_ID"
    bot.send_message(chat_id, "Отправьте ID альбома, с которого нужно начать парсинг (например: 25376473):")

@bot.message_handler(commands=['stop'])
def stop_parsing(message):
    chat_id = message.chat.id
    state = get_state(chat_id)
    
    if not state["is_parsing"]:
        bot.send_message(chat_id, "Парсинг и так не запущен.")
        return
        
    state["is_parsing"] = False
    bot.send_message(chat_id, "🛑 Парсинг останавливается... (дожидаемся завершения текущих запросов)")

@bot.message_handler(commands=['status'])
def show_status(message):
    chat_id = message.chat.id
    state = get_state(chat_id)
    
    status_text = "В процессе 🟢" if state["is_parsing"] else "Остановлен 🔴"
    text = (
        f"*Статус парсинга:* {status_text}\n"
        f"*Текущий ID:* {state['current_id']}\n"
        f"*Артистов в поиске:* {len(state['target_artists'])}"
    )
    bot.send_message(chat_id, text, parse_mode="Markdown")

@bot.message_handler(func=lambda message: True)
def handle_text(message):
    chat_id = message.chat.id
    state = get_state(chat_id)
    
    if state["step"] == "WAITING_FOR_ARTISTS":
        # Разбиваем по строкам и убираем пустые
        artists = [a.strip() for a in message.text.split('\n') if a.strip()]
        state["target_artists"] = artists
        state["step"] = "IDLE"
        bot.send_message(chat_id, f"✅ Список артистов сохранен ({len(artists)} шт.).\nТеперь вы можете начать парсинг командой /parse")
        
    elif state["step"] == "WAITING_FOR_START_ID":
        try:
            start_id = int(message.text.strip())
        except ValueError:
            bot.send_message(chat_id, "❌ Пожалуйста, отправьте корректное число (ID).")
            return
            
        state["step"] = "IDLE"
        state["current_id"] = start_id
        state["is_parsing"] = True
        
        bot.send_message(chat_id, f"🚀 Парсинг запущен с ID: {start_id}\n🎯 Ищем артистов ({len(state['target_artists'])} шт.)")
        
        # Запускаем парсинг в отдельном потоке, чтобы не блокировать бота
        threading.Thread(target=run_parser, args=(chat_id,)).start()

# ==========================================
# ЛОГИКА ПАРСИНГА
# ==========================================
def check_album(album_id, target_artists, chat_id):
    """Проверяет один альбом."""
    url = f"https://slacker.liveone.com/mixer/catalog/SLACKER/US/651625258/albums/{album_id}/full?_rc=ae%3D1%26ut%3D0"
    headers = {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Accept": "application/json"
    }
    
    try:
        response = requests.get(url, headers=headers, timeout=10)
        if response.status_code == 200:
            data = response.json()
            artist_name = data.get("artistName", "Неизвестный артист")
            album_name = data.get("albumName", "Неизвестный альбом")
            release_year = data.get("releaseYear", "")
            release_month = data.get("releaseMonth", "")
            release_day = data.get("releaseDay", "")
            img_path = data.get("imgPath", "")
            
            release_date = f"{release_day}.{release_month}.{release_year}"
            
            if artist_name in target_artists:
                msg = (
                    f"🎉 *ЦЕЛЬ НАЙДЕНА*\n\n"
                    f"👤 Артист: {artist_name}\n"
                    f"💿 Альбом: {album_name}\n"
                    f"📅 Дата выхода: {release_date}\n"
                    f"🖼 Обложка: {img_path}\n"
                    f"🆔 ID альбома: {album_id}"
                )
                bot.send_message(chat_id, msg, parse_mode="Markdown")
                print(f"🎉 [ЦЕЛЬ НАЙДЕНА] ID: {album_id} | {artist_name} - {album_name}")
            else:
                print(f"✅ [ОК] ID: {album_id} | {artist_name} - {album_name}")
        elif response.status_code == 404:
            print(f"❌ [404] ID: {album_id} | Нет данных")
        else:
            print(f"⚠️ [{response.status_code}] ID: {album_id} | Ошибка сервера")
    except Exception as e:
        error_msg = str(e).split(":")[-1].strip()
        print(f"🌐 [ОШИБКА СЕТИ] ID: {album_id} | {error_msg}")

def run_parser(chat_id):
    """Основной цикл парсинга (работает в фоне)."""
    state = get_state(chat_id)
    batch_size = 20 # Количество одновременных потоков
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=batch_size) as executor:
        while state["is_parsing"]:
            current_id = state["current_id"]
            album_ids = range(current_id, current_id + batch_size)
            
            # Запускаем пачку ID в пуле потоков
            futures = [executor.submit(check_album, aid, state["target_artists"], chat_id) for aid in album_ids]
            concurrent.futures.wait(futures)
            
            # Проверяем, не остановил ли пользователь парсинг во время выполнения пачки
            if not state["is_parsing"]:
                break
                
            state["current_id"] += batch_size
            time.sleep(0.5) # Небольшая пауза, чтобы не спамить API

    bot.send_message(chat_id, "🛑 Процесс парсинга полностью остановлен.")

if __name__ == "__main__":
    print("🤖 Бот запущен! Нажмите Ctrl+C для остановки.")
    # Запускаем бота
    bot.infinity_polling()
