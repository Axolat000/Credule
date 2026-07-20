// Prelude — Nintendo Switch homebrew for the Nextendo Network.
// Copyright (C) 2026 Nextendo Network
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.

// ============================================================
//  Nextendo .nro — point d'entree.
//  Ecran principal :
//    [ NEXTENDO ]   [ NINTENDO ]           <- bascule de mode (A = appliquer + REDEMARRER)
//    [ Splatoon 2 - Installer le planning ] <- installe le byaml BCAT de S2 (sans redemarrage)
//  Navigation : < > pour passer de la paire de modes a la barre S2.
//  Au lancement : verif de mise a jour -> bandeau discret + Y pour installer.
// ============================================================
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <netdb.h>
#include <SDL2/SDL.h>

#include "ui.h"
#include "audio.h"
#include "nextendo_apply.h"
#include "nextendo_bcat.h"
#include "nextendo_update.h"
#include "ui_theme.h"
#include "lang.h"

#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

enum {
    SCREEN_PICKER, SCREEN_S2_INFO, SCREEN_S2_PROGRESS, SCREEN_S2_RESULT,
    SCREEN_UPD_CONFIRM, SCREEN_UPD_PROGRESS, SCREEN_UPD_RESULT,
    SCREEN_LANG, SCREEN_NINTENDO_COMING, SCREEN_FAKE_BAN,
    SCREEN_BGM_MENU, SCREEN_PURPLE_MUSIC
};

static HidVibrationDeviceHandle s_vibHandles[2];
static bool s_hasVib = false;

// --- LOCAL BGM DISCOVERY ---
char g_local_bgms[32][64];
int g_local_bgm_count = 0;

void scan_local_bgms(void) {
    g_local_bgm_count = 0;
    DIR *d = opendir("romfs:/");
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            int len = strlen(dir->d_name);
            if (len > 4 && strcasecmp(dir->d_name + len - 4, ".mp3") == 0) {
                if (strcasecmp(dir->d_name, "sfx1.mp3") != 0) {
                    strncpy(g_local_bgms[g_local_bgm_count], dir->d_name, 63);
                    g_local_bgms[g_local_bgm_count][63] = '\0';
                    g_local_bgm_count++;
                    if (g_local_bgm_count >= 32) break;
                }
            }
        }
        closedir(d);
    }
}

// --- PURPLEMUSIC INTEGRATION ---
#define MAX_SONGS 500
#define STREAM_BUFFER_SIZE (16 * 1024 * 1024)

Song playlist[MAX_SONGS];
int total_songs = 0;
int playing_song_idx = -1;

static double current_playback_time = 0.0;
static double estimated_duration = 0.0;
static u32 playback_start_ticks = 0;

static unsigned char *stream_buffer = NULL;
static u32 volatile stream_buffer_size = 0;
static int volatile download_finished_ok = 0;
static int volatile download_thread_active = 0;
static int volatile kill_download_requested = 0;
static pthread_t download_thread;
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static int is_downloading = 0;

// JSON Helpers
static void clean_json_text(char *str) {
    char *src = str; char *dst = str;
    while (*src) {
        if (*src == '\\') {
            src++; 
            if (*src == 'u') {
                src++;
                char hex[5] = {src[0], src[1], src[2], src[3], 0};
                int cp = strtol(hex, NULL, 16);
                if (cp <= 0x7F) { *dst++ = cp; }
                else if (cp <= 0x7FF) { *dst++ = 0xC0 | (cp >> 6); *dst++ = 0x80 | (cp & 0x3F); }
                else { *dst++ = 0xE0 | (cp >> 12); *dst++ = 0x80 | ((cp >> 6) & 0x3F); *dst++ = 0x80 | (cp & 0x3F); }
                src += 3;
            } else { *dst++ = *src; }
        } else { *dst++ = *src; }
        src++;
    }
    *dst = '\0';
}

static void extract_json_string(char *source, const char *key, char *dest, int max_len) {
    char search[64]; snprintf(search, sizeof(search), "\"%s\":\"", key);
    char *start = strstr(source, search);
    if (start) {
        start += strlen(search);
        char *end = strchr(start, '"');
        while (end && *(end-1) == '\\') end = strchr(end + 1, '"');
        if (end) {
            int len = end - start;
            if (len >= max_len) len = max_len - 1;
            strncpy(dest, start, len);
            dest[len] = '\0';
            clean_json_text(dest);
        }
    } else dest[0] = '\0';
}

static void extract_json_int_as_str(char *source, const char *key, char *dest) {
    char search[64]; snprintf(search, sizeof(search), "\"%s\":", key);
    char *start = strstr(source, search);
    if (start) {
        start += strlen(search);
        while (*start == ' ' || *start == '"') start++;
        snprintf(dest, 16, "%d", atoi(start));
    }
}

static void extract_json_int_val(char *source, const char *key, int *dest) {
    char search[64]; snprintf(search, sizeof(search), "\"%s\":", key);
    char *start = strstr(source, search);
    if (start) {
        start += strlen(search);
        while (*start == ' ' || *start == '"') start++;
        *dest = atoi(start); 
    }
}

// Audio controls
static void clear_audio_memory(void) {
    audio_pause();
    if (download_thread_active) {
        kill_download_requested = 1;
        pthread_join(download_thread, NULL);
    }
    pthread_mutex_lock(&audio_mutex);
    if (stream_buffer) { 
        free(stream_buffer); 
        stream_buffer = NULL; 
    }
    stream_buffer_size = 0;
    download_finished_ok = 0;
    kill_download_requested = 0;
    pthread_mutex_unlock(&audio_mutex);
    is_downloading = 0;
    estimated_duration = 0.0;
    current_playback_time = 0.0;
}

// Background download thread
typedef struct {
    char track_id[16];
} DownloadArgs;

static void increment_view(const char* id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *hp = gethostbyname("randomfilm.fr");
    if (!hp) { close(sock); return; }
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(80);
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    if(connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) { close(sock); return; }
    char data[64]; snprintf(data, sizeof(data), "track_id=%s", id);
    char request[1024]; 
    snprintf(request, sizeof(request), "POST /music/api.php?action=increment_play HTTP/1.0\r\nHost: randomfilm.fr\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %zu\r\n\r\n%s", strlen(data), data);
    send(sock, request, strlen(request), 0);
    close(sock);
}

static void* download_audio_thread_func(void* arg) {
    DownloadArgs* args = (DownloadArgs*)arg;
    
    pthread_mutex_lock(&audio_mutex);
    if (stream_buffer) {
        free(stream_buffer);
    }
    stream_buffer = malloc(STREAM_BUFFER_SIZE);
    stream_buffer_size = 0;
    download_finished_ok = 0;
    pthread_mutex_unlock(&audio_mutex);
    
    increment_view(args->track_id);
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *hp = gethostbyname("randomfilm.fr");
    if (!hp) { 
        close(sock); 
        download_thread_active = 0; 
        free(args); 
        return NULL; 
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(80);
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    
    if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) { 
        close(sock); 
        download_thread_active = 0; 
        free(args); 
        return NULL; 
    }

    char request[1024]; 
    snprintf(request, sizeof(request), 
             "GET /music/api.php?action=stream&q=%s HTTP/1.0\r\nHost: randomfilm.fr\r\n\r\n", 
             args->track_id);
    send(sock, request, strlen(request), 0);

    char chunk[16384];
    s32 ret;
    int parse_state = 0;
    u32 audio_start_idx = 0;

    while ((ret = recv(sock, chunk, sizeof(chunk), 0)) > 0) {
        if (kill_download_requested) break;

        pthread_mutex_lock(&audio_mutex);
        if (parse_state == 0) {
            for (s32 i = 0; i < ret - 4; i++) {
                if (chunk[i] == '\r' && chunk[i+1] == '\n' && 
                    chunk[i+2] == '\r' && chunk[i+3] == '\n') {
                    
                    audio_start_idx = i + 4;
                    parse_state = 1;
                    u32 to_copy = ret - audio_start_idx;
                    if (stream_buffer_size + to_copy < STREAM_BUFFER_SIZE) {
                        memcpy(stream_buffer + stream_buffer_size, chunk + audio_start_idx, to_copy);
                        stream_buffer_size += to_copy;
                    }
                    break;
                }
            }
        } else {
            if (stream_buffer_size + ret < STREAM_BUFFER_SIZE) {
                memcpy(stream_buffer + stream_buffer_size, chunk, ret);
                stream_buffer_size += ret;
            }
        }
        pthread_mutex_unlock(&audio_mutex);
    }

    close(sock);
    
    pthread_mutex_lock(&audio_mutex);
    if (!kill_download_requested) {
        download_finished_ok = 1; 
    }
    pthread_mutex_unlock(&audio_mutex);

    download_thread_active = 0;
    free(args);
    return NULL;
}

static void start_playback(int index) {
    if (index < 0 || index >= total_songs) return;

    clear_audio_memory(); 
    
    Song* target = &playlist[index];
    is_downloading = 1;
    estimated_duration = (double)target->duration;
    
    DownloadArgs* args = malloc(sizeof(DownloadArgs));
    memset(args->track_id, 0, sizeof(args->track_id));
    strncpy(args->track_id, target->id, 15);
    
    download_thread_active = 1;
    pthread_create(&download_thread, NULL, download_audio_thread_func, args);
}

static void fetch_playlist(void) {
    total_songs = 0;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *hp = gethostbyname("randomfilm.fr");
    if (!hp) return;

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(80);
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);

    if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) { close(sock); return; }

    char request[1024]; 
    snprintf(request, sizeof(request), "GET /music/api.php?action=list HTTP/1.0\r\nHost: randomfilm.fr\r\n\r\n");
    send(sock, request, strlen(request), 0);

    u32 buf_size = 512 * 1024;
    char *json_response = malloc(buf_size);
    if(!json_response) { close(sock); return; }
    memset(json_response, 0, buf_size);
    
    int received = 0, ret;
    while ((ret = recv(sock, json_response + received, buf_size - 1 - received, 0)) > 0) {
        received += ret;
        if (received >= buf_size - 1) break;
    }
    close(sock);

    if (received > 0) {
        char *body = strstr(json_response, "\r\n\r\n");
        if (body) {
            body += 4;
            char *cursor = strchr(body, '[');
            if (cursor) {
                while (total_songs < MAX_SONGS && (cursor = strchr(cursor, '{'))) {
                    char *end_obj = strchr(cursor, '}');
                    if (!end_obj) break;
                    char save = *end_obj; *end_obj = '\0'; 
                    char temp_id[16];
                    extract_json_int_as_str(cursor, "id", temp_id);
                    strcpy(playlist[total_songs].id, temp_id);
                    extract_json_string(cursor, "title", playlist[total_songs].title, 127);
                    if (strlen(playlist[total_songs].title) == 0) {
                        snprintf(playlist[total_songs].title, 128, "Track %s", temp_id);
                    }
                    
                    extract_json_string(cursor, "artist", playlist[total_songs].artist, 127);
                    if (strlen(playlist[total_songs].artist) == 0 || strcmp(playlist[total_songs].artist, "null") == 0) 
                        snprintf(playlist[total_songs].artist, 128, "Unknown Artist");
                    
                    extract_json_string(cursor, "genre", playlist[total_songs].genre, 63);
                    if (strlen(playlist[total_songs].genre) == 0 || strcmp(playlist[total_songs].genre, "null") == 0) 
                        snprintf(playlist[total_songs].genre, 64, "Other");
                    
                    extract_json_int_val(cursor, "play_count", &playlist[total_songs].play_count);
                    extract_json_int_val(cursor, "duration", &playlist[total_songs].duration); 

                    total_songs++;
                    *end_obj = save;
                    cursor = end_obj + 1;
                }
            }
        }
    }
    free(json_response);
}

int main(int argc, char **argv) {
    romfsInit();
    socketInitializeDefault();
    audio_init();
    lang_init();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    if (R_SUCCEEDED(hidInitializeVibrationDevices(s_vibHandles, 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual))) {
        s_hasVib = true;
    } else if (R_SUCCEEDED(hidInitializeVibrationDevices(s_vibHandles, 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld))) {
        s_hasVib = true;
    }

    if (!ui_init()) {
        audio_exit();
        romfsExit();
        socketExit();
        return 1;
    }

    // La trace repart de zero a chaque lancement : elle documente LA session courante (celle dont
    // l'utilisateur nous parle) et ne grossit pas indefiniment. Conservee en release : le gel du
    // build 10 n'a jamais ete explique, donc si un joueur le revit, ce fichier est notre seul temoin.
    remove(NEXTENDO_TRACE_PATH);
    nextendo_trace("10 main: ui_init ok");

    // Une console sans emuMMC fait tourner le CFW sur la memoire interne, donc avec son vrai
    // identifiant : blank_prodinfo_emummc, la protection posee par le mode NINTENDO, n'a alors
    // aucun effet. On previent au lieu de laisser croire a une protection inexistante.
    // Detecte UNE fois (splInitialize/splExit), pas a chaque frame.
    nextendo_trace("11 avant detect_boot (splInitialize)");
    BootType boot = nextendo_detect_boot();
    nextendo_trace(boot == BOOT_SYSMMC  ? "12 detect_boot = SYSMMC (pas d emuMMC)"
                 : boot == BOOT_EMUMMC  ? "12 detect_boot = EMUMMC"
                                        : "12 detect_boot = INCONNU (spl a echoue)");
    bool noEmummc = (boot == BOOT_SYSMMC);

    int  current = nextendo_current_mode();
    int  sel    = (current == CHOICE_NEXTENDO) ? CHOICE_NINTENDO : CHOICE_NEXTENDO;
    int  focus  = FOCUS_MODE;
    int  screen = SCREEN_PICKER;
    int  state  = 0;
    int  langSel = g_lang;
    char status[160] = {0};
    char rTitle[64] = {0}, rMsg[192] = {0};
    bool rOk = false;
    int nintendo_distance = -1;
    bool nintendo_in_meters = false;

    // Verif de mise a jour au demarrage (affiche d'abord le picker pour ne pas rester noir).
    ui_draw_picker(sel, current, focus, NULL, 0);
    nextendo_trace("13 picker dessine");
    NextendoUpdate upd = { false, 0, 0 }; // Bypassed for Credule fork
    nextendo_trace("15 entree dans la boucle principale");

    bool tracedLoop = false, tracedConfirm = false;
    while (appletMainLoop()) {
        consoleUpdate(NULL);
        padUpdate(&pad);
        u64 k = padGetButtonsDown(&pad);
        if (k) {
            g_major_shake = 15;
        }

        // Background audio monitor and auto-play
        pthread_mutex_lock(&audio_mutex);
        if (is_downloading && download_finished_ok) {
            audio_play_mem(stream_buffer, stream_buffer_size);
            playback_start_ticks = SDL_GetTicks();
            is_downloading = 0;
            download_finished_ok = 0;
        }
        pthread_mutex_unlock(&audio_mutex);

        // Auto-play next online track
        if (playing_song_idx != -1 && !audio_is_playing() && !audio_is_paused() && !is_downloading) {
            if (playing_song_idx < total_songs - 1) {
                playing_song_idx++;
                start_playback(playing_song_idx);
            }
        }

        // BGM status update
        if (is_downloading) {
            snprintf(status, sizeof(status), "Caching online Music... (%d KB)", (int)(stream_buffer_size / 1024));
        } else if (download_thread_active) {
            snprintf(status, sizeof(status), "Loading online Music...");
        } else if (audio_is_playing() && playing_song_idx != -1) {
            snprintf(status, sizeof(status), "Playing: %s", playlist[playing_song_idx].title);
        } else {
            if (strncmp(status, "Caching", 7) == 0 || strncmp(status, "Loading", 7) == 0 || strncmp(status, "Playing:", 8) == 0) {
                status[0] = '\0';
            }
        }

        if (s_hasVib) {
            if (g_major_shake > 0) {
                HidVibrationValue v = { .amp_low = 0.9f, .freq_low = 140.0f, .amp_high = 0.9f, .freq_high = 280.0f };
                hidSendVibrationValue(s_vibHandles[0], &v);
                hidSendVibrationValue(s_vibHandles[1], &v);
            } else {
                HidVibrationValue v_stop = { 0 };
                hidSendVibrationValue(s_vibHandles[0], &v_stop);
                hidSendVibrationValue(s_vibHandles[1], &v_stop);
            }
        }
        // Une seule fois : prouve que la boucle tourne ET que l'entree remonte (si A ne fait rien
        // alors que cette ligne est absente, c'est padUpdate/HID qui est mort, pas la logique).
        if (!tracedLoop && k) { nextendo_trace("16 premiere touche detectee dans la boucle"); tracedLoop = true; }

        if (screen == SCREEN_PICKER) {
            if (state == 0) {
                if (k & (HidNpadButton_B | HidNpadButton_Plus)) break;
                if (k & HidNpadButton_R) { screen = SCREEN_LANG; langSel = g_lang; }
                if (k & HidNpadButton_L) {
                    ui_draw_progress("Fetching Music list...");
                    svcSleepThread(100000000ULL);
                    scan_local_bgms();
                    fetch_playlist();
                    screen = SCREEN_BGM_MENU;
                    state = 0;
                    langSel = 0; // scroll_y
                }
                if (upd.available) {
                    // MAJ OBLIGATOIRE : tant qu'une version plus recente existe, le homebrew
                    // est verrouille -> seules l'installation (Y) et la sortie (+/B) sont possibles.
                    if (k & HidNpadButton_Y) { screen = SCREEN_UPD_CONFIRM; }
                } else {
                    if (k & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight | HidNpadButton_AnyUp | HidNpadButton_AnyDown)) {
                        if (k & (HidNpadButton_AnyUp | HidNpadButton_AnyLeft)) {
                            focus = (focus == FOCUS_MODE) ? FOCUS_CALL : (focus == FOCUS_CALL ? FOCUS_S2 : FOCUS_MODE);
                        } else {
                            focus = (focus == FOCUS_MODE) ? FOCUS_S2 : (focus == FOCUS_S2 ? FOCUS_CALL : FOCUS_MODE);
                        }
                        status[0] = 0;
                    }
                    if (k & HidNpadButton_A) {
                        if (focus == FOCUS_MODE) {
                            nextendo_trace("17 A picker -> ecran de confirmation");
                            state = 1;
                            status[0] = 0;
                        } else if (focus == FOCUS_S2) {
                            screen = SCREEN_S2_INFO;
                        } else if (focus == FOCUS_CALL) {
                            screen = SCREEN_NINTENDO_COMING;
                            nintendo_distance = 9000;
                            nintendo_in_meters = false;
                        }
                    }
                }
                if (screen == SCREEN_PICKER && state == 0)
                    ui_draw_picker(sel, current, focus, status[0] ? status : NULL,
                                   upd.available ? upd.latest : 0);
            } else {
                if (k & (HidNpadButton_B | HidNpadButton_Plus)) {
                    state = 0;
                } else if (k & HidNpadButton_A) {
                    nextendo_trace("19 A confirmation -> appel de apply_*");
                    bool ok = (sel == CHOICE_NEXTENDO) ? nextendo_apply_nextendo()
                                                       : nextendo_apply_nintendo();
                    nextendo_trace(ok ? "28 apply a renvoye OK -> reboot"
                                      : "28 apply a renvoye ECHEC -> message d erreur");
                    if (ok) {
                        snprintf(status, sizeof(status), "%s",
                                 lang_str(sel == CHOICE_NEXTENDO
                                     ? STR_STATUS_NEXTENDO_OK
                                     : STR_STATUS_NINTENDO_OK));
                        ui_draw_picker(sel, current, focus, status, upd.available ? upd.latest : 0);
                        svcSleepThread(1200000000ULL);
                        audio_exit();
                        nextendo_reboot();
                        snprintf(status, sizeof(status), "%s", lang_str(STR_STATUS_REBOOT_FAIL));
                        state = 0;
                    } else {
                        snprintf(status, sizeof(status), "%s", lang_str(STR_STATUS_SD_ERROR));
                        state = 0;
                    }
                }
                if (state == 1) {
                    if (!tracedConfirm) { nextendo_trace("18 avant ui_draw_confirm"); }
                    ui_draw_confirm(sel, noEmummc);
                    if (!tracedConfirm) { nextendo_trace("18b ui_draw_confirm rendu ok"); tracedConfirm = true; }
                }
            }

        } else if (screen == SCREEN_S2_INFO) {
            if (k & (HidNpadButton_B | HidNpadButton_Plus)) {
                screen = SCREEN_PICKER;
            } else if (k & HidNpadButton_A) {
                screen = SCREEN_S2_PROGRESS;
            }
            if (screen == SCREEN_S2_INFO) ui_draw_s2_info();

        } else if (screen == SCREEN_LANG) {
            if (k & HidNpadButton_B) {
                screen = SCREEN_PICKER;
            } else if (k & HidNpadButton_A) {
                if (langSel != g_lang) {
                    g_lang = langSel;
                    lang_save();
                }
                screen = SCREEN_PICKER;
            } else if (k & HidNpadButton_Up) {
                if (langSel > 0) langSel--;
            } else if (k & HidNpadButton_Down) {
                if (langSel < 2) langSel++;
            }
            if (screen == SCREEN_LANG) ui_draw_lang_menu(langSel);

        } else if (screen == SCREEN_UPD_CONFIRM) {
            if (k & HidNpadButton_A) {
                screen = SCREEN_UPD_PROGRESS;
            } else if (k & (HidNpadButton_B | HidNpadButton_Plus)) {
                screen = SCREEN_PICKER;
            }
            if (screen == SCREEN_UPD_CONFIRM) ui_draw_upd_confirm(upd.latest);

        } else if (screen == SCREEN_S2_PROGRESS) {
            ui_draw_progress(lang_str(STR_STATUS_DOWNLOAD_SCHEDULE));
            svcSleepThread(150000000ULL);
            nextendo_bcat_result res = nextendo_bcat_install_s2();
            rOk = (res == NB_OK);
            switch (res) {
                case NB_OK:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_SCHEDULE_OK));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_SCHEDULE_OK_DESC));
                    break;
                case NB_NO_SCHEDULE:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_NO_SCHEDULE));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_NO_SCHEDULE_DESC));
                    break;
                case NB_MOUNT_FAIL:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_MOUNT_FAIL));
                    snprintf(rMsg, sizeof(rMsg), "%s (rc=0x%x)", lang_str(STR_STATUS_MOUNT_FAIL_DESC), g_last_rc);
                    break;
                case NB_NET_CONNECT:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_NET_CONNECT));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_NET_CONNECT_DESC));
                    break;
                case NB_NET_TIMEOUT:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_NET_TIMEOUT));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_NET_TIMEOUT_DESC));
                    break;
                case NB_NET_HTTP_ERR:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_NET_HTTP_ERR));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_NET_HTTP_ERR_DESC));
                    break;
                case NB_NET_FAIL:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_NET_FAIL));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_NET_FAIL_DESC));
                    break;
                default:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_WRITE_FAIL));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_WRITE_FAIL_DESC));
                    break;
            }
            screen = SCREEN_S2_RESULT;

        } else if (screen == SCREEN_UPD_PROGRESS) {
            ui_draw_progress(lang_str(STR_STATUS_DOWNLOAD_UPDATE));
            svcSleepThread(150000000ULL);
            nextendo_update_result res = nextendo_update_apply(upd.size);
            rOk = (res == NUP_OK);
            switch (res) {
                case NUP_OK:
                    upd.available = false;   // faite : on retire le bandeau
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_UPDATE_OK));
                    snprintf(rMsg, sizeof(rMsg), lang_str(STR_STATUS_UPDATE_OK_DESC), upd.latest);
                    break;
                case NUP_SIZE_FAIL:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_UPDATE_SIZE_FAIL));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_UPDATE_SIZE_FAIL_DESC));
                    break;
                case NUP_WRITE_FAIL:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_UPDATE_WRITE_FAIL));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_UPDATE_WRITE_FAIL_DESC));
                    break;
                default:
                    snprintf(rTitle, sizeof(rTitle), "%s", lang_str(STR_STATUS_UPDATE_NET_FAIL));
                    snprintf(rMsg, sizeof(rMsg), "%s", lang_str(STR_STATUS_UPDATE_NET_FAIL_DESC));
                    break;
            }
            screen = SCREEN_UPD_RESULT;

        } else if (screen == SCREEN_NINTENDO_COMING) {
            g_major_shake = 20;
            if (!nintendo_in_meters) {
                // Decrement km slower
                nintendo_distance -= 47;
                if (nintendo_distance <= 0) {
                    nintendo_distance = 1000;
                    nintendo_in_meters = true;
                }
            } else {
                // Decrement meters slower
                nintendo_distance -= 5;
                if (nintendo_distance <= 0) {
                    audio_play_sfx();
                    screen = SCREEN_FAKE_BAN;
                }
            }
            ui_draw_nintendo_coming(nintendo_distance, nintendo_in_meters);
            svcSleepThread(16666666ULL);

        } else if (screen == SCREEN_FAKE_BAN) {
            g_major_shake = 0; // stop vibration/shaking
            if (k) {
                break; // Exit app on any button press
            }
            ui_draw_fake_ban();

        } else if (screen == SCREEN_BGM_MENU) {
            g_major_shake = 0;
            int total_bgm_items = g_local_bgm_count + total_songs;
            if (k & HidNpadButton_Up) {
                if (state > 0) state--;
                else state = total_bgm_items - 1;
            } else if (k & HidNpadButton_Down) {
                if (state < total_bgm_items - 1) state++;
                else state = 0;
            } else if (k & HidNpadButton_A) {
                if (state < g_local_bgm_count) {
                    audio_play_bgm_file(g_local_bgms[state]);
                    playing_song_idx = -1;
                    screen = SCREEN_PICKER;
                    state = 0;
                } else {
                    playing_song_idx = state - g_local_bgm_count;
                    start_playback(playing_song_idx);
                    screen = SCREEN_PICKER;
                    state = 0;
                }
            } else if (k & (HidNpadButton_B | HidNpadButton_Plus)) {
                screen = SCREEN_PICKER;
                state = 0;
            }

            int max_scr = (total_bgm_items * 45) - 360;
            if (max_scr < 0) max_scr = 0;
            if (state * 45 < langSel) langSel = state * 45;
            if (state * 45 > langSel + 360 - 45) langSel = state * 45 - 360 + 45;
            if (langSel > max_scr) langSel = max_scr;
            if (langSel < 0) langSel = 0;

            if (screen == SCREEN_BGM_MENU) {
                ui_draw_bgm_menu(state, langSel, playing_song_idx);
            }

        } else { // SCREEN_S2_RESULT / SCREEN_UPD_RESULT / SCREEN_UPD_CONFIRM (fallback)
            if (k & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Plus))
                screen = SCREEN_PICKER;
            ui_draw_result(rTitle, rMsg, rOk);
        }
    }

    if (s_hasVib) {
        HidVibrationValue v_stop = { 0 };
        hidSendVibrationValue(s_vibHandles[0], &v_stop);
        hidSendVibrationValue(s_vibHandles[1], &v_stop);
    }
    ui_exit();
    audio_exit();
    romfsExit();
    return 0;
}
