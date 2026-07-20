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
//  Nextendo .nro — thème UI (style Nimbus/Pretendo, marque rouge+bleu).
// ============================================================
#ifndef UI_THEME_H
#define UI_THEME_H
#include <switch.h>

typedef struct { u8 r, g, b, a; } Color;
#define COL(R, G, B) ((Color){ (R), (G), (B), 255 })

#define C_RED      COL(0x39, 0xFF, 0x14)  // vert fluo dégueulasse
#define C_BLUE     COL(0xFF, 0x14, 0x93)  // rose fuchsia insupportable
#define C_BG       COL(0x12, 0x14, 0x19)  // fond (sera surchargé par le gradient dynamique)
#define C_TITLE    COL(0xFF, 0xFF, 0x00)  // jaune stabilo clignotant
#define C_SUBTLE   COL(0xBF, 0x00, 0xFF)  // violet agressif
#define C_CARD     COL(0x55, 0x6B, 0x2F)  // vert caca d'oie
#define C_CARD_SEL COL(0xFF, 0x8C, 0x00)  // orange citrouille sélectionné
#define C_GREEN    COL(0x00, 0xFF, 0xFF)  // cyan électrique
#define C_WARN     COL(0xFF, 0x00, 0x00)  // rouge sang
#define C_S2       COL(0x8B, 0x45, 0x13)  // marron boue

#define FB_W 1280
#define FB_H 720
#define CARD_W 300
#define CARD_H 300
#define CARD_GAP 80
#define CARD_Y 170
#define CARD0_X ((FB_W - (2 * CARD_W + CARD_GAP)) / 2)  // gauche (Nextendo)
#define CARD1_X (CARD0_X + CARD_W + CARD_GAP)           // droite (Nintendo)

// Barre "Splatoon 2 — Planning en ligne" sous les deux cartes de mode.
#define S2BAR_W 900
#define S2BAR_H 64
#define S2BAR_X ((FB_W - S2BAR_W) / 2)
#define S2BAR_Y 500

#define CHOICE_NEXTENDO 0
#define CHOICE_NINTENDO 1

// Focus de l'ecran de bascule : la paire de cartes (mode), la barre S2 ou le bouton de secours.
#define FOCUS_MODE 0
#define FOCUS_S2   1
#define FOCUS_CALL 2

#endif // UI_THEME_H
