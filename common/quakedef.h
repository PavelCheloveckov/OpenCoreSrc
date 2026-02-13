#pragma once
#ifndef QUAKEDEF_H
#define QUAKEDEF_H

#ifdef __cplusplus
extern "C" {
#endif

// Стандартные заголовки
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

// Базовые типы
typedef unsigned char byte;
typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
extern vec3_t vec3_origin;

#ifndef QBOOLEAN_DEFINED
#define QBOOLEAN_DEFINED
typedef int qboolean;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// Содержимое пространства
#define CONTENTS_EMPTY      -1
#define CONTENTS_SOLID      -2
#define CONTENTS_WATER      -3
#define CONTENTS_SLIME      -4
#define CONTENTS_LAVA       -5
#define CONTENTS_SKY        -6

// Типы моделей
typedef enum {
    mod_bad,
    mod_brush,
    mod_sprite,
    mod_studio
} modtype_t;

// Цвета
typedef struct color24_s {
    byte r, g, b;
} color24;

typedef struct colorVec_s {
    unsigned r, g, b, a;
} colorVec;

// String type
typedef int string_t;


typedef struct mplane_s mplane_t;

// Лимиты
#define MAX_QPATH           64
#define MAX_OSPATH          260
#define MAX_EDICTS          1024
#define MAX_MODELS          512
#define MAX_SOUNDS          512
#define MAX_CLIENTS         32
#define MAX_MSGLEN          16384
#define MAX_DATAGRAM        4000
#define MAX_INFO_STRING     256
#define MAX_DLIGHTS         32
#define MAX_PARTICLES       4096
#define MAX_LIGHTSTYLES     64

// Movetypes
#define MOVETYPE_NONE           0
#define MOVETYPE_WALK           3
#define MOVETYPE_STEP           4
#define MOVETYPE_FLY            5
#define MOVETYPE_TOSS           6
#define MOVETYPE_PUSH           7
#define MOVETYPE_NOCLIP         8
#define MOVETYPE_FLYMISSILE     9
#define MOVETYPE_BOUNCE         10
#define MOVETYPE_BOUNCEMISSILE  11
#define MOVETYPE_FOLLOW         12
#define MOVETYPE_PUSHSTEP       13

// Buttons
#define IN_ATTACK       (1<<0)
#define IN_JUMP         (1<<1)
#define IN_DUCK         (1<<2)
#define IN_FORWARD      (1<<3)
#define IN_BACK         (1<<4)
#define IN_USE          (1<<5)
#define IN_CANCEL       (1<<6)
#define IN_LEFT         (1<<7)
#define IN_RIGHT        (1<<8)
#define IN_MOVELEFT     (1<<9)
#define IN_MOVERIGHT    (1<<10)
#define IN_ATTACK2      (1<<11)
#define IN_RUN          (1<<12)
#define IN_RELOAD       (1<<13)
#define IN_ALT1         (1<<14)
#define IN_SCORE        (1<<15)

// Entity flags
#define FL_FLY              (1<<0)
#define FL_SWIM             (1<<1)
#define FL_CLIENT           (1<<3)
#define FL_INWATER          (1<<4)
#define FL_MONSTER          (1<<5)
#define FL_GODMODE          (1<<6)
#define FL_NOTARGET         (1<<7)
#define FL_ONGROUND         (1<<9)
#define FL_PARTIALGROUND    (1<<10)
#define FL_WATERJUMP        (1<<11)
#define FL_FROZEN           (1<<12)
#define FL_FAKECLIENT       (1<<13)
#define FL_DUCKING          (1<<14)
#define FL_JUMPED           (1<<15)

// Move types for SV_Move
#define MOVE_NORMAL     0
#define MOVE_NOMONSTERS 1
#define MOVE_MISSILE    2

// Solid types
#define SOLID_NOT       0
#define SOLID_TRIGGER   1
#define SOLID_BBOX      2
#define SOLID_SLIDEBOX  3
#define SOLID_BSP       4

// Stats
#define STAT_HEALTH     0

// Client commands
#define clc_stringcmd   4

// Server commands
#define svc_nop             1
#define svc_disconnect      2
#define svc_print           8
#define svc_stufftext       9
#define svc_serverinfo      11
#define svc_time            7
#define svc_clientdata      15
#define svc_packetentities  40

// Macros
#define IS_NAN(x) ((x) != (x))
#define STRING(x) ((const char*)(x))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================
// Usercmd
// ============================================
typedef struct usercmd_s {
    byte msec;
    vec3_t viewangles;
    float forwardmove;
    float sidemove;
    float upmove;
    unsigned short buttons;
    byte impulse;
} usercmd_t;

// ============================================
// Forward declarations
// ============================================
typedef struct entvars_s entvars_t;
typedef struct edict_s edict_t;
typedef struct client_s client_t;
struct model_s;
typedef struct netchan_s netchan_t;
typedef struct progs_s progs_t;

// ============================================
// Externals
// ============================================
extern struct sizebuf_s net_message;
extern qboolean host_initialized;
extern int msg_badread;
extern double host_frametime;

// ============================================
// Mplane
// ============================================
typedef struct mplane_s {
    vec3_t normal;
    float dist;
    byte type;
    byte signbits;
    byte pad[2];
} mplane_t;

// ============================================
// Trace result
// ============================================
typedef struct trace_s {
    qboolean    allsolid;
    qboolean    startsolid;
    float       fraction;
    vec3_t      endpos;
    mplane_t    plane;
    edict_t* ent;
} trace_t;

// ============================================
typedef struct refdef_s {
    int x, y;
    int width, height;

    float fov_x, fov_y;

    vec3_t vieworg;
    vec3_t viewangles;

    float time;

    int rdflags;

    byte* areabits;

    int num_entities;
    struct entity_s* entities;

    int num_dlights;
    struct dlight_s* dlights;

} refdef_t;

// ============================================
// Core functions
// ============================================
void Sys_Error(const char* error, ...);
void Sys_Quit(void);
double Sys_FloatTime(void);
void Sys_Sleep(int msec);

void Host_Disconnect_f(void);
void Host_Map_f(void);
void Host_Quit_f(void);
void Host_WriteConfig(void);

void S_Init(void);
void S_Shutdown(void);
void S_StopAllSounds(void);
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up);

void IN_Init(void);
void IN_Shutdown(void);
void IN_Frame(void);

void CL_Init(void);
void CL_Shutdown(void);
void CL_Frame(float frametime);
void CL_Disconnect(void);

void SV_Init(void);
void SV_Shutdown(void);
void SV_Frame(float frametime);
void SV_SpawnServer(const char* mapname, const char* startspot);
void SV_Physics(void);

void R_BeginFrame(void);
void R_EndFrame(void);
void R_RenderView(float x, float y, float z, float pitch, float yaw, float roll);
void R_Shutdown(void);
void R_DrawWorld(void);

void GL_Set2D(void);

edict_t* EDICT_NUM(int n);

void Cvar_WriteVariables(FILE* f);
void Cvar_Shutdown(void);

void SeedRandomNumberGenerator(void);

#ifdef __cplusplus
}
#endif

#endif // QUAKEDEF_H
