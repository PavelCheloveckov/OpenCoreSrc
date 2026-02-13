// client/menu.c
#include "quakedef.h"
#include "client.h"
#include "console.h"
#include "gl_local.h"
#include "keys.h"
#include "server.h"

extern int char_texture;
extern HWND hwnd_main;
extern qboolean mouse_grabbed;

typedef enum {
    MENU_NONE = 0,
    MENU_DEV
} menu_state_t;

static menu_state_t menu_state = MENU_NONE;
static int menu_cursor = 0;

static const char* dev_menu_items[] = {
    "Load Map: 1stqmfmap",
    "Close Menu",
    "Quit"
};
#define DEV_MENU_COUNT 3

// ============================================================================
// Мышь
// ============================================================================

static void Menu_GrabMouse(void)
{
#ifdef _WIN32
    if (hwnd_main)
    {
        RECT r;
        GetClientRect(hwnd_main, &r);
        ClientToScreen(hwnd_main, (POINT*)&r.left);
        ClientToScreen(hwnd_main, (POINT*)&r.right);
        ClipCursor(&r);
        SetCapture(hwnd_main);
        while (ShowCursor(FALSE) >= 0);

        int cx = (r.left + r.right) / 2;
        int cy = (r.top + r.bottom) / 2;
        SetCursorPos(cx, cy);
    }
#endif
    mouse_grabbed = TRUE;
}

static void Menu_ReleaseMouse(void)
{
#ifdef _WIN32
    ClipCursor(NULL);
    ReleaseCapture();
    while (ShowCursor(TRUE) < 0);
#endif
    mouse_grabbed = FALSE;
}

// ============================================================================
// Открытие / закрытие
// ============================================================================

void M_ToggleMenu_f(void)
{
    Con_Printf("M_ToggleMenu_f: state=%d\n", menu_state);

    if (menu_state != MENU_NONE)
    {
        M_CloseMenu();
        return;
    }

    menu_state = MENU_DEV;
    menu_cursor = 0;
    key_dest = key_menu;
    Menu_ReleaseMouse();
}

void M_CloseMenu(void)
{
    menu_state = MENU_NONE;
    menu_cursor = 0;
    key_dest = key_game;
    Menu_GrabMouse();
}

qboolean M_IsActive(void)
{
    return menu_state != MENU_NONE;
}

// ============================================================================
// Выполнение пункта
// ============================================================================

static void M_ExecuteItem(void)
{
    switch (menu_cursor)
    {
    case 0: // Load Map
        Cbuf_AddText("map 1stqmfmap\n");
        M_CloseMenu();
        break;
    case 1: // Close
        M_CloseMenu();
        break;
    case 2: // Quit
        Cbuf_AddText("quit\n");
        break;
    }
}

// ============================================================================
// Клавиши
// ============================================================================

void M_Keydown(int key)
{
    Con_Printf("M_Keydown: key=%d state=%d cursor=%d\n", key, menu_state, menu_cursor);

    if (menu_state == MENU_NONE)
        return;

    switch (key)
    {
    case K_ESCAPE:
        M_CloseMenu();
        break;

    case K_UPARROW:
        menu_cursor--;
        if (menu_cursor < 0)
            menu_cursor = DEV_MENU_COUNT - 1;
        break;

    case K_DOWNARROW:
        menu_cursor++;
        if (menu_cursor >= DEV_MENU_COUNT)
            menu_cursor = 0;
        break;

    case K_ENTER:
        M_ExecuteItem();
        break;
    }
}

// ============================================================================
// Отрисовка
// ============================================================================

static void M_DrawChar(int x, int y, int num, float scale)
{
    float row, col;
    float sz = 0.0625f;

    num &= 255;
    if (num == ' ' || !char_texture)
        return;

    row = (num >> 4) * sz;
    col = (num & 15) * sz;

    glBindTexture(GL_TEXTURE_2D, char_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int s = (int)(8 * scale);

    glBegin(GL_QUADS);
    glTexCoord2f(col, row);            glVertex2i(x, y);
    glTexCoord2f(col + sz, row);       glVertex2i(x + s, y);
    glTexCoord2f(col + sz, row + sz);  glVertex2i(x + s, y + s);
    glTexCoord2f(col, row + sz);       glVertex2i(x, y + s);
    glEnd();
}

static void M_DrawStr(int x, int y, const char* str, float scale)
{
    int step = (int)(8 * scale);
    while (*str)
    {
        M_DrawChar(x, y, *str, scale);
        str++;
        x += step;
    }
}

void M_Draw(void)
{
    if (menu_state == MENU_NONE)
        return;

    GL_Set2D();

    // === Затемнение ===
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.7f);

    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(glstate.width, 0);
    glVertex2i(glstate.width, glstate.height);
    glVertex2i(0, glstate.height);
    glEnd();

    glEnable(GL_TEXTURE_2D);

    int cx = glstate.width / 2;
    int cy = glstate.height / 2;

    // === Заголовок ===
    float title_scale = 2.5f;
    const char* title = "OpenCoreSrc";
    int title_w = (int)(strlen(title) * 8 * title_scale);
    glColor4f(1, 0.5f, 0, 1);  // оранжевый
    M_DrawStr(cx - title_w / 2, cy - 140, title, title_scale);

    // === Версия ===
    float ver_scale = 1.0f;
    const char* ver = "OpenCoreSrc v0 [pre-alpha]";
    int ver_w = (int)(strlen(ver) * 8 * ver_scale);
    glColor4f(0.5f, 0.5f, 0.5f, 1);
    M_DrawStr(cx - ver_w / 2, cy - 100, ver, ver_scale);

    // === Пункты ===
    float item_scale = 1.5f;
    int item_h = (int)(8 * item_scale) + 16;

    for (int i = 0; i < DEV_MENU_COUNT; i++)
    {
        int item_w = (int)(strlen(dev_menu_items[i]) * 8 * item_scale);
        int item_y = cy - 60 + i * item_h;

        if (i == menu_cursor)
        {
            // Подсветка
            glDisable(GL_TEXTURE_2D);
            glColor4f(1, 0.5f, 0, 0.2f);
            glBegin(GL_QUADS);
            glVertex2i(cx - 200, item_y - 4);
            glVertex2i(cx + 200, item_y - 4);
            glVertex2i(cx + 200, item_y + (int)(8 * item_scale) + 4);
            glVertex2i(cx - 200, item_y + (int)(8 * item_scale) + 4);
            glEnd();

            glEnable(GL_TEXTURE_2D);
            glColor4f(1, 0.8f, 0, 1);  // жёлтый

            // Стрелка
            M_DrawStr(cx - item_w / 2 - (int)(16 * item_scale), item_y, ">", item_scale);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, 1);  // серый
        }

        M_DrawStr(cx - item_w / 2, item_y, dev_menu_items[i], item_scale);
    }

    // === Подсказка внизу ===
    float hint_scale = 1.0f;
    const char* hint = "[UP/DOWN] Navigate  [ENTER] Select  [ESC] Close";
    int hint_w = (int)(strlen(hint) * 8 * hint_scale);
    glColor4f(0.4f, 0.4f, 0.4f, 1);
    M_DrawStr(cx - hint_w / 2, glstate.height - 30, hint, hint_scale);

    glColor4f(1, 1, 1, 1);
    glDisable(GL_BLEND);
}

// ============================================================================
// Инициализация
// ============================================================================

void M_Init(void)
{
    Cmd_AddCommand("togglemenu", M_ToggleMenu_f);

    // Стартуем с dev-меню
    menu_state = MENU_DEV;
    menu_cursor = 0;
    key_dest = key_menu;
}
