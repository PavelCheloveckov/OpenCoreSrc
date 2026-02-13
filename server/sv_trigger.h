// server/sv_trigger.h
#ifndef SV_TRIGGER_H
#define SV_TRIGGER_H

#include "quakedef.h"

void SV_ClearTriggers(void);
void SV_AddTriggerTeleport(vec3_t mins, vec3_t maxs, const char* target);
void SV_AddTeleportDestination(vec3_t origin, float angle, const char* targetname);
void SV_CheckTriggers(vec3_t player_origin);

// Геттеры для отладки
int SV_GetTriggerCount(void);
int SV_GetDestinationCount(void);

#endif
