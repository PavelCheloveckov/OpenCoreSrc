// server/sv_main.c
#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "mathlib.h"
#include "server.h"
#include "progs.h"
#include "bsp.h"
#include "mdl.h"
#include "client.h"
#include "sv_trigger.h"
#include <gl_local.h>

server_t sv;
server_static_t svs;

static cvar_t* sv_maxclients;
static cvar_t* sv_maxrate;
static cvar_t* sv_minrate;
static cvar_t* sv_timeout;
static cvar_t* sv_cheats;
cvar_t* sv_gravity;
cvar_t* sv_maxspeed;
cvar_t* sv_accelerate;
cvar_t* sv_friction;
cvar_t* sv_stopspeed;
cvar_t* sv_stepsize;

DLL_FUNCTIONS gEntityInterface;

void SV_Noclip_f(void);
void SV_Setpos_f(void);
void R_SetEntityAnim_f(void);
void Test_PointContents_f(void);

typedef struct {
    const char* classname;
    const char* modelpath;
} entity_model_map_t;

static entity_model_map_t entity_models[] = {
    // Монстры
    {"monster_army",        "progs/soldier.mdl"},
    {"monster_dog",         "progs/dog.mdl"},
    {"monster_ogre",        "progs/ogre.mdl"},
    {"monster_knight",      "progs/knight.mdl"},
    {"monster_zombie",      "progs/zombie.mdl"},
    {"monster_wizard",      "progs/wizard.mdl"},
    {"monster_demon1",      "progs/demon.mdl"},
    {"monster_shambler",    "progs/shambler.mdl"},
    {"monster_boss",        "progs/boss.mdl"},
    {"monster_enforcer",    "progs/enforcer.mdl"},
    {"monster_hell_knight", "progs/hknight.mdl"},
    {"monster_shalrath",    "progs/shalrath.mdl"},
    {"monster_tarbaby",     "progs/tarbaby.mdl"},
    {"monster_fish",        "progs/fish.mdl"},
    {"monster_oldone",      "progs/oldone.mdl"},

    // Оружие
    {"weapon_supershotgun",     "progs/g_shot.mdl"},
    {"weapon_nailgun",          "progs/g_nail.mdl"},
    {"weapon_supernailgun",     "progs/g_nail2.mdl"},
    {"weapon_grenadelauncher",  "progs/g_rock.mdl"},
    {"weapon_rocketlauncher",   "progs/g_rock2.mdl"},
    {"weapon_lightning",        "progs/g_light.mdl"},

    // Предметы
    {"item_health",             "progs/b_bh25.bsp"},    // или b_bh10, b_bh100
    {"item_armor1",             "progs/armor.mdl"},
    {"item_armor2",             "progs/armor.mdl"},
    {"item_armorInv",           "progs/armor.mdl"},
    {"item_shells",             "progs/b_shell0.bsp"},  // или b_shell1
    {"item_spikes",             "progs/b_nail0.bsp"},   // или b_nail1
    {"item_rockets",            "progs/b_rock0.bsp"},   // или b_rock1
    {"item_cells",              "progs/b_batt0.bsp"},   // или b_batt1
    {"item_artifact_invulnerability", "progs/invulner.mdl"},
    {"item_artifact_envirosuit",      "progs/suit.mdl"},
    {"item_artifact_invisibility",    "progs/invisibl.mdl"},
    {"item_artifact_super_damage",    "progs/quaddama.mdl"},

    // Ключи
    {"item_key1",               "progs/w_s_key.mdl"},
    {"item_key2",               "progs/w_g_key.mdl"},

    {NULL, NULL}  // Конец списка
};

static const char* GetModelForClassname(const char* classname)
{
    entity_model_map_t* map = entity_models;

    while (map->classname)
    {
        if (!strcmp(classname, map->classname))
            return map->modelpath;
        map++;
    }

    return NULL;
}

// ============================================================================
// Доп. функции
// ============================================================================

static void SV_DropToFloor(render_entity_t* ent)
{
    vec3_t p;
    float mins_z;
    int step;

    if (!ent || !ent->model)
        return;

    mins_z = ent->model->frame_mins[ent->frame][2];

    p[0] = ent->origin[0];
    p[1] = ent->origin[1];

    // если ноги в solid - поднимаем немного вверх
    for (step = 0; step < 128; step++)
    {
        p[2] = ent->origin[2] + mins_z;
        if (SV_PointContents(p) != CONTENTS_SOLID)
            break;
        ent->origin[2] += 1.0f;
    }

    // опускаем вниз до пола
    for (step = 0; step < 1024; step++)
    {
        p[2] = ent->origin[2] + mins_z - 1.0f;
        if (SV_PointContents(p) == CONTENTS_SOLID)
        {
            ent->origin[2] += 1.0f;
            break;
        }
        ent->origin[2] -= 1.0f;
    }
}

static qboolean SV_TestEntityPosition(edict_t* ent)
{
    trace_t tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent);
    return (tr.startsolid || tr.allsolid);
}

static void SV_UnstickPlayer(edict_t* ent)
{
    int i;
    for (i = 0; i < 128; i++)
    {
        if (!SV_TestEntityPosition(ent))
            return;
        ent->v.origin[2] += 1.0f;
    }
}

static void SV_DropPlayerToFloor(edict_t* ent)
{
    vec3_t end;
    trace_t tr;

    VectorCopy(ent->v.origin, end);
    end[2] -= 256.0f;

    tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
    VectorCopy(tr.endpos, ent->v.origin);

    if (tr.fraction < 1.0f)
    {
        ent->v.origin[2] += 1.0f;
        ent->v.flags |= FL_ONGROUND;
        ent->v.groundentity = tr.ent;
    }
}

// ============================================================================
// Управление сущностями
// ============================================================================
// Функция парсинга сущностей из строки (LUMP_ENTITIES)

void ED_LoadFromFile(char* data)
{
    char key[256];
    char value[256];
    char classname[64] = "";
    char targetname[64] = "";
    char target[64] = "";
    vec3_t origin = { 0, 0, 0 };
    vec3_t mins = { 0, 0, 0 };
    vec3_t maxs = { 0, 0, 0 };
    float angle = 0;
    int brush_model = -1;

    qboolean player_spawned = FALSE;

    if (!data) return;

    // Очищаем старые триггеры
    SV_ClearTriggers();

    while (1)
    {
        // Ищем начало сущности '{'
        data = COM_Parse(data);
        if (!data) break;
        if (com_token[0] != '{') continue;

        // Сброс для новой сущности
        classname[0] = 0;
        targetname[0] = 0;
        target[0] = 0;
        VectorClear(origin);
        VectorClear(mins);
        VectorClear(maxs);
        angle = 0;
        brush_model = -1;

        // Парсим поля
        while (1)
        {
            data = COM_Parse(data);
            if (!data) break;
            if (com_token[0] == '}') break;

            // Ключ
            strcpy(key, com_token);

            // Значение
            data = COM_Parse(data);
            if (!data) break;
            strcpy(value, com_token);

            // Обработка полей
            if (!strcmp(key, "classname"))
                strcpy(classname, value);
            else if (!strcmp(key, "targetname"))
                strcpy(targetname, value);
            else if (!strcmp(key, "target"))
                strcpy(target, value);
            else if (!strcmp(key, "origin"))
                sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
            else if (!strcmp(key, "angle"))
                angle = (float)atof(value);
            else if (!strcmp(key, "model"))
            {
                // "*1", "*2" etc - brush models
                if (value[0] == '*')
                    brush_model = atoi(value + 1);
            }
        }

        // === Обработка типов сущностей ===

        // Точка спавна игрока
        if (!strcmp(classname, "info_player_start"))
        {
            if (!player_spawned)
            {
                edict_t* player = EDICT_NUM(1);

                // Ставим origin
                VectorCopy(origin, player->v.origin);

                // Углы
                VectorClear(player->v.angles);
                player->v.angles[1] = angle;
                VectorCopy(player->v.angles, cl.viewangles);
                VectorCopy(player->v.angles, svs.clients[0].lastcmd.viewangles);

                // Параметры игрока
                player->v.movetype = MOVETYPE_WALK;
                player->v.solid = SOLID_SLIDEBOX;
                player->v.gravity = 1.0f;

                player->v.mins[0] = -16; player->v.mins[1] = -16; player->v.mins[2] = -24;
                player->v.maxs[0] = 16;  player->v.maxs[1] = 16;  player->v.maxs[2] = 32;

                player->v.view_ofs[0] = 0;
                player->v.view_ofs[1] = 0;
                player->v.view_ofs[2] = 22;

                VectorClear(player->v.velocity);

                // === ПОКА НЕ ТРОГАЕМ ПОЗИЦИЮ! ===
                // Коллизии (SV_Move) не работают, поэтому:
                // НЕ вызываем SV_Move для поиска пола
                // НЕ вызываем SV_UnstickPlayer
                // НЕ вызываем SV_DropPlayerToFloor

                // Просто ставим на спавн + линкуем
                player->v.flags |= FL_ONGROUND;  // Считаем что на земле

                SV_LinkEdict(player, TRUE);

                player_spawned = TRUE;

                Con_Printf("Player spawn: %f %f %f\n", origin[0], origin[1], origin[2]);
            }
        }
        // Монстры, оружие, предметы - используем таблицу маппинга
        else if (!strncmp(classname, "monster_", 8) ||
            !strncmp(classname, "item_", 5) ||
            !strncmp(classname, "weapon_", 7))
        {
            const char* modelpath = GetModelForClassname(classname);

            if (modelpath)
            {
                aliasmodel_t* mdl = Mod_LoadAliasModel(modelpath);
                if (mdl)
                {
                    render_entity_t* rent = R_AllocRenderEntity();
                    if (rent)
                    {
                        rent->model = mdl;
                        VectorCopy(origin, rent->origin);

                        rent->angles[1] = angle;
                        rent->frame = 0;
                        rent->skin = 0;
                        rent->current_anim = ANIM_IDLE;  // начинаем с idle
                        rent->anim_time = 0;  // инициализируется при первом обновлении

                        // === ФИЗИКА ===
                        rent->frame = 0;
                        SV_DropToFloor(rent);
                        rent->gravity_scale = 0.0f;
                        rent->onground = true;

                        Con_Printf("Spawned %s at %.0f %.0f %.0f\n",
                            classname, origin[0], origin[1], origin[2]);
                    }
                }
                else
                {
                    Con_Printf("Model not found for %s: %s\n", classname, modelpath);
                }
            }
            else
            {
                Con_Printf("Unknown entity: %s\n", classname);
            }
        }
        // Точка назначения телепорта
        else if (!strcmp(classname, "info_teleport_destination"))
        {
            SV_AddTeleportDestination(origin, angle, targetname);
        }
        // Триггер телепорта
        else if (!strcmp(classname, "trigger_teleport"))
        {
            // Для brush-based триггеров нужно получить bbox из модели
            if (brush_model >= 0 && sv.worldmodel)
            {
                // Берём размеры из submodel
                dmodel_t* mod = &sv.worldmodel->submodels[brush_model];
                VectorCopy(mod->mins, mins);
                VectorCopy(mod->maxs, maxs);

                SV_AddTriggerTeleport(mins, maxs, target);
            }
            else
            {
                // Fallback: используем origin с дефолтным размером
                mins[0] = origin[0] - 32;
                mins[1] = origin[1] - 32;
                mins[2] = origin[2] - 32;
                maxs[0] = origin[0] + 32;
                maxs[1] = origin[1] + 32;
                maxs[2] = origin[2] + 32;

                SV_AddTriggerTeleport(mins, maxs, target);
            }
        }
    }

    Con_Printf("Loaded %d triggers, %d teleport destinations\n",
        SV_GetTriggerCount(), SV_GetDestinationCount());
}

edict_t* ED_Alloc(void)
{
    int i;
    edict_t* e;

    // Поиск свободного слота после клиентов
    for (i = svs.maxclients + 1; i < sv.num_edicts; i++)
    {
        e = EDICT_NUM(i);
        if (e->free && (sv.time - e->freetime) > 0.5f)
        {
            ED_ClearEdict(e);
            return e;
        }
    }

    // Расширение массива если нужно
    if (sv.num_edicts >= sv.max_edicts)
    {
        Sys_Error("ED_Alloc: no free edicts");
    }

    e = EDICT_NUM(sv.num_edicts);
    sv.num_edicts++;
    ED_ClearEdict(e);

    return e;
}

void ED_Free(edict_t* ed)
{
    // Отвязка от area
    SV_UnlinkEdict(ed);

    ed->free = TRUE;
    ed->freetime = sv.time;
    ed->v.classname = 0;
    ed->v.model = 0;
    ed->v.takedamage = 0;
    ed->v.modelindex = 0;
    ed->v.colormap = 0;
    ed->v.skin = 0;
    ed->v.frame = 0;
    ed->v.solid = SOLID_NOT;
    ed->v.flags = 0;

    VectorClear(ed->v.origin);
    VectorClear(ed->v.angles);
    VectorClear(ed->v.mins);
    VectorClear(ed->v.maxs);

    if (ed->pvPrivateData)
    {
        free(ed->pvPrivateData);
        ed->pvPrivateData = NULL;
    }
}

void ED_ClearEdict(edict_t* e)
{
    memset(&e->v, 0, sizeof(entvars_t));
    e->free = FALSE;
    e->serialnumber++;
}

int NUM_FOR_EDICT(const edict_t* e)
{
    int index = e - sv.edicts;
    if (index < 0 || index >= sv.max_edicts)
        Sys_Error("NUM_FOR_EDICT: bad pointer");
    return index;
}

void SV_ClearEdicts(void) {
    memset(sv.edicts, 0, sv.max_edicts * sizeof(edict_t));
    sv.num_edicts = 1; // worldspawn
}

edict_t* EDICT_NUM(int num) {
    if (num < 0 || num >= sv.max_edicts) {
        Sys_Error("EDICT_NUM: bad number %d", num);
    }
    return &sv.edicts[num];
}

// ============================================================================
// Инициализация сервера
// ============================================================================

void SV_CheckWaterTransition(edict_t* ent) {}
void SV_PushMove(edict_t* ent, vec3_t move, float time) {}
void SV_ClearWorld(void) {}
void SV_ReadPackets(void) {}
void SV_SendClientMessages(void) {}
void Test_BSPTree_f(void);
void Test_Leafs_f(void);

void SV_Init(void)
{
    memset(&sv, 0, sizeof(sv));
    memset(&svs, 0, sizeof(svs));

    // Регистрация cvars
    sv_maxclients = Cvar_Get("maxplayers", "16", FCVAR_SERVER);
    sv_maxrate = Cvar_Get("sv_maxrate", "20000", FCVAR_SERVER);
    sv_minrate = Cvar_Get("sv_minrate", "0", FCVAR_SERVER);
    sv_timeout = Cvar_Get("sv_timeout", "65", 0);
    sv_cheats = Cvar_Get("sv_cheats", "0", FCVAR_SERVER);
    sv_gravity = Cvar_Get("sv_gravity", "800", FCVAR_SERVER);
    sv_maxspeed = Cvar_Get("sv_maxspeed", "320", FCVAR_SERVER);
    sv_accelerate = Cvar_Get("sv_accelerate", "10", FCVAR_SERVER);
    sv_friction = Cvar_Get("sv_friction", "4", FCVAR_SERVER);
    sv_stopspeed = Cvar_Get("sv_stopspeed", "100", FCVAR_SERVER);
    sv_stepsize = Cvar_Get("sv_stepsize", "18", FCVAR_SERVER);

    svs.maxclientslimit = MAX_CLIENTS;
    svs.maxclients = (int)sv_maxclients->value;

    if (svs.maxclients < 1)
        svs.maxclients = 1;
    else if (svs.maxclients > svs.maxclientslimit)
        svs.maxclients = svs.maxclientslimit;

    Cmd_AddCommand("noclip", SV_Noclip_f);
    Cmd_AddCommand("setpos", SV_Setpos_f);
    Cmd_AddCommand("anim", R_SetEntityAnim_f);
    Cmd_AddCommand("testcontents", Test_PointContents_f);
    Cmd_AddCommand("testbsp", Test_BSPTree_f);
    Cmd_AddCommand("testleafs", Test_Leafs_f);

    // Выделение памяти для клиентов
    svs.clients = (client_t*)Mem_Alloc(sizeof(client_t) * svs.maxclientslimit);

    sv.max_edicts = MAX_EDICTS;
    sv.edicts = (edict_t*)Mem_Alloc(sizeof(edict_t) * sv.max_edicts);

    svs.initialized = TRUE;

    Con_Printf("Server initialized\n");
}

void SV_Shutdown(void)
{
    if (!svs.initialized)
        return;

    // Отключение всех клиентов
    int i;
    for (i = 0; i < svs.maxclients; i++)
    {
        if (svs.clients[i].active)
            SV_DropClient(&svs.clients[i], FALSE, "Server shutdown");
    }

    if (svs.clients)
    {
        Mem_Free(svs.clients);
        svs.clients = NULL;
    }

    if (sv.edicts)
    {
        Mem_Free(sv.edicts);
        sv.edicts = NULL;
    }

    memset(&sv, 0, sizeof(sv));
    memset(&svs, 0, sizeof(svs));

    Con_Printf("Server shutdown\n");
}

// ============================================================================
// Запуск карты
// ============================================================================

void SV_SpawnServer(const char* mapname, const char* startspot)
{
    int i;
    edict_t* ent;

    Con_Printf("SpawnServer: %s\n", mapname);

    // Очистка предыдущего состояния
    sv.state = ss_dead;

    // Отключение всех клиентов с сохранением соединения
    for (i = 0; i < svs.maxclients; i++)
    {
        if (svs.clients[i].active)
        {
            svs.clients[i].spawned = FALSE;
        }
    }

    // Инкремент spawncount (для отслеживания перезагрузок)
    svs.spawncount++;

    // Очистка структуры сервера (кроме некоторых данных)
    memset(&sv, 0, sizeof(sv));

    Q_strncpy(sv.name, mapname, sizeof(sv.name));
    if (startspot)
        Q_strncpy(sv.startspot, startspot, sizeof(sv.startspot));

    snprintf(sv.modelname, sizeof(sv.modelname), "maps/%s.qbsp", mapname);

    sv.state = ss_loading;
    sv.paused = FALSE;
    sv.time = 1.0;  // Начинаем с 1, чтобы избежать проблем с нулевым временем

    // Выделение edicts
    sv.max_edicts = MAX_EDICTS;
    if (!sv.edicts)
        sv.edicts = (edict_t*)Mem_Alloc(sizeof(edict_t) * sv.max_edicts);
    else
        memset(sv.edicts, 0, sizeof(edict_t) * sv.max_edicts);

    sv.num_edicts = svs.maxclients + 1;

    // Создание клиентских edict'ов
    for (i = 0; i < svs.maxclients; i++)
    {
        ent = EDICT_NUM(i + 1);
        svs.clients[i].edict = ent;
    }

    // Загрузка карты
    sv.worldmodel = Mod_LoadBrushModel(sv.modelname);

    extern model_t* cl_worldmodel;
    cl_worldmodel = sv.worldmodel;

    if (!sv.worldmodel)
    {
        Con_Printf("Couldn't load map %s\n", sv.modelname);
        R_LoadTextures(sv.worldmodel);
        sv.state = ss_dead;
        return;
    }

    // Precache мировой модели
    sv.model_precache[0] = "";  // пустая строка для индекса 0
    sv.model_precache[1] = sv.modelname;
    sv.model_precache_count = 2;

    sv.sound_precache[0] = "";
    sv.sound_precache_count = 1;

    // Инициализация мировой сущности
    ent = EDICT_NUM(0);
    ent->free = FALSE;
    ent->v.model = 1;  // Индекс в model_precache
    ent->v.modelindex = 1;
    ent->v.solid = SOLID_BSP;
    ent->v.movetype = MOVETYPE_PUSH;

    // Очистка world links
    SV_ClearWorld();

    R_ClearRenderEntities();

    // Загрузка сущностей из карты
    ED_LoadFromFile(sv.worldmodel->entities);

    sv.active = TRUE;
    sv.state = ss_active;

    // Вызов GameDLL
    if (gEntityInterface.pfnServerActivate)
        gEntityInterface.pfnServerActivate(sv.edicts, sv.num_edicts, svs.maxclients);

    Con_Printf("Server spawned.\n");
}

// ============================================================================
// Клиенты
// ============================================================================

void SV_DropClient(client_t* client, qboolean crash, const char* reason)
{
    if (!client->active)
        return;

    Con_Printf("Dropped %s: %s\n", client->name, reason);

    // Уведомление GameDLL
    if (gEntityInterface.pfnClientDisconnect && client->edict)
        gEntityInterface.pfnClientDisconnect(client->edict);

    // Очистка edict
    if (client->edict)
    {
        ED_Free(client->edict);
    }

    client->active = FALSE;
    client->spawned = FALSE;
    client->connected = FALSE;
    client->name[0] = 0;

    svs.num_clients--;
}

void SV_ClientPrintf(client_t* client, const char* fmt, ...)
{
    va_list args;
    char string[1024];

    va_start(args, fmt);
    vsnprintf(string, sizeof(string), fmt, args);
    va_end(args);

    // В реальной реализации - отправка через netchan
    MSG_WriteByte(&client->netchan.message, svc_print);
    MSG_WriteString(&client->netchan.message, string);
}

void SV_BroadcastPrintf(const char* fmt, ...)
{
    va_list args;
    char string[1024];
    int i;
    client_t* client;

    va_start(args, fmt);
    vsnprintf(string, sizeof(string), fmt, args);
    va_end(args);

    for (i = 0; i < svs.maxclients; i++)
    {
        client = &svs.clients[i];
        if (!client->active || !client->spawned)
            continue;

        SV_ClientPrintf(client, "%s", string);
    }

    Con_Printf("%s", string);
}

// ============================================================================
// Основной кадр сервера
// ============================================================================

void SV_Frame(float frametime)
{
    static int dbg;
    dbg++;

    if (!sv.active)
        return;

    // ЗАЩИТА от мусора
    if (frametime < 0.0f || frametime > 1.0f || _isnan(frametime))
    {
        Con_Printf("SV_Frame: BAD dt=%f, clamping to 0.01\n", frametime);
        frametime = 0.01f;
    }

    if (sv.time < 0.0 || sv.time > 1e9 || _isnan(sv.time))
    {
        Con_Printf("SV_Frame: sv.time corrupted (%f), resetting\n", sv.time);
        sv.time = 0.0;
        svs.realtime = 0.0;
    }

    if ((dbg % 100) == 0)
        Con_Printf("SV_Frame dt=%f active=%d time=%f\n", frametime, sv.active, sv.time);

    int i;
    client_t* client;

    if (!sv.active)
        return;

    svs.realtime += frametime;

    if (sv.paused)
        return;

    sv.oldtime = sv.time;
    sv.time += frametime;
    sv.frametime = frametime;

    // Чтение пакетов от клиентов
    SV_ReadPackets();

    // Запуск физики
    SV_Physics();

    // Обновление клиентов
    for (i = 0; i < svs.maxclients; i++)
    {
        client = &svs.clients[i];
        if (!client->active)
            continue;

        // Проверка timeout
        if (svs.realtime - client->last_message > sv_timeout->value)
        {
            SV_DropClient(client, FALSE, "Timed out");
            continue;
        }
    }

    // Отправка сообщений клиентам
    SV_SendClientMessages();
}

// ============================================================================
// Читы
// ============================================================================

void SV_Noclip_f(void)
{
    edict_t* player = EDICT_NUM(1);

    if (player->v.movetype == MOVETYPE_NOCLIP)
    {
        player->v.movetype = MOVETYPE_WALK;
        player->v.solid = SOLID_SLIDEBOX;
        Con_Printf("noclip OFF\n");
    }
    else
    {
        player->v.movetype = MOVETYPE_NOCLIP;
        player->v.solid = SOLID_NOT;
        Con_Printf("noclip ON\n");
    }
}

void SV_Setpos_f(void)
{
    vec3_t pos;
    if (Cmd_Argc() < 4)
    {
        Con_Printf("Usage: setpos <x> <y> <z>\n");
        return;
    }

    pos[0] = atof(Cmd_Argv(1));
    pos[1] = atof(Cmd_Argv(2));
    pos[2] = atof(Cmd_Argv(3));

    edict_t* ent = EDICT_NUM(1);
    Con_Printf("DEBUG: Old pos: %f %f %f\n", ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);

    VectorCopy(pos, ent->v.origin);

    Con_Printf("DEBUG: New pos: %f %f %f\n", ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);
    Con_Printf("Teleported to %f %f %f\n", pos[0], pos[1], pos[2]);
}

void R_SetEntityAnim_f(void)
{
    int anim;

    if (Cmd_Argc() < 2)
    {
        Con_Printf("Usage: anim <0=idle, 1=walk, 2=run>\n");
        return;
    }

    anim = atoi(Cmd_Argv(1));
    if (anim < 0 || anim >= ANIM_COUNT)
        anim = 0;

    // Устанавливаем всем entities
    for (int i = 0; i < r_numentities; i++)
    {
        r_entities[i].current_anim = (anim_type_t)anim;
        r_entities[i].anim_time = (float)cl.time;
    }

    Con_Printf("Set all entities to anim %d\n", anim);
}

void Test_PointContents_f(void)
{
    vec3_t pos;
    int contents;

    // Позиция игрока
    VectorCopy(cl.vieworg, pos);
    contents = SV_PointContents(pos);
    Con_Printf("Player pos: %.0f %.0f %.0f = %d\n", pos[0], pos[1], pos[2], contents);

    // Ниже на 100
    pos[2] -= 100;
    contents = SV_PointContents(pos);
    Con_Printf("Below 100: %.0f %.0f %.0f = %d\n", pos[0], pos[1], pos[2], contents);

    // Ниже на 500
    pos[2] -= 400;
    contents = SV_PointContents(pos);
    Con_Printf("Below 500: %.0f %.0f %.0f = %d\n", pos[0], pos[1], pos[2], contents);

    // Первый entity
    if (r_numentities > 0)
    {
        VectorCopy(r_entities[0].origin, pos);
        contents = SV_PointContents(pos);
        Con_Printf("Entity 0: %.0f %.0f %.0f = %d\n", pos[0], pos[1], pos[2], contents);

        pos[2] -= 50;
        contents = SV_PointContents(pos);
        Con_Printf("Entity 0 below: %.0f %.0f %.0f = %d\n", pos[0], pos[1], pos[2], contents);
    }
}

void Test_BSPTree_f(void)
{
    hull_t* hull = &sv.worldmodel->hulls[0];
    vec3_t p;

    // Позиция игрока
    VectorCopy(EDICT_NUM(1)->v.origin, p);

    Con_Printf("=== BSP Tree Debug ===\n");
    Con_Printf("Hull 0: firstclip=%d lastclip=%d\n",
        hull->firstclipnode, hull->lastclipnode);
    Con_Printf("Num clipnodes=%d, num planes=%d\n",
        sv.worldmodel->numclipnodes, sv.worldmodel->numplanes);
    Con_Printf("Player pos: %.1f %.1f %.1f\n", p[0], p[1], p[2]);

    // Проходим дерево вручную
    int num = hull->firstclipnode;
    int depth = 0;

    while (num >= 0 && depth < 32)
    {
        dclipnode_t* node = &hull->clipnodes[num];
        mplane_t* plane = &hull->planes[node->planenum];

        float d = DotProduct(p, plane->normal) - plane->dist;

        Con_Printf("  depth=%d node=%d plane=%d (%.2f %.2f %.2f d=%.1f) "
            "dot=%.1f -> %s child=%d\n",
            depth, num, node->planenum,
            plane->normal[0], plane->normal[1], plane->normal[2],
            plane->dist,
            d,
            (d >= 0) ? "FRONT" : "BACK",
            (d >= 0) ? node->children[0] : node->children[1]);

        num = (d >= 0) ? node->children[0] : node->children[1];
        depth++;
    }

    Con_Printf("  LEAF contents = %d", num);
    if (num == CONTENTS_EMPTY) Con_Printf(" (EMPTY)\n");
    else if (num == CONTENTS_SOLID) Con_Printf(" (SOLID)\n");
    else if (num == -1) Con_Printf(" (EMPTY/-1)\n");
    else if (num == -2) Con_Printf(" (SOLID/-2)\n");
    else Con_Printf(" (other)\n");

    // Также проверим SV_PointContents через nodes (не clipnodes)
    int pc = SV_PointContents(p);
    Con_Printf("SV_PointContents = %d\n", pc);

    // Проверим несколько высот
    for (float z = p[2] + 100; z >= p[2] - 100; z -= 20)
    {
        vec3_t test = { p[0], p[1], z };
        int c = SV_PointContents(test);
        Con_Printf("  z=%.0f contents=%d\n", z, c);
    }
}

void Test_Leafs_f(void)
{
    model_t* model = sv.worldmodel;

    Con_Printf("=== Leafs Debug (%d leafs) ===\n", model->numleafs);
    Con_Printf("sizeof(dleaf_t) = %d\n", (int)sizeof(dleaf_t));

    // Перечитаем сырые данные
    int length = 0;
    byte* buf = (byte*)COM_LoadFile(model->name, &length);
    if (!buf) { Con_Printf("Can't reload\n"); return; }

    dheader_t* header = (dheader_t*)buf;
    lump_t* l = &header->lumps[LUMP_LEAFS];
    int offset = LittleLong(l->offset);
    int lumplen = LittleLong(l->length);

    Con_Printf("LUMP_LEAFS: offset=%d length=%d\n", offset, lumplen);
    Con_Printf("count if 24-byte: %d\n", lumplen / 24);
    Con_Printf("count if 28-byte: %d\n", lumplen / 28);
    Con_Printf("count if 32-byte: %d\n", lumplen / 32);

    // Печатаем сырые байты первых 4 листьев
    byte* data = buf + offset;
    for (int i = 0; i < 4 && i * 28 < lumplen; i++)
    {
        Con_Printf("\nRaw bytes at +%d: ", i * 28);
        for (int j = 0; j < 28; j++)
            Con_Printf("%02X ", data[i * 28 + j]);
        Con_Printf("\n");

        // Попробуем как 28-байт (стандартный BSP29)
        int* as_int = (int*)(data + i * 28);
        short* as_short = (short*)(data + i * 28 + 8); // после contents+visofs
        Con_Printf("  As 28-byte: contents=%d visofs=%d mins=(%d %d %d) maxs=(%d %d %d)\n",
            LittleLong(as_int[0]), LittleLong(as_int[1]),
            LittleShort(as_short[0]), LittleShort(as_short[1]), LittleShort(as_short[2]),
            LittleShort(as_short[3]), LittleShort(as_short[4]), LittleShort(as_short[5]));

        // И как 24-байт (твой формат)
        as_int = (int*)(data + i * 24);
        as_short = (short*)(data + i * 24 + 8);
        Con_Printf("  As 24-byte: contents=%d visofs=%d mins=(%d %d %d) maxs=(%d %d %d)\n",
            LittleLong(as_int[0]), LittleLong(as_int[1]),
            LittleShort(as_short[0]), LittleShort(as_short[1]), LittleShort(as_short[2]),
            LittleShort(as_short[3]), LittleShort(as_short[4]), LittleShort(as_short[5]));
    }

    COM_FreeFile(buf);

    // Clipnodes
    Con_Printf("\n=== Clipnodes ===\n");
    hull_t* hull = &model->hulls[0];
    for (int i = 0; i <= hull->lastclipnode; i++)
    {
        dclipnode_t* cn = &hull->clipnodes[i];
        Con_Printf("  Clip[%d]: plane=%d children=(%d, %d)\n",
            i, cn->planenum, cn->children[0], cn->children[1]);
    }
}
