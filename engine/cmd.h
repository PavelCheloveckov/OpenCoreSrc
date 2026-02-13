// engine/cmd.h
#ifndef CMD_H
#define CMD_H
#include <quakedef.h>
#ifdef _WIN32
#define strdup _strdup
#endif
#define MAX_ARGS        80
#define MAX_CMD_BUFFER  16384
#define MAX_CMD_LINE    1024

typedef void (*xcommand_t)(void);

typedef struct cmd_function_s {
    struct cmd_function_s* next;
    char* name;
    xcommand_t function;
    int flags;
} cmd_function_t;

// Источники команд
typedef enum {
    src_client,     // от клиента
    src_command     // от консоли/сервера
} cmd_source_t;

extern cmd_source_t cmd_source;

void Cmd_Init(void);
void Cmd_Shutdown(void);
void Cmd_AddCommand(const char* name, xcommand_t function);
void Cmd_RemoveCommand(const char* name);
qboolean Cmd_Exists(const char* name);
int Cmd_Argc(void);
const char* Cmd_Argv(int arg);
const char* Cmd_Args(void);
void Cmd_TokenizeString(const char* text);
void Cmd_ExecuteString(const char* text, cmd_source_t src);
void Cbuf_Init(void);
void Cbuf_AddText(const char* text);
void Cbuf_InsertText(const char* text);
void Cbuf_Execute(void);
qboolean Cvar_Command(void);

#endif // CMD_H
