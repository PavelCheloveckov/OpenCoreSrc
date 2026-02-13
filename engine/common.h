// engine/common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <quakedef.h>

// String functions
int Q_stricmp(const char* s1, const char* s2);
int Q_strnicmp(const char* s1, const char* s2, int n);
void Q_strncpy(char* dest, const char* src, int count);
char* Q_strdup(const char* s);

// File functions
void* COM_LoadFile(const char* path, int* length);
void COM_FreeFile(void* buffer);
int COM_FileExists(const char* path);
void COM_CreatePath(const char* path);

// String formatting
char* va(const char* format, ...);

// Parsing
char* COM_Parse(char* data);
extern char com_token[1024];
extern qboolean com_eof;
extern byte host_basepal[768];

// Byte order
short LittleShort(short l);
int LittleLong(int l);
float LittleFloat(float f);
short BigShort(short l);
int BigLong(int l);
float BigFloat(float f);

// CRC
unsigned short CRC_Block(byte* start, int count);

// Error handling
void Sys_Error(const char* error, ...);
void Con_Printf(const char* fmt, ...);
void Con_DPrintf(const char* fmt, ...);

// Time
double Sys_FloatTime(void);
void Sys_Sleep(int msec);

// Memory
void* Mem_Alloc(size_t size);
void* Mem_Realloc(void* ptr, size_t size);
void Mem_Free(void* ptr);


// Info strings
char* Info_ValueForKey(const char* s, const char* key);
void Info_SetValueForKey(char* s, const char* key, const char* value, int maxsize);
void Info_RemoveKey(char* s, const char* key);


extern double host_time;

#endif // COMMON_H
