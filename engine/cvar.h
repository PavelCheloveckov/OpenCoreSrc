#ifndef CVAR_H
#define CVAR_H

#include "quakedef.h"

#define FCVAR_ARCHIVE       1
#define FCVAR_USERINFO      2
#define FCVAR_SERVER        4

typedef struct cvar_s {
    char* name;
    char* string;
    int flags;
    float value;
    struct cvar_s* next;
} cvar_t;

void Cvar_Init(void);
cvar_t* Cvar_Get(const char* name, const char* value, int flags);
cvar_t* Cvar_Find(const char* name);
void Cvar_Set(const char* name, const char* value);
void Cvar_SetValue(const char* name, float value);
float Cvar_VariableValue(const char* name);
const char* Cvar_VariableString(const char* name);
void Cvar_List_f(void);

#endif // CVAR_H
