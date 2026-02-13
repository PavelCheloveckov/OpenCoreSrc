#ifndef CLIENT_H
#define CLIENT_H

#include "quakedef.h"
#include "mathlib.h"
#include "net.h"

// ============================================
// Forward declarations для типов из других заголовков
// ============================================

// entity_state_t определена в entity_state.h, но мы используем forward declaration
// чтобы избежать циклических зависимостей
struct entity_state_s;

// ============================================
// Состояние подключения
// ============================================
typedef enum {
    ca_disconnected,
    ca_connecting,
    ca_connected,
    ca_active
} connstate_t;

// ============================================
// Динамический свет (локальное определение для клиента)
// ============================================
typedef struct cl_dlight_s {
    vec3_t origin;
    vec3_t color;
    float intensity;
    float die;
    float decay;
} cl_dlight_t;

// ============================================
// Частица (локальное определение для клиента)
// ============================================
typedef struct cl_particle_s {
    vec3_t origin;
    vec3_t velocity;
    vec3_t accel;
    float time;
    float lifetime;
    color24 color;
    float alpha;
    float size;
} cl_particle_t;

// ============================================
// Client State - состояние игры на клиенте
// ============================================
typedef struct client_state_s {
    int         servercount;
    double      time;
    int         playernum;

    vec3_t      vieworg;
    vec3_t      viewangles;
    vec3_t      punchangle;
    vec3_t      velocity;

    vec3_t      predicted_origin;
    vec3_t      predicted_velocity;
    vec3_t      prediction_error;
    double      prediction_time;

    int         onground;
    int         waterlevel;

    usercmd_t   cmd;

    // Сущности - используем указатель вместо массива с неизвестным типом
    // или определяем упрощённую структуру
    // struct entity_state_s* entities;
    // int num_entities;

    // Простой вариант - храним данные напрямую
    vec3_t      ent_origins[MAX_EDICTS];
    vec3_t      ent_angles[MAX_EDICTS];
    int         ent_models[MAX_EDICTS];

    char        model_name[MAX_MODELS][MAX_QPATH];
    char        sound_name[MAX_SOUNDS][MAX_QPATH];

    cl_dlight_t dlights[MAX_DLIGHTS];
    int         num_dlights;

    cl_particle_t particles[MAX_PARTICLES];
    int         num_particles;

    int         stats[32];

    struct model_s* worldmodel;
} client_state_t;

// ============================================
// Client Static - постоянные данные клиента
// ============================================
typedef struct client_static_s {
    qboolean    initialized;
    connstate_t state;
    netchan_t   netchan;
    double      realtime;
    float       frametime;
    qboolean    demoplayback;
    netadr_t    serveraddress;
    double      connect_time;
    int         connect_count;
} client_static_t;

// ============================================
// Глобальные переменные
// ============================================
extern client_state_t cl;
extern client_static_t cls;

// ============================================
// Функции клиента
// ============================================
void CL_Init(void);
void CL_Shutdown(void);
void CL_Frame(float frametime);
void CL_SendCmd(void);
void CL_Connect_f(void);
void CL_Disconnect_f(void);
void CL_Reconnect_f(void);
void CL_ForwardToServer_f(void);
void CL_Disconnect(void);

// Парсинг
void CL_ReadPackets(void);
void CL_ParseServerMessage(void);
void CL_ParseServerInfo(void);
void CL_ParseClientData(void);
void CL_ParsePacketEntities(void);

// Предсказание и ввод
void CL_CreateCmd(void);
void CL_PredictMove(void);
void CL_SetPredictionError(vec3_t error);

// Вид
void V_CalcRefdef(void);
void V_RenderView(void);

// HUD
void HUD_Draw(void);

void M_Init(void);
void M_Draw(void);
void M_Keydown(int key);
void M_ToggleMenu_f(void);
void M_CloseMenu(void);
qboolean M_IsActive(void);

#endif // CLIENT_H
