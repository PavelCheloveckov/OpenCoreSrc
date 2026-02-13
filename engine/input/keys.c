// engine/input/keys.c
#include "quakedef.h"
#include "common.h"
#include "cmd.h"
#include "console.h"
#include "keys.h"

keydest_t key_dest = key_game;

static qboolean keydown[256];
static char* keybindings[256];

qboolean Key_IsDown(int key)
{
    if (key < 0 || key >= 256)
        return FALSE;
    return keydown[key];
}

typedef struct keyname_s {
    const char* name;
    int keynum;
} keyname_t;

static keyname_t keynames[] = {
    {"TAB", K_TAB},
    {"ENTER", K_ENTER},
    {"ESCAPE", K_ESCAPE},
    {"SPACE", K_SPACE},
    {"BACKSPACE", K_BACKSPACE},
    {"UPARROW", K_UPARROW},
    {"DOWNARROW", K_DOWNARROW},
    {"LEFTARROW", K_LEFTARROW},
    {"RIGHTARROW", K_RIGHTARROW},
    {"ALT", K_ALT},
    {"CTRL", K_CTRL},
    {"SHIFT", K_SHIFT},
    {"F1", K_F1},
    {"F2", K_F2},
    {"F3", K_F3},
    {"F4", K_F4},
    {"F5", K_F5},
    {"F6", K_F6},
    {"F7", K_F7},
    {"F8", K_F8},
    {"F9", K_F9},
    {"F10", K_F10},
    {"F11", K_F11},
    {"F12", K_F12},
    {"INS", K_INS},
    {"DEL", K_DEL},
    {"PGDN", K_PGDN},
    {"PGUP", K_PGUP},
    {"HOME", K_HOME},
    {"END", K_END},
    {"PAUSE", K_PAUSE},
    {"MOUSE1", K_MOUSE1},
    {"MOUSE2", K_MOUSE2},
    {"MOUSE3", K_MOUSE3},
    {"MOUSE4", K_MOUSE4},
    {"MOUSE5", K_MOUSE5},
    {"MWHEELUP", K_MWHEELUP},
    {"MWHEELDOWN", K_MWHEELDOWN},
    {NULL, 0}
};

// ============================================================================
// Key string conversion
// ============================================================================

int Key_StringToKeynum(const char* str)
{
    keyname_t* kn;

    if (!str || !str[0])
        return -1;

    // Одиночный символ
    if (!str[1])
        return (unsigned char)str[0];

    // Поиск в таблице
    for (kn = keynames; kn->name; kn++)
    {
        if (!Q_stricmp(str, kn->name))
            return kn->keynum;
    }

    return -1;
}

const char* Key_KeynumToString(int keynum)
{
    static char tinystr[2];
    keyname_t* kn;

    if (keynum < 0)
        return "<KEY NOT FOUND>";

    // Печатный символ
    if (keynum > 32 && keynum < 127)
    {
        tinystr[0] = keynum;
        tinystr[1] = 0;
        return tinystr;
    }

    // Поиск в таблице
    for (kn = keynames; kn->name; kn++)
    {
        if (keynum == kn->keynum)
            return kn->name;
    }

    return "<UNKNOWN KEYNUM>";
}

// ============================================================================
// Bindings
// ============================================================================

void Key_SetBinding(int keynum, const char* binding)
{
    if (keynum < 0 || keynum >= 256)
        return;

    if (keybindings[keynum])
    {
        free(keybindings[keynum]);
        keybindings[keynum] = NULL;
    }

    if (binding && binding[0])
    {
        keybindings[keynum] = Q_strdup(binding);
    }
}

const char* Key_GetBinding(int keynum)
{
    if (keynum < 0 || keynum >= 256)
        return NULL;
    return keybindings[keynum];
}

static void Key_Bind_f(void)
{
    int c, key;
    char cmd[1024];

    c = Cmd_Argc();

    if (c < 2)
    {
        Con_Printf("bind <key> [command] : attach a command to a key\n");
        return;
    }

    key = Key_StringToKeynum(Cmd_Argv(1));
    if (key == -1)
    {
        Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    if (c == 2)
    {
        if (keybindings[key])
            Con_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[key]);
        else
            Con_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
        return;
    }

    // Объединение аргументов
    cmd[0] = 0;
    for (int i = 2; i < c; i++)
    {
        strcat(cmd, Cmd_Argv(i));
        if (i != c - 1)
            strcat(cmd, " ");
    }

    Key_SetBinding(key, cmd);
}

static void Key_Unbind_f(void)
{
    int key;

    if (Cmd_Argc() != 2)
    {
        Con_Printf("unbind <key> : remove commands from a key\n");
        return;
    }

    key = Key_StringToKeynum(Cmd_Argv(1));
    if (key == -1)
    {
        Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
        return;
    }

    Key_SetBinding(key, NULL);
}

static void Key_Unbindall_f(void)
{
    for (int i = 0; i < 256; i++)
    {
        Key_SetBinding(i, NULL);
    }
}

static void Key_Bindlist_f(void)
{
    for (int i = 0; i < 256; i++)
    {
        if (keybindings[i])
        {
            Con_Printf("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
        }
    }
}

void Key_WriteBindings(FILE* f)
{
    for (int i = 0; i < 256; i++)
    {
        if (keybindings[i])
        {
            fprintf(f, "bind \"%s\" \"%s\"\n",
                Key_KeynumToString(i), keybindings[i]);
        }
    }
}

// ============================================================================
// Key events
// ============================================================================

void Key_Event(int key, qboolean down)
{
    if (down && (key == K_ESCAPE || key == 27))
        Con_Printf("KEY_EVENT: ESC detected, key=%d K_ESCAPE=%d key_dest=%d\n", key, K_ESCAPE, key_dest);

    const char* binding;
    char cmd[1024];

    if (key < 0 || key >= 256)
        return;

    keydown[key] = down;

    // Консоль всегда на `
    if (key == '`' || key == '~')
    {
        if (down)
        {
            Con_ToggleConsole_f();
            Con_Printf("Toggled! Current state: %d\n", key_dest);
        }
        return;
    }

    // Escape
    if (key == K_ESCAPE && down)
    {
        if (key_dest == key_console)
        {
            Con_ToggleConsole_f();
        }
        else if (key_dest == key_menu)
        {
            M_Keydown(key);
        }
        else
        {
            M_ToggleMenu_f();
        }
        return;
    }

    if (key_dest == key_menu)
    {
        if (down)
            M_Keydown(key);
        return;
    }

    // Консольный ввод
    if (key_dest == key_console)
    {
        if (down)
            Key_Console(key);
        return;
    }

    // Игровой ввод - выполнение бинда
    binding = keybindings[key];
    if (binding)
    {
        if (binding[0] == '+')
        {
            // +команда - с номером клавиши
            snprintf(cmd, sizeof(cmd), "%s %d\n",
                down ? binding : va("-%s", binding + 1), key);
            Cbuf_AddText(cmd);
        }
        else if (down)
        {
            // Обычная команда - только при нажатии
            Cbuf_AddText(binding);
            Cbuf_AddText("\n");
        }
    }
}

void Key_ClearStates(void)
{
    memset(keydown, 0, sizeof(keydown));
}

// ============================================================================
// Init
// ============================================================================

void Key_Init(void)
{
    memset(keydown, 0, sizeof(keydown));
    memset(keybindings, 0, sizeof(keybindings));

    Cmd_AddCommand("bind", Key_Bind_f);
    Cmd_AddCommand("unbind", Key_Unbind_f);
    Cmd_AddCommand("unbindall", Key_Unbindall_f);
    Cmd_AddCommand("bindlist", Key_Bindlist_f);

    // Дефолтные бинды
    Key_SetBinding('w', "+forward");
    Key_SetBinding('s', "+back");
    Key_SetBinding('a', "+moveleft");
    Key_SetBinding('d', "+moveright");
    Key_SetBinding(K_LEFTARROW, "+left");
    Key_SetBinding(K_RIGHTARROW, "+right");
    Key_SetBinding(K_UPARROW, "+lookup");
    Key_SetBinding(K_DOWNARROW, "+lookdown");
    Key_SetBinding(K_SPACE, "+jump");
    Key_SetBinding(K_CTRL, "+duck");
    Key_SetBinding(K_MOUSE1, "+attack");
    Key_SetBinding(K_MOUSE2, "+attack2");
    Key_SetBinding('e', "+use");
    Key_SetBinding('r', "+reload");
}
