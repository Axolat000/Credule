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
//  Nextendo .nro — musique de fond en boucle (SDL2_mixer, audio-only).
//  Joue romfs:/bgm.mp3 en boucle infinie. Non-fatal : si l'audio ou le
//  fichier manque, l'app continue en silence.
//  SDL_MAIN_HANDLED : on garde NOTRE main() (dans main.c), SDL ne le detourne pas.
// ============================================================
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <switch.h>
#include <stdio.h>
#include <sys/stat.h>

#include "audio.h"

static Mix_Music *s_music     = NULL;
static bool       s_audioOpen = false;

char g_current_bgm[64] = "default-From-Tiktok.mp3";

static void load_bgm_choice(void) {
    FILE *f = fopen("sdmc:/switch/credule_bgm.txt", "r");
    if (f) {
        if (fgets(g_current_bgm, sizeof(g_current_bgm), f)) {
            int len = strlen(g_current_bgm);
            while (len > 0 && (g_current_bgm[len - 1] == '\n' || g_current_bgm[len - 1] == '\r' || g_current_bgm[len - 1] == ' ')) {
                g_current_bgm[len - 1] = '\0';
                len--;
            }
        }
        fclose(f);
    }
}

void audio_halt(void) {
    if (!s_audioOpen) return;
    Mix_HaltMusic();
    if (s_music) { Mix_FreeMusic(s_music); s_music = NULL; }
}

static void save_bgm_choice(const char *name) {
    mkdir("sdmc:/switch", 0777);
    FILE *f = fopen("sdmc:/switch/credule_bgm.txt", "w");
    if (f) {
        fprintf(f, "%s\n", name);
        fclose(f);
    }
}

bool audio_init(void) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_AUDIO) != 0) return false;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) != 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    s_audioOpen = true;
    Mix_Init(MIX_INIT_MP3);

    load_bgm_choice();
    char path[256];
    // Si le choix sauvegardé est un fichier téléchargé sur SD (préfixe "sd:")
    if (strncmp(g_current_bgm, "sd:", 3) == 0) {
        snprintf(path, sizeof(path), "sdmc:/switch/credule_music/%s", g_current_bgm + 3);
    } else {
        snprintf(path, sizeof(path), "romfs:/%s", g_current_bgm);
    }
    s_music = Mix_LoadMUS(path);
    if (!s_music) {
        // Fallback to default romfs BGM
        s_music = Mix_LoadMUS("romfs:/default-From-Tiktok.mp3");
        if (s_music) strcpy(g_current_bgm, "default-From-Tiktok.mp3");
    }
    if (!s_music) return false;
    Mix_VolumeMusic(MIX_MAX_VOLUME);
    Mix_PlayMusic(s_music, -1); // -1 = boucle infinie
    return true;
}

void audio_play_bgm_file(const char *filename) {
    if (!s_audioOpen) return;
    if (s_music) { Mix_HaltMusic(); Mix_FreeMusic(s_music); s_music = NULL; }
    char path[128];
    snprintf(path, sizeof(path), "romfs:/%s", filename);
    s_music = Mix_LoadMUS(path);
    if (s_music) {
        Mix_VolumeMusic(MIX_MAX_VOLUME);
        Mix_PlayMusic(s_music, -1); // boucle infinie
        strncpy(g_current_bgm, filename, sizeof(g_current_bgm) - 1);
        g_current_bgm[sizeof(g_current_bgm) - 1] = '\0';
        save_bgm_choice(filename);
    }
}

// Joue un fichier MP3 déjà téléchargé sur la SD en boucle infinie
void audio_play_bgm_sd(const char *sd_filename) {
    if (!s_audioOpen) return;
    if (s_music) { Mix_HaltMusic(); Mix_FreeMusic(s_music); s_music = NULL; }
    char path[256];
    snprintf(path, sizeof(path), "sdmc:/switch/credule_music/%s", sd_filename);
    s_music = Mix_LoadMUS(path);
    if (s_music) {
        Mix_VolumeMusic(MIX_MAX_VOLUME);
        Mix_PlayMusic(s_music, -1); // boucle infinie
        // Sauvegarde avec le préfixe "sd:" pour le rechargement au prochain boot
        snprintf(g_current_bgm, sizeof(g_current_bgm), "sd:%s", sd_filename);
        save_bgm_choice(g_current_bgm);
    }
}

void audio_play_mem(unsigned char *buf, size_t size) {
    if (!s_audioOpen) return;
    if (s_music) { Mix_HaltMusic(); Mix_FreeMusic(s_music); s_music = NULL; }
    SDL_RWops *rw = SDL_RWFromMem(buf, size);
    if (rw) {
        s_music = Mix_LoadMUS_RW(rw, 1);
        if (s_music) {
            Mix_VolumeMusic(MIX_MAX_VOLUME);
            Mix_PlayMusic(s_music, -1); // boucle infinie
        }
    }
}

void audio_pause(void) {
    if (s_audioOpen) Mix_PauseMusic();
}

void audio_resume(void) {
    if (s_audioOpen) Mix_ResumeMusic();
}

bool audio_is_paused(void) {
    return s_audioOpen && Mix_PausedMusic();
}

bool audio_is_playing(void) {
    return s_audioOpen && Mix_PlayingMusic();
}

static Mix_Music *s_sfx = NULL;

void audio_play_sfx(void) {
    if (!s_audioOpen) return;
    if (s_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(s_music);
        s_music = NULL;
    }
    if (s_sfx) {
        Mix_FreeMusic(s_sfx);
    }
    s_sfx = Mix_LoadMUS("romfs:/sfx1.mp3");
    if (s_sfx) {
        Mix_VolumeMusic(MIX_MAX_VOLUME);
        Mix_PlayMusic(s_sfx, 0);
    }
}

void audio_exit(void) {
    if (s_music) { Mix_HaltMusic(); Mix_FreeMusic(s_music); s_music = NULL; }
    if (s_sfx) { Mix_HaltMusic(); Mix_FreeMusic(s_sfx); s_sfx = NULL; }
    if (s_audioOpen) { Mix_CloseAudio(); s_audioOpen = false; }
    Mix_Quit();
    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
