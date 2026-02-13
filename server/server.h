#ifndef SERVER_H
#define SERVER_H

#include "quakedef.h"
#include "net.h"
#include "progs.h"

typedef enum { ss_dead, ss_loading, ss_active } server_state_t;

typedef struct server_static_s {
    qboolean    initialized;
    int         maxclients;
    int         maxclientslimit;
    int         spawncount;
    int         num_clients;
    struct client_s* clients;
    double      realtime;
    double      last_heartbeat;
} server_static_t;

typedef struct server_s {
    qboolean    active;
    qboolean    paused;
    qboolean    loadgame;

    // Состояние
    int         state;      // ss_loading, ss_active и т.д.

    int         num_edicts;
    int         max_edicts;
    edict_t* edicts;

    double      time;
    double      oldtime;
    float       frametime;

    char        name[64];
    char        startspot[64];
    char        modelname[64];

    struct model_s* worldmodel;

    // Precache
    char* model_precache[MAX_MODELS];
    int         model_precache_count;
    char* sound_precache[MAX_SOUNDS];
    int         sound_precache_count;
    char* lightstyles[MAX_LIGHTSTYLES];

} server_t;

// Структура клиента на сервере
typedef struct client_s {
    qboolean    active;
    qboolean    spawned;
    qboolean    connected;
    qboolean    fakeclient;

    netchan_t   netchan;
    usercmd_t   lastcmd;

    edict_t* edict;
    char        name[32];
    int         userid;
    char        userinfo[MAX_INFO_STRING];

    double      last_message;
    double      connection_started;

    float       ping;
    float       packet_loss;

    // Дельта-компрессия (если нужно)
    // entity_state_t frames[64][MAX_PACKET_ENTITIES];
} client_t;

extern server_t sv;
extern server_static_t svs;

typedef struct {
    void (*pfnServerActivate)(edict_t* pEdicts, int edictCount, int clientMax);
    void (*pfnThink)(edict_t* pent);
    void (*pfnTouch)(edict_t* pent, edict_t* pentOther);
    void (*pfnClientDisconnect)(edict_t* pEntity);
} DLL_FUNCTIONS;

extern DLL_FUNCTIONS gEntityInterface;

void SV_Init(void);
void SV_Shutdown(void);
void SV_Frame(float frametime);

trace_t SV_Move(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t* passedict);
int SV_PointContents(vec3_t p);
void SV_LinkEdict(edict_t* ent, qboolean touch_triggers);
void SV_UnlinkEdict(edict_t* ent);
trace_t SV_PushEntity(edict_t* ent, vec3_t push);
void SV_Accelerate(edict_t* ent, vec3_t wishdir, float wishspeed, float accel);
void SV_AirAccelerate(edict_t* ent, vec3_t wishdir, float wishspeed, float accel);
void SV_UserFriction(edict_t* ent);
void SV_DropClient(client_t* client, qboolean crash, const char* reason);
void ED_ClearEdict(edict_t* e);
void SV_CheckWaterTransition(edict_t* ent);

#endif // SERVER_H
