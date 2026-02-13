#pragma once
#include "quakedef.h"


	void PAK_Init(void);
	void PAK_InitForDir(const char* gamedir);
	void PAK_Shutdown(void);
	int PAK_GetNumPaks(void);

	byte* PAK_LoadFile(const char* path, int* len);

	int PAK_EnumFiles(void (*cb)(const char* name, int filelen, void* user), void* user);
	const char* PAK_GetGameDir(void);
