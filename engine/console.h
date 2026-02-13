// engine/console.h
#ifndef CONSOLE_H
#define CONSOLE_H

#include "quakedef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CON_TEXTSIZE    32768
#define NUM_CON_TIMES   4
#define CON_LINES       128
#define CON_LINE_WIDTH  80

typedef struct console_s {
    char text[CON_TEXTSIZE];
    int current;            // текущая строка
    int x;                  // позиция в текущей строке
    int display;            // отображаемая строка
    int linewidth;
    int totallines;
    float cursorspeed;
    int vislines;
    float times[NUM_CON_TIMES];
    qboolean initialized;
} console_t;

extern console_t con;

extern char key_lines[32][256];
extern int edit_line;
extern float con_scrollup;

typedef void (*con_print_cb_t)(const char* text);
void Con_SetPrintCallback(con_print_cb_t cb);

void Con_Init(void);
void Con_Shutdown(void);
void Con_ToggleConsole_f(void);
void Con_Clear_f(void);
void Con_ClearNotify(void);
void Con_Printf(const char* fmt, ...);
void Con_DPrintf(const char* fmt, ...);
void Con_Warning(const char* fmt, ...);
void Con_Print(const char* txt);
void Con_Draw(void);
void Con_DrawNotify(void);
void Con_CheckResize(void);

// Input
void Key_Console(int key);
void Con_CompleteCommand(void);
void Con_DrawInput(void);

// History
void Con_HistoryUp(void);
void Con_HistoryDown(void);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_H
