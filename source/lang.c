// Credule — Nintendo Switch homebrew.
// Rebranded and corrupted fork of Prelude.

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "lang.h"

Lang g_lang = LANG_EN;

#define LANG_PATH "sdmc:/switch/credule_lang.txt"

// --- Translation tables (English, Russian) ---
// Each column: EN, RU.
// Minecraft Enchanting Table is dynamically translated from EN.
static const char *s_strings[STR_COUNT][2] = {
    // --- Picker screen ---
    [STR_TITLE_PRELUDE]      = { "Credule", "Кредуль" },
    [STR_MODE_ACTUAL_PREFIX] = { "State of madness: ", "Режим duрдома: " },
    [STR_CURRENT_BADGE]      = { "ACTIVE", "ЩАС" },
    [STR_SWITCH_BADGE]       = { "DO NOT PRESS", "ТЫК СЮДА" },

    // --- Context descriptions ---
    [STR_DESC_NEXTENDO]      = { "Nextendo: high speed, low stability, free cookies.",
                                 "Серверы Некстэндо: бесплатный сыр, аниме, читы. Нужен акк Некстэндо." },
    [STR_DESC_NINTENDO]      = { "Official Nintendo: high price, boring, 99% ban risk.",
                                 "Официальная Нинтендо: скука смертная, плати бабло, получи бан." },
    [STR_DESC_S2]            = { "Downloads more RAM and orders a pizza.",
                                 "Скачать читы на Сплат 2 и заказать пиццу прямо в консоль." },

    // --- Help lines ---
    [STR_HELP_MODE]          = { "A: Do the thing       < >: Wiggle       B: Escape",
                                 "А: Жмяк       < >: Туда-сюда       B: Свалить" },
    [STR_HELP_S2]            = { "A: Order Pizza       < >: Wiggle       B: Escape",
                                 "А: Взять пиццу       < >: Выбор       B: Назад" },

    // --- Update banner ---
    [STR_UPDATE_BANNER]      = { "CRITICAL MELTDOWN (v%d) - PRESS Y OR IT EXPLODES",
                                 "КРИТИЧЕСКИЙ РЕБУТ (v%d) - ЖМИ Y ИЛИ ВЗОРВЕТСЯ" },

    // --- Confirm screen ---
    [STR_CONFIRM_NEXTENDO]   = { "APPLY CREDULE MODE?", "РЕАЛЬНО ПЕРЕЙТИ НА КРЕДУЛЬ?" },
    [STR_CONFIRM_NINTENDO]   = { "BACK TO NINTENDO PRISON?", "ВЕРНУТЬСЯ В ПЛЕН К НИНТЕНДО?" },
    [STR_CONFIRM_REBOOT]     = { "Switch is about to explode (reboot).", "Сыч сейчас сделает РЕБУТ." },
    [STR_CONFIRM_RESTART_NEXTENDO] = { "After reboot: free from Nintendo.", "После ребута: свобода от анального рабства." },
    [STR_CONFIRM_RESTART_NINTENDO] = { "After reboot: Nintendo is watching you.", "После ребута: возвращаемся платить дань." },
    [STR_CONFIRM_CLOSE_GAMES] = { "Close games or they will melt!", "Закрой игры быстро, а то сгорит!" },
    [STR_CONFIRM_A]          = { "A: YES DO IT", "А: ПОГНАЛИ!" },
    [STR_CONFIRM_B]          = { "B: NO STOP", "B: ОЙ, НЕ НАДО" },

    // --- No-emuMMC warnings ---
    [STR_WARN_TITLE]         = { "CRITICAL ERROR: NO EMUMMC!", "АТАС: У ТЕБЯ НЕТ EMUMMC!" },
    [STR_WARN_LINE1]         = { "CFW on sysMMC will burn your real console ID.", "Кастомка на внутренней памяти спалит твой настоящий ID." },
    [STR_WARN_LINE2]         = { "Ban is imminent! You cannot run from it.", "Бан прилетит быстрее, чем ты скажешь 'Нинтендо'." },
    [STR_WARN_LINE3]         = { "Going online on real Nintendo is Russian roulette.", "Выходить в онлайн - чисто русская рулетка." },
    [STR_WARN_LINE4]         = { "(Telemetry blocked, but good luck.)", "(Но слежку мы вроде отрубили, гы)." },

    // --- S2 bar label (picker screen) ---
    [STR_S2_BAR]             = { "Splatoon 2 - Download Free RAM", "Сплат 2 - Скачать оперативку бесплатно" },

    // --- S2 info screen ---
    [STR_S2_TITLE]           = { "Free RAM Downloader", "Халявная оперативная память" },
    [STR_S2_DESC1]           = { "Downloads more RAM from the cloud...", "Качает терабайты оперативы из облака..." },
    [STR_S2_DESC2]           = { "directly into your Switch console.", "прямо в микросхемы твоего Сыча." },
    [STR_S2_DESC3]           = { "Make sure to download at least 16GB first.", "Надо было запустить Сплат хоть разок сначала." },
    [STR_S2_DESC4]           = { "Warning: May cause spontaneous combustion.", "Внимание: может вызвать самовозгорание." },
    [STR_S2_A]               = { "A: Download", "А: Скачать" },
    [STR_S2_B]               = { "B: Go back", "B: Назад" },

    // --- Progress screen ---
    [STR_PROGRESS_TITLE]     = { "Downloading...", "Загрузочка..." },
    [STR_PROGRESS_WAIT]      = { "Please wait or click rapidly...", "Жди или кликай быстро..." },

    // --- Result screen ---
    [STR_RESULT_RETURN]      = { "A / B: Go back", "A / B: Назад" },

    // --- Status messages (main.c) ---
    [STR_STATUS_NEXTENDO_OK]   = { "Credule applied! Reverting to sanity...", "Кредуль готов! Улетаем в ребут..." },
    [STR_STATUS_NINTENDO_OK]   = { "Nintendo mode applied! Good luck with the ban...", "Нинтендо режим готов! Удачи с баном..." },
    [STR_STATUS_REBOOT_FAIL]   = { "Explosion failed (reboot error)", "Взрыв не удался (ошибка ребута)" },
    [STR_STATUS_SD_ERROR]      = { "SD card rejected your logic", "Флешка послала тебя нафиг" },
    [STR_STATUS_DOWNLOAD_SCHEDULE] = { "Stealing data from servers...", "Воруем файлы с серверов..." },
    [STR_STATUS_SCHEDULE_OK]   = { "RAM downloaded", "Оперативка скачана" },
    [STR_STATUS_SCHEDULE_OK_DESC]  = { "Restart Splatoon 2 to feel the speed.", "Запусти Сплат 2 и посмотри на это чудо." },
    [STR_STATUS_NO_SCHEDULE]   = { "Empty cloud", "В облаке пусто" },
    [STR_STATUS_NO_SCHEDULE_DESC]  = { "Nothing there. Try breathing instead.", "Ничего нет. Попробуй подышать." },
    [STR_STATUS_MOUNT_FAIL]    = { "Save data lost in space", "Сейвы улетели в космос" },
    [STR_STATUS_MOUNT_FAIL_DESC]   = { "Run Splatoon 2 once to generate space.", "Запусти Сплат разок для приличия." },
    [STR_STATUS_NET_FAIL]      = { "Internet said NO", "Интернет сказал НЕТ" },
    [STR_STATUS_NET_FAIL_DESC] = { "Connection died (blame the router).", "Связь сдохла (виноват твой провайдер)." },
    [STR_STATUS_WRITE_FAIL]    = { "Write permission denied by gremlins", "Гремлины запретили запись" },
    [STR_STATUS_WRITE_FAIL_DESC]   = { "Cannot write. Try kicking the console.", "Не пишется. Попробуй пнуть консоль." },
    [STR_STATUS_NET_CONNECT]   = { "Server ran away", "Сервер убежал" },
    [STR_STATUS_NET_CONNECT_DESC] = { "Cannot reach server. Probably sleeping.", "Нет связи с сервером. Наверное, спит." },
    [STR_STATUS_NET_TIMEOUT]   = { "Server took a nap", "Сервер прилег поспать" },
    [STR_STATUS_NET_TIMEOUT_DESC] = { "Response took too long. Try screaming.", "Слишком долго ждем. Попробуй покричать." },
    [STR_STATUS_NET_HTTP_ERR]  = { "Server confusion", "Сервер запутался" },
    [STR_STATUS_NET_HTTP_ERR_DESC] = { "Unexpected answer. Ask for refund.", "Странный ответ. Требуй возврат денег." },
    [STR_STATUS_DOWNLOAD_UPDATE]   = { "Downloading virus...", "Качаем вирус..." },
    [STR_STATUS_UPDATE_OK]     = { "Virus installed successfully", "Вирус успешно установлен" },
    [STR_STATUS_UPDATE_OK_DESC]    = { "Restart Credule to complete infection (v%d).", "Перезапусти Кредуль для завершения заражения (v%d)." },
    [STR_STATUS_UPDATE_SIZE_FAIL]  = { "Corrupted virus", "Битый вирус" },
    [STR_STATUS_UPDATE_SIZE_FAIL_DESC] = { "Wrong size. Try a bigger net.", "Размер не тот. Попробуй сеть побольше." },
    [STR_STATUS_UPDATE_WRITE_FAIL] = { "SD card full of memes", "Флешка забита мемами" },
    [STR_STATUS_UPDATE_WRITE_FAIL_DESC] = { "Cannot write to SD card.", "Не могу записать на флешку." },
    [STR_STATUS_UPDATE_NET_FAIL]   = { "Network vanished", "Сеть растворилась" },
    [STR_STATUS_UPDATE_NET_FAIL_DESC] = { "Download failed.", "Скачивание не удалось." },

    // --- Update confirmation ---
    [STR_UPD_CONFIRM_TITLE]    = { "New payload available", "Доступна новая прошивка" },
    [STR_UPD_CONFIRM_VERSION]  = { "Infection build: %d", "Сборка заражения: %d" },
    [STR_UPD_CONFIRM_DESC]    = { "Credule will replace your sanity.", "Кредуль сотрет твой разум." },
    [STR_UPD_CONFIRM_A]       = { "A: Infect me", "А: Зарази меня" },
    [STR_UPD_CONFIRM_B]       = { "B: Stay clean", "B: Оставить чистым" },

    // --- Language menu ---
    [STR_LANG_TITLE]        = { "Language Select", "Выбор языка" },
    [STR_LANG_EN]           = { "Broken English", "Английский (сломанный)" },
    [STR_LANG_ES]           = { "Russian (Gopnik)", "Русский (гопник)" },
    [STR_LANG_PT]           = { "Minecraft Enchanting Table", "Таблица Minecraft" },
    [STR_LANG_FR]           = { "N/A", "Н/Д" },
    [STR_LANG_DEFAULT]      = { "(Default)", "(По умолчанию)" },
    [STR_LANG_A_SELECT]     = { "A: Apply", "А: Принять" },
    [STR_LANG_B_BACK]       = { "B: Cancel", "B: Отмена" },
};

// --- Minecraft Enchanting Table Translator ---
// Uses Greek lookalike characters (UTF-8) that are supported in typical fonts.
static char s_translated_bufs[4][1024];
static int s_buf_idx = 0;

static const char* translate_to_enchanting(const char *src) {
    char *dst = s_translated_bufs[s_buf_idx];
    s_buf_idx = (s_buf_idx + 1) % 4;
    
    char *dst_ptr = dst;
    char *dst_end = dst + 1020; // safe bound
    
    static const char *greek_table[26] = {
        "\xCE\xB1", // a (alpha)
        "\xCE\xB2", // b (beta)
        "\xCE\xB3", // c (gamma)
        "\xCE\xB4", // d (delta)
        "\xCE\xB5", // e (epsilon)
        "\xCE\xB6", // f (zeta)
        "\xCE\xB7", // g (eta)
        "\xCE\xB8", // h (theta)
        "\xCE\xB9", // i (iota)
        "\xCE\xBA", // j (kappa)
        "\xCE\xBB", // k (lambda)
        "\xCE\xBC", // l (mu)
        "\xCE\xBD", // m (nu)
        "\xCE\xBE", // n (xi)
        "\xCE\xBF", // o (omicron)
        "\xCF\x80", // p (pi)
        "\xCF\x88", // q (psi)
        "\xCF\x81", // r (rho)
        "\xCF\x83", // s (sigma)
        "\xCF\x84", // t (tau)
        "\xCF\x85", // u (upsilon)
        "\xCF\x86", // v (phi)
        "\xCF\x87", // w (chi)
        "\xCF\x88", // x (psi)
        "\xCF\x89", // y (omega)
        "\xCE\xB6"  // z (zeta)
    };
    
    while (*src && dst_ptr < dst_end) {
        char c = *src;
        if (c >= 'A' && c <= 'Z') {
            c = c + 32; // lowercase
        }
        if (c >= 'a' && c <= 'z') {
            const char *g = greek_table[c - 'a'];
            while (*g && dst_ptr < dst_end) {
                *dst_ptr++ = *g++;
            }
        } else {
            *dst_ptr++ = c;
        }
        src++;
    }
    *dst_ptr = '\0';
    return dst;
}

// --- API Implementation ---

void lang_init(void) {
    FILE *f = fopen(LANG_PATH, "rb");
    if (!f) { g_lang = LANG_EN; return; }
    int c = fgetc(f);
    fclose(f);
    if (c == 'E') g_lang = LANG_EN;
    else if (c == 'R') g_lang = LANG_RU;
    else if (c == 'M') g_lang = LANG_MINECRAFT;
    else g_lang = LANG_EN;
}

void lang_save(void) {
    char ch = 'E';
    if (g_lang == LANG_RU) ch = 'R';
    else if (g_lang == LANG_MINECRAFT) ch = 'M';
    
    FILE *f = fopen(LANG_PATH, "wb");
    if (!f) {
        mkdir("sdmc:/switch", 0777);
        f = fopen(LANG_PATH, "wb");
    }
    if (f) { fputc(ch, f); fclose(f); }
    fsdevCommitDevice("sdmc");
}

const char *lang_str(StringID id) {
    if (id < 0 || id >= STR_COUNT) return "?";
    
    if (g_lang == LANG_MINECRAFT) {
        return translate_to_enchanting(s_strings[id][LANG_EN]);
    }
    
    // LANG_RU represents Russian (index 1)
    int idx = (g_lang == LANG_RU) ? 1 : 0;
    return s_strings[id][idx];
}
