#include "quakedef.h"
#include "console.h"
#include "keys.h"
#include "common.h"
#include "cvar.h"
#include "cmd.h"
#include <client.h>



// Глобальные переменные
console_t con;
char key_lines[32][256];
int edit_line = 0;
int key_linepos = 0;
float con_scrollup = 0.0f;
static con_print_cb_t g_con_print_cb = NULL;

void Con_SetPrintCallback(con_print_cb_t cb)
{
    g_con_print_cb = cb;
}

void Con_DPrintf(const char* fmt, ...)
{
    // Если developer == 0, можно выйти
    // Но пока просто перенаправляем в обычный Printf
    va_list args;
    char msg[1024];

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    Con_Printf("%s", msg);
}

void Con_Init(void)
{
    con.linewidth = 80; // Стандартная ширина
    con.totallines = CON_TEXTSIZE / con.linewidth;
    con.current = 0;
    con.initialized = TRUE;

    // Очистка
    memset(con.text, 0, CON_TEXTSIZE);
    memset(key_lines, 0, sizeof(key_lines));

    key_lines[0][0] = ']'; // Маркер начала строки
    key_linepos = 1;

    Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);

    Con_Printf("Console initialized\n");
}

void Con_ToggleConsole_f(void)
{
    if (key_dest == key_game)
    {
        key_dest = key_console;
        // Освобождаем мышь (если была захвачена)
        // Sys_GrabMouse(FALSE); 
    }
    else
    {
        key_dest = key_game;
        // Захватываем мышь
        // Sys_GrabMouse(TRUE);
    }
}

void Con_Print(const char* txt)
{
    // Простая реализация печати в буфер
    // (без переносов слов для простоты)
    while (*txt)
    {
        if (*txt == '\n')
        {
            con.current++;
            if (con.current >= con.totallines)
                con.current = 0;
            // Очистка новой строки
            memset(&con.text[con.current * con.linewidth], 0, con.linewidth);
        }
        else
        {
            // Пишем в текущую строку (пока просто в конец, нужен x)
        }
        txt++;
    }
}

// Упрощенная версия для вывода в буфер
void Con_Printf(const char* fmt, ...)
{
    va_list args;
    char msg[1024];

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);

    if (g_con_print_cb)
        g_con_print_cb(msg);
    va_end(args);

    printf("%s", msg); // В консоль Windows всегда пишем

    // Если консоль не инициализирована или кривая - выходим
    if (!con.initialized || con.totallines <= 0 || con.linewidth <= 0)
        return;

    // Запись в игровой буфер
    int len = strlen(msg);
    for (int i = 0; i < len; i++)
    {
        if (msg[i] == '\n')
        {
            con.current = (con.current + 1) % con.totallines;
            memset(&con.text[con.current * con.linewidth], 0, con.linewidth);
            con.x = 0;
        }
        else
        {
            if (con.x < con.linewidth)
            {
                con.text[con.current * con.linewidth + con.x] = msg[i];
                con.x++;
            }
        }
    }
}

void Key_Console(int key)
{
    if (key == K_ENTER)
    {
        // Выполнить
        Cbuf_AddText(key_lines[edit_line] + 1); // Пропускаем ']'
        Cbuf_AddText("\n");

        Con_Printf("] %s\n", key_lines[edit_line] + 1);

        // Новая строка
        edit_line = (edit_line + 1) % 32;
        memset(key_lines[edit_line], 0, 256);
        key_lines[edit_line][0] = ']';
        key_linepos = 1;
        return;
    }

    if (key == K_BACKSPACE)
    {
        if (key_linepos > 1)
        {
            key_linepos--;
            key_lines[edit_line][key_linepos] = 0;
        }
        return;
    }

    // Ввод символов
    if (key >= 32 && key < 127 && key_linepos < 250)
    {
        if (key == '-' && (GetKeyState(VK_SHIFT) & 0x8000))
        {
            key = '_';
        }

        key_lines[edit_line][key_linepos] = key;
        key_linepos++;
    }
}

void Con_CheckResize(void)
{
    float speed = cls.frametime * 2.0f; // Скорость анимации

    if (key_dest == key_console)
    {
        // Открываем (едем вниз)
        con_scrollup -= speed;
        if (con_scrollup < 0.0f) con_scrollup = 0.0f;
    }
    else
    {
        // Закрываем (едем вверх)
        con_scrollup += speed;
        if (con_scrollup > 0.5f) con_scrollup = 0.5f; // Половина экрана
    }
}

void Con_Shutdown(void) {}
void Con_Clear_f(void) {}
void Con_ClearNotify(void) {}
void Con_DrawNotify(void) {}
void Con_Warning(const char* fmt, ...) { /* заглушка */ }
void Con_HistoryUp(void) {}
void Con_HistoryDown(void) {}
void Con_DrawInput(void) {}
void Con_CompleteCommand(void) {}
