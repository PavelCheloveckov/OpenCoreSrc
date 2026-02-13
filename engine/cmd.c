// engine/cmd.c
#include "quakedef.h"
#include <cmd.h>
#ifdef _WIN32
#define strdup _strdup
#endif

char* Q_strdup(const char* src)
{
    char* dst;
    int len;
    if (!src) return NULL;
    len = strlen(src) + 1;
    dst = (char*)malloc(len);
    if (dst) memcpy(dst, src, len);
    return dst;
}

// Буфер команд
static char cmd_text_buf[MAX_CMD_BUFFER];
static int cmd_text_len = 0;

// Текущие аргументы
static int cmd_argc;
static char* cmd_argv[MAX_ARGS];
static char cmd_args[MAX_CMD_LINE];
static char cmd_tokenized[MAX_CMD_LINE * 2];

// Список команд
cmd_function_t* cmd_functions = NULL;

cmd_source_t cmd_source;

// Алиасы
typedef struct cmdalias_s {
    struct cmdalias_s* next;
    char name[MAX_QPATH];
    char* value;
} cmdalias_t;

static cmdalias_t* cmd_alias = NULL;

// ============================================================================
// Буфер команд
// ============================================================================

void Cbuf_Init(void)
{
    cmd_text_len = 0;
    cmd_text_buf[0] = 0;
}

void Cbuf_AddText(const char* text)
{
    int len = strlen(text);
    if (cmd_text_len + len >= MAX_CMD_BUFFER)
    {
        Con_Printf("Cbuf_AddText: overflow\n");
        return;
    }
    memcpy(cmd_text_buf + cmd_text_len, text, len);
    cmd_text_len += len;
    cmd_text_buf[cmd_text_len] = 0;
}

void Cbuf_InsertText(const char* text)
{
    char* temp;
    int len = strlen(text);

    if (cmd_text_len + len >= MAX_CMD_BUFFER)
    {
        Con_Printf("Cbuf_InsertText: overflow\n");
        return;
    }

    temp = (char*)malloc(cmd_text_len + 1);
    if (!temp) {
        Con_Printf("Cbuf_InsertText: out of memory\n");
        return;
    }
    memcpy(temp, cmd_text_buf, cmd_text_len + 1);
    memcpy(cmd_text_buf, text, len);
    memcpy(cmd_text_buf + len, temp, cmd_text_len + 1);
    cmd_text_len += len;
    free(temp);
}

void Cbuf_Execute(void)
{
    int i;
    char* text;
    char line[MAX_CMD_LINE];
    int quotes;

    while (cmd_text_len)
    {
        text = cmd_text_buf;

        quotes = 0;
        for (i = 0; i < cmd_text_len; i++)
        {
            if (text[i] == '"')
                quotes++;
            if (!(quotes & 1) && text[i] == ';')
                break;
            if (text[i] == '\n' || text[i] == '\r')
                break;
        }

        if (i >= MAX_CMD_LINE - 1)
            i = MAX_CMD_LINE - 1;

        memcpy(line, text, i);
        line[i] = 0;

        if (i == cmd_text_len)
            cmd_text_len = 0;
        else
        {
            i++;
            cmd_text_len -= i;
            memmove(text, text + i, cmd_text_len + 1);
        }

        Cmd_ExecuteString(line, src_command);
    }
}

// ============================================================================
// Разбор командной строки
// ============================================================================

void Cmd_TokenizeString(const char* text)
{
    int i;
    char* out;

    // Очистка предыдущих аргументов
    cmd_argc = 0;
    cmd_args[0] = 0;

    if (!text)
        return;

    out = cmd_tokenized;

    while (1)
    {
        // Пропуск пробелов
        while (*text && *text <= ' ' && *text != '\n')
            text++;

        if (*text == '\n' || *text == 0)
            break;

        // Сохранение аргументов после первого
        if (cmd_argc == 1)
        {
            int len;
            // Пропуск начальных пробелов
            while (*text && *text <= ' ' && *text != '\n')
                text++;
            len = strlen(text);
            if (len >= MAX_CMD_LINE)
                len = MAX_CMD_LINE - 1;
            strncpy(cmd_args, text, len);
            cmd_args[len] = 0;
        }

        if (cmd_argc >= MAX_ARGS)
            return;

        cmd_argv[cmd_argc] = out;
        cmd_argc++;

        // Обработка кавычек
        if (*text == '"')
        {
            text++;
            while (*text && *text != '"')
                *out++ = *text++;
            if (*text == '"')
                text++;
        }
        else
        {
            while (*text > ' ' && *text != '\n')
                *out++ = *text++;
        }
        *out++ = 0;
    }
}

int Cmd_Argc(void)
{
    return cmd_argc;
}

const char* Cmd_Argv(int arg)
{
    if (arg >= cmd_argc || arg < 0)
        return "";
    return cmd_argv[arg];
}

const char* Cmd_Args(void)
{
    return cmd_args;
}

// ============================================================================
// Команды
// ============================================================================

void Cmd_AddCommand(const char* name, xcommand_t function)
{
    cmd_function_t* cmd;

    // Проверка дубликата
    if (Cmd_Exists(name))
    {
        Con_Printf("Cmd_AddCommand: %s already defined\n", name);
        return;
    }

    cmd = (cmd_function_t*)malloc(sizeof(cmd_function_t));
    cmd->name = _strdup(name);
    cmd->function = function;
    cmd->next = cmd_functions;
    cmd_functions = cmd;
}

void Cmd_RemoveCommand(const char* name)
{
    cmd_function_t* cmd, ** prev;

    prev = &cmd_functions;
    while (1)
    {
        cmd = *prev;
        if (!cmd)
            return;
        if (!Q_stricmp(name, cmd->name))
        {
            *prev = cmd->next;
            free(cmd->name);
            free(cmd);
            return;
        }
        prev = &cmd->next;
    }
}

qboolean Cmd_Exists(const char* name)
{
    cmd_function_t* cmd;

    for (cmd = cmd_functions; cmd; cmd = cmd->next)
    {
        if (!Q_stricmp(name, cmd->name))
            return TRUE;
    }
    return FALSE;
}

// ============================================================================
// Алиасы
// ============================================================================

static void Cmd_Alias_f(void)
{
    cmdalias_t* a;
    char cmd[MAX_CMD_LINE];
    int i, len;
    const char* s;

    if (Cmd_Argc() == 1)
    {
        Con_Printf("Current alias commands:\n");
        for (a = cmd_alias; a; a = a->next)
            Con_Printf("%s : %s\n", a->name, a->value);
        return;
    }

    s = Cmd_Argv(1);
    if (strlen(s) >= MAX_QPATH)
    {
        Con_Printf("Alias name is too long\n");
        return;
    }

    // Поиск существующего
    for (a = cmd_alias; a; a = a->next)
    {
        if (!Q_stricmp(s, a->name))
        {
            free(a->value);
            break;
        }
    }

    if (!a)
    {
        a = (cmdalias_t*)malloc(sizeof(cmdalias_t));
        a->next = cmd_alias;
        cmd_alias = a;
        strcpy(a->name, s);
    }

    // Объединение аргументов
    cmd[0] = 0;
    for (i = 2; i < Cmd_Argc(); i++)
    {
        strcat(cmd, Cmd_Argv(i));
        if (i != Cmd_Argc() - 1)
            strcat(cmd, " ");
    }
    strcat(cmd, "\n");

    a->value = strdup(cmd);
}

// ============================================================================
// Исполнение
// ============================================================================

void Cmd_ExecuteString(const char* text, cmd_source_t src)
{
    cmd_function_t* cmd;
    cmdalias_t* a;

    cmd_source = src;

    Cmd_TokenizeString(text);

    if (!Cmd_Argc())
        return;

    // Проверка команд
    for (cmd = cmd_functions; cmd; cmd = cmd->next)
    {
        if (!Q_stricmp(Cmd_Argv(0), cmd->name))
        {
            if (cmd->function)
                cmd->function();
            return;
        }
    }

    // Проверка алиасов
    for (a = cmd_alias; a; a = a->next)
    {
        if (!Q_stricmp(Cmd_Argv(0), a->name))
        {
            Cbuf_InsertText(a->value);
            return;
        }
    }

    // Проверка cvar
    if (Cvar_Command())
        return;

    Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(0));
}

// ============================================================================
// Базовые команды
// ============================================================================

static void Cmd_Echo_f(void)
{
    int i;
    for (i = 1; i < Cmd_Argc(); i++)
        Con_Printf("%s ", Cmd_Argv(i));
    Con_Printf("\n");
}

static void Cmd_Exec_f(void)
{
    char* f;
    int len;
    char name[MAX_QPATH];

    if (Cmd_Argc() != 2)
    {
        Con_Printf("exec <filename> : execute a script file\n");
        return;
    }

    snprintf(name, sizeof(name), "%s", Cmd_Argv(1));

    f = (char*)COM_LoadFile(name, &len);
    if (!f)
    {
        Con_Printf("couldn't exec %s\n", name);
        return;
    }

    Con_Printf("execing %s\n", name);
    Cbuf_InsertText(f);
    free(f);
}

static void Cmd_Wait_f(void)
{
    // Команда wait - подождать один кадр
    // Реализуется в главном цикле
}

static void Cmd_Cmdlist_f(void)
{
    cmd_function_t* cmd;
    int count = 0;

    for (cmd = cmd_functions; cmd; cmd = cmd->next)
    {
        Con_Printf("%s\n", cmd->name);
        count++;
    }
    Con_Printf("%d commands\n", count);
}

void Cmd_Init(void)
{
    cmd_functions = NULL;
    Cbuf_Init();
    Cmd_AddCommand("echo", Cmd_Echo_f);
    Cmd_AddCommand("exec", Cmd_Exec_f);
    Cmd_AddCommand("alias", Cmd_Alias_f);
    Cmd_AddCommand("wait", Cmd_Wait_f);
    Cmd_AddCommand("cmdlist", Cmd_Cmdlist_f);
}

void Cmd_Shutdown(void)
{
    cmd_function_t* cmd, * next;
    cmdalias_t* a, * anext;

    for (cmd = cmd_functions; cmd; cmd = next)
    {
        next = cmd->next;
        free(cmd->name);
        free(cmd);
    }
    cmd_functions = NULL;

    for (a = cmd_alias; a; a = anext)
    {
        anext = a->next;
        free(a->value);
        free(a);
    }
    cmd_alias = NULL;
}
