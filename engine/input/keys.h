// engine/input/keys.h
#ifndef KEYS_H
#define KEYS_H

#include "quakedef.h"

// Специальные клавиши
#define K_TAB           9
#define K_ENTER         13
#define K_ESCAPE        27
#define K_SPACE         32
#define K_BACKSPACE     127

#define K_UPARROW       128
#define K_DOWNARROW     129
#define K_LEFTARROW     130
#define K_RIGHTARROW    131

#define K_ALT           132
#define K_CTRL          133
#define K_SHIFT         134

#define K_F1            135
#define K_F2            136
#define K_F3            137
#define K_F4            138
#define K_F5            139
#define K_F6            140
#define K_F7            141
#define K_F8            142
#define K_F9            143
#define K_F10           144
#define K_F11           145
#define K_F12           146

#define K_INS           147
#define K_DEL           148
#define K_PGDN          149
#define K_PGUP          150
#define K_HOME          151
#define K_END           152

#define K_PAUSE         153
#define K_CAPSLOCK      154

// Мышь
#define K_MOUSE1        200
#define K_MOUSE2        201
#define K_MOUSE3        202
#define K_MOUSE4        203
#define K_MOUSE5        204
#define K_MWHEELUP      205
#define K_MWHEELDOWN    206

// Состояния
typedef enum {
    key_game,
    key_console,
    key_message,
    key_menu
} keydest_t;

extern keydest_t key_dest;

// Функции
void Key_Init(void);
void Key_Event(int key, qboolean down);
void Key_ClearStates(void);
int Key_StringToKeynum(const char* str);
const char* Key_KeynumToString(int keynum);
void Key_SetBinding(int keynum, const char* binding);
const char* Key_GetBinding(int keynum);
void Key_WriteBindings(FILE* f);
qboolean Key_IsDown(int key);

#endif // KEYS_H
