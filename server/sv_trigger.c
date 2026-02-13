// server/sv_trigger.c
#include "quakedef.h"
#include "server.h"
#include "mathlib.h"
#include "common.h"
#include "console.h"
#include "client.h"

// ============================================================================
// Структуры для триггеров
// ============================================================================

#define MAX_TRIGGERS        128
#define MAX_TELEPORTS       64

typedef struct trigger_teleport_s {
    vec3_t mins;
    vec3_t maxs;
    char target[64];        // Имя точки назначения
    qboolean active;
} trigger_teleport_t;

typedef struct teleport_dest_s {
    vec3_t origin;
    float angle;
    char targetname[64];
    qboolean active;
} teleport_dest_t;

static trigger_teleport_t triggers[MAX_TRIGGERS];
static int num_triggers = 0;

static teleport_dest_t destinations[MAX_TELEPORTS];
static int num_destinations = 0;

// ============================================================================
// Очистка
// ============================================================================

void SV_ClearTriggers(void)
{
    memset(triggers, 0, sizeof(triggers));
    memset(destinations, 0, sizeof(destinations));
    num_triggers = 0;
    num_destinations = 0;
}

// ============================================================================
// Добавление триггера телепорта
// ============================================================================

void SV_AddTriggerTeleport(vec3_t mins, vec3_t maxs, const char* target)
{
    if (num_triggers >= MAX_TRIGGERS)
    {
        Con_Printf("Too many triggers!\n");
        return;
    }

    trigger_teleport_t* t = &triggers[num_triggers++];
    VectorCopy(mins, t->mins);
    VectorCopy(maxs, t->maxs);
    Q_strncpy(t->target, target, sizeof(t->target));
    t->active = TRUE;

    Con_Printf("Added trigger_teleport -> %s\n", target);
}

// ============================================================================
// Добавление точки телепорта
// ============================================================================

void SV_AddTeleportDestination(vec3_t origin, float angle, const char* targetname)
{
    if (num_destinations >= MAX_TELEPORTS)
    {
        Con_Printf("Too many teleport destinations!\n");
        return;
    }

    teleport_dest_t* d = &destinations[num_destinations++];
    VectorCopy(origin, d->origin);
    d->angle = angle;
    Q_strncpy(d->targetname, targetname, sizeof(d->targetname));
    d->active = TRUE;

    Con_Printf("Added info_teleport_destination: %s at (%f %f %f)\n",
        targetname, origin[0], origin[1], origin[2]);
}

// ============================================================================
// Поиск точки назначения по имени
// ============================================================================

static teleport_dest_t* SV_FindTeleportDestination(const char* targetname)
{
    for (int i = 0; i < num_destinations; i++)
    {
        if (destinations[i].active &&
            !Q_stricmp(destinations[i].targetname, targetname))
        {
            return &destinations[i];
        }
    }
    return NULL;
}

// ============================================================================
// Проверка: точка внутри бокса?
// ============================================================================

static qboolean PointInBox(vec3_t point, vec3_t mins, vec3_t maxs)
{
    if (point[0] < mins[0] || point[0] > maxs[0]) return FALSE;
    if (point[1] < mins[1] || point[1] > maxs[1]) return FALSE;
    if (point[2] < mins[2] || point[2] > maxs[2]) return FALSE;
    return TRUE;
}

// ============================================================================
// Проверка всех триггеров
// ============================================================================

void SV_CheckTriggers(vec3_t player_origin)
{
    static double last_teleport_time = 0;

    // Защита от мгновенного повторного телепорта
    if (cls.realtime - last_teleport_time < 0.5)
        return;

    for (int i = 0; i < num_triggers; i++)
    {
        trigger_teleport_t* t = &triggers[i];

        if (!t->active)
            continue;

        // Проверяем, находится ли игрок внутри триггера
        if (PointInBox(player_origin, t->mins, t->maxs))
        {
            // Ищем точку назначения
            teleport_dest_t* dest = SV_FindTeleportDestination(t->target);

            if (dest)
            {
                Con_Printf("TELEPORT! -> %s\n", t->target);

                // Телепортируем игрока
                VectorCopy(dest->origin, cl.vieworg);
                cl.viewangles[1] = dest->angle;
                cl.viewangles[0] = 0;  // Сброс pitch

                // Небольшой подъём чтобы не застрять в полу
                cl.vieworg[2] += 16;

                last_teleport_time = cls.realtime;

                // Звук телепорта
                // S_StartSound(cl.vieworg, 0, 0, "misc/r_tele1.wav", 1.0f, 1.0f);
            }
            else
            {
                Con_Printf("Teleport destination '%s' not found!\n", t->target);
            }

            break;  // Только один телепорт за раз
        }
    }
}

int SV_GetTriggerCount(void) { return num_triggers; }
int SV_GetDestinationCount(void) { return num_destinations; }
