// engine/cvar.c
#include "quakedef.h"
#include "common.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"

#ifdef _WIN32
#define strdup _strdup
#endif

static cvar_t* cvar_vars = NULL;

void Cvar_Init(void)
{
    cvar_vars = NULL;
    Cmd_AddCommand("cvarlist", Cvar_List_f);
}

void Cvar_Shutdown(void)
{
    cvar_t* var, * next;

    for (var = cvar_vars; var; var = next)
    {
        next = var->next;
        if (var->string)
            free(var->string);
        free(var);
    }
    cvar_vars = NULL;
}

cvar_t* Cvar_Find(const char* name)
{
    cvar_t* var;

    for (var = cvar_vars; var; var = var->next)
    {
        if (!Q_stricmp(name, var->name))
            return var;
    }
    return NULL;
}

cvar_t* Cvar_Get(const char* name, const char* value, int flags)
{
    cvar_t* var;

    var = Cvar_Find(name);
    if (var)
    {
        var->flags |= flags;
        return var;
    }

    var = (cvar_t*)malloc(sizeof(cvar_t));
    memset(var, 0, sizeof(cvar_t));

    var->name = strdup(name);
    var->string = strdup(value);
    var->value = (float)atof(value);
    var->flags = flags;

    var->next = cvar_vars;
    cvar_vars = var;

    return var;
}

void Cvar_DirectSet(cvar_t* var, const char* value)
{
    if (!var)
        return;

    if (var->string)
        free(var->string);

    var->string = strdup(value);
    var->value = (float)atof(value);
}

void Cvar_Set(const char* name, const char* value)
{
    cvar_t* var = Cvar_Find(name);
    if (!var)
    {
        var = Cvar_Get(name, value, 0);
        return;
    }
    Cvar_DirectSet(var, value);
}

void Cvar_SetValue(const char* name, float value)
{
    char val[32];
    if (value == (int)value)
        snprintf(val, sizeof(val), "%d", (int)value);
    else
        snprintf(val, sizeof(val), "%f", value);
    Cvar_Set(name, val);
}

float Cvar_VariableValue(const char* name)
{
    cvar_t* var = Cvar_Find(name);
    if (!var)
        return 0;
    return var->value;
}

const char* Cvar_VariableString(const char* name)
{
    cvar_t* var = Cvar_Find(name);
    if (!var)
        return "";
    return var->string;
}

qboolean Cvar_Command(void)
{
    cvar_t* var;

    var = Cvar_Find(Cmd_Argv(0));
    if (!var)
        return FALSE;

    if (Cmd_Argc() == 1)
    {
        Con_Printf("\"%s\" is \"%s\"\n", var->name, var->string);
        return TRUE;
    }

    Cvar_DirectSet(var, Cmd_Argv(1));
    return TRUE;
}

void Cvar_WriteVariables(FILE* f)
{
    cvar_t* var;

    for (var = cvar_vars; var; var = var->next)
    {
        if (var->flags & FCVAR_ARCHIVE)
            fprintf(f, "%s \"%s\"\n", var->name, var->string);
    }
}

void Cvar_List_f(void)
{
    cvar_t* var;
    int count = 0;

    for (var = cvar_vars; var; var = var->next)
    {
        Con_Printf("%c%c%c %s \"%s\"\n",
            (var->flags & FCVAR_ARCHIVE) ? 'A' : ' ',
            (var->flags & FCVAR_SERVER) ? 'S' : ' ',
            (var->flags & FCVAR_USERINFO) ? 'U' : ' ',
            var->name, var->string);
        count++;
    }
    Con_Printf("%d cvars\n", count);
}
