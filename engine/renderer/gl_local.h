// engine/renderer/gl_local.h

#ifndef GL_LOCAL_H
#define GL_LOCAL_H

#include "quakedef.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#ifndef GL_TEXTURE0
#define GL_TEXTURE0     0x84C0
#define GL_TEXTURE1     0x84C1
#define GL_TEXTURE2     0x84C2
#define GL_TEXTURE3     0x84C3
#endif

#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E6
#endif
#include "bsp.h"
#include "mdl.h"

// Расширения OpenGL
typedef void (APIENTRY* PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void (APIENTRY* PFNGLCLIENTACTIVETEXTUREPROC)(GLenum texture);

extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTexture;

// Состояние рендерера
typedef struct glstate_s {
    int width;
    int height;
    qboolean fullscreen;

    float fov_x;
    float fov_y;

    int texnum;
    int texenv;

    qboolean blend;
    qboolean alphatest;
    qboolean depthtest;
    qboolean cullface;
} glstate_t;

extern glstate_t glstate;
extern refdef_t r_refdef;

// Сущность для рендеринга
typedef struct entity_s {
    struct model_s* model;

    vec3_t origin;
    vec3_t angles;

    float frame;
    float oldframe;
    float backlerp;

    int skin;
    int body;

    float scale;

    int rendermode;
    int renderamt;
    color24 rendercolor;
    int renderfx;
} entity_t;

// Динамический свет
typedef struct dlight_s {
    vec3_t origin;
    vec3_t color;
    float intensity;
    float die;
    float decay;
} dlight_t;

#define MAX_DLIGHTS     32
#define MAX_ENTITIES    1024
#define MAX_PARTICLES   4096
#define MAX_RENDER_ENTITIES MAX_ENTITIES

// Частица
typedef struct particle_s {
    vec3_t origin;
    vec3_t velocity;
    vec3_t accel;
    float time;
    float lifetime;
    color24 color;
    float alpha;
    float size;
} particle_t;

typedef struct render_entity_s
{
    qboolean        active;
    aliasmodel_t* model;
    vec3_t          origin;
    vec3_t          angles;
    int             frame;
    int             skin;

    // Анимация
    anim_type_t     current_anim;   // текущая анимация
    float           anim_time;      // время начала анимации

    // Физика
    vec3_t          velocity;
    qboolean        onground;
    float           gravity_scale;  // 1.0 = нормальная гравитация, 0 = без гравитации
} render_entity_t;

extern render_entity_t r_entities[MAX_RENDER_ENTITIES];
extern int r_numentities;
struct model_s;
extern int gl_fullbright;

// ============================================================================
// Функции рендерера
// ============================================================================

// Инициализация
qboolean R_Init(void* hinstance, void* hwnd);
void R_Shutdown(void);
void R_BeginFrame(void);
void R_EndFrame(void);

// Главный рендер
void R_RenderView(float x, float y, float z, float pitch, float yaw, float roll);
void R_SetupGL(float x, float y, float z, float pitch, float yaw, float roll);

// Мир
void R_DrawWorld(void);
void R_DrawWorldNoSky(void);
void    R_SetWorldModel(model_t* m);
model_t* R_GetWorldModel(void);

// Текстуры
int GL_LoadTexture(const char* name, byte* data, int width, int height, int flags);
void GL_FreeTexture(int texnum);
void GL_Bind(int texnum);
void GL_SelectTexture(int unit);

// Lightmaps
void GL_CreateLightmaps(model_t* model);
void GL_FreeLightmaps(model_t* model);

// Sky
void R_InitSky(texture_t* tex);
void R_InitSkyFromRGBA(const byte* rgba, int w, int h);
void R_DrawSkyPoly(msurface_t* surf);
void R_DrawSkyBackground(msurface_t* surf);

// 2D отрисовка
void Draw_Pic(int x, int y, int w, int h, int texnum);
void Draw_Fill(int x, int y, int w, int h, int r, int g, int b, int a);
void Draw_String(int x, int y, const char* str, int scale);
void Draw_Character(int x, int y, int num, int scale);
int GL_LoadImage(const char* filename);
void Draw_Background(void);

// Модели
void R_DrawBrushModel(entity_t* e);
void R_DrawStudioModel(entity_t* e);
void R_DrawSpriteModel(entity_t* e);

// Эффекты (заглушки пока)
void R_DrawParticles(void);
void R_DrawDlights(void);

// VBO
void R_BuildWorldVBO(model_t* world);
void R_DrawWorldVBO(void);
void R_FreeWorldVBO(void);
qboolean R_WorldVBOReady(void);

// Вспомогательные
void R_SetupFrustum(void);
void R_RotateForEntity(entity_t* e);

void VID_SetGamma(float gamma, float brightness);
void VID_RestoreGamma(void);
void R_UpdateGamma(void);

render_entity_t* R_AllocRenderEntity(void);
void R_ClearRenderEntities(void);
void R_DrawEntities(void);
void R_AnimateEntities(void);
void R_PhysicsEntities(float frametime);

// Флаги текстур
#define TF_NOMIPMAP     (1<<0)
#define TF_NOPICMIP     (1<<1)
#define TF_CLAMP        (1<<2)
#define TF_INTENSITY    (1<<3)
#define TF_ALPHA        (1<<4)

#endif // GL_LOCAL_H
