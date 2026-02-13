// engine/renderer/gl_main.c
#include "gl_local.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "mathlib.h"
#include "bsp.h"
#include <string.h>

glstate_t glstate;
extern refdef_t r_refdef;

// Cvars
cvar_t* r_norefresh;
cvar_t* r_drawentities;
cvar_t* r_drawworld;
cvar_t* r_speeds;
cvar_t* r_fullbright;
cvar_t* r_lightmap;
cvar_t* r_shadows;
cvar_t* r_dynamic;
cvar_t* r_clear;
cvar_t* gl_cull;
cvar_t* gl_polyblend;
cvar_t* gl_flashblend;
cvar_t* gl_texturemode;
cvar_t* gl_finish;
cvar_t* vid_gamma;
cvar_t* vid_brightness;

// Frustum planes для culling
mplane_t frustum[4];

// Матрицы
float r_world_matrix[16];
float r_projection_matrix[16];

// Расширения
PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTexture = NULL;

extern model_t* cl_worldmodel;

// ============================================================================
// Инициализация
// ============================================================================

static void R_RegisterVariables(void)
{
    r_norefresh = Cvar_Get("r_norefresh", "0", 0);
    r_drawentities = Cvar_Get("r_drawentities", "1", 0);
    r_drawworld = Cvar_Get("r_drawworld", "1", 0);
    r_clear = Cvar_Get("r_clear", "0", 0);
    r_speeds = Cvar_Get("r_speeds", "0", 0);
    r_fullbright = Cvar_Get("r_fullbright", "0", 0);
    r_lightmap = Cvar_Get("r_lightmap", "0", 0);
    r_shadows = Cvar_Get("r_shadows", "1", FCVAR_ARCHIVE);
    r_dynamic = Cvar_Get("r_dynamic", "1", 0);
    r_clear = Cvar_Get("r_clear", "0", 0);
    gl_cull = Cvar_Get("gl_cull", "1", 0);
    gl_polyblend = Cvar_Get("gl_polyblend", "1", 0);
    gl_flashblend = Cvar_Get("gl_flashblend", "0", 0);
    gl_texturemode = Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", FCVAR_ARCHIVE);
    gl_finish = Cvar_Get("gl_finish", "0", 0);
}

static void R_InitExtensions(void)
{
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);

    if (!extensions)
    {
        Con_Printf("R_InitExtensions: Warning - OpenGL context not valid (extensions is NULL)\n");
        return;
    }

    Con_Printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    Con_Printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    Con_Printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
    
    // Multitexture
    if (strstr(extensions, "GL_ARB_multitexture"))
    {
#ifdef _WIN32
        glActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTextureARB");
        glClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)wglGetProcAddress("glClientActiveTextureARB");
#else
        glActiveTexture = (PFNGLACTIVETEXTUREPROC)glXGetProcAddress((const GLubyte*)"glActiveTextureARB");
        glClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)glXGetProcAddress((const GLubyte*)"glClientActiveTextureARB");
#endif
        Con_Printf("...using GL_ARB_multitexture\n");
    }
}

extern void Draw_Init(void);

qboolean R_Init(void* hinstance, void* hwnd)
{
    Con_Printf("-------- R_Init --------\n");

    // 1. Регистрация переменных (r_norefresh, r_drawworld и т.д.)
    R_RegisterVariables();

    // 2. Инициализация расширений OpenGL (Multitexture и т.д.)
    R_InitExtensions();

    // 3. Базовые настройки OpenGL
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Черный фон по умолчанию
    glClearDepth(1.0f);

    vid_gamma = Cvar_Get("gamma", "1.5", FCVAR_ARCHIVE);
    vid_brightness = Cvar_Get("brightness", "1.0", FCVAR_ARCHIVE);
    VID_SetGamma(vid_gamma->value, vid_brightness->value);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glEnable(GL_TEXTURE_2D);

    glFrontFace(GL_CW);

    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    glAlphaFunc(GL_GREATER, 0.666f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Con_Printf("Initializing 2D subsystem...\n");
    Draw_Init();

    Con_Printf("-------- R_Init complete --------\n");
    return TRUE;
}

void R_Shutdown(void)
{
    VID_RestoreGamma();

    Con_Printf("R_Shutdown\n");
    // Освобождение текстур и т.д.
}

// ============================================================================
// Настройка кадра
// ============================================================================

void R_SetupFrustum(void)
{
    int i;
    vec3_t forward, right, up;
    float ang, xs, xc;

    AngleVectors(r_refdef.viewangles, forward, right, up);

    // Левая плоскость
    ang = DEG2RAD(r_refdef.fov_x / 2);
    xs = sinf(ang);
    xc = cosf(ang);

    VectorScale(forward, xs, frustum[0].normal);
    VectorMA(frustum[0].normal, xc, right, frustum[0].normal);

    VectorScale(forward, xs, frustum[1].normal);
    VectorMA(frustum[1].normal, -xc, right, frustum[1].normal);

    // Верхняя и нижняя
    ang = DEG2RAD(r_refdef.fov_y / 2);
    xs = sinf(ang);
    xc = cosf(ang);

    VectorScale(forward, xs, frustum[2].normal);
    VectorMA(frustum[2].normal, xc, up, frustum[2].normal);

    VectorScale(forward, xs, frustum[3].normal);
    VectorMA(frustum[3].normal, -xc, up, frustum[3].normal);

    for (i = 0; i < 4; i++)
    {
        frustum[i].type = PLANE_ANYZ;
        frustum[i].dist = DotProduct(r_refdef.vieworg, frustum[i].normal);
        frustum[i].signbits = SignbitsForPlane(&frustum[i]);
    }
}

void R_UpdateGamma(void)
{
    VID_SetGamma(vid_gamma->value, vid_brightness->value);
}

void R_SetupGL(float x, float y, float z, float pitch, float yaw, float roll)
{
    float screenaspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    screenaspect = (float)r_refdef.width / (float)r_refdef.height;
    gluPerspective(r_refdef.fov_y, screenaspect, 4.0, 4096.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(-90, 1, 0, 0);
    glRotatef(90, 0, 0, 1);

    glRotatef(-roll, 1, 0, 0);
    glRotatef(-pitch, 0, 1, 0);
    glRotatef(-yaw, 0, 0, 1);

    glTranslatef(-x, -y, -z);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
}

// ============================================================================
// Рендеринг кадра
// ============================================================================

void R_BeginFrame(void)
{
    glClearColor(0.0f, 0.0f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void R_EndFrame(void)
{
    if (gl_finish && gl_finish->value)
        glFinish();

    // Swap buffers делается в системном коде
}

void R_RenderView(float x, float y, float z, float pitch, float yaw, float roll)
{
    glViewport(0, 0, glstate.width, glstate.height);

    // НЕ делаем glClear - это делает R_BeginFrame!

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)glstate.width / (float)glstate.height;
    gluPerspective(90.0, aspect, 1.0, 8192.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(-90, 1, 0, 0);
    glRotatef(90, 0, 0, 1);
    glRotatef(-pitch, 0, 1, 0);
    glRotatef(-yaw, 0, 0, 1);
    glTranslatef(-x, -y, -z);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    if (cl_worldmodel)
        R_DrawWorld();
}

void (*R_PostWorldCallback)(void) = NULL;

// ============================================================================
// Вспомогательные функции
// ============================================================================

void R_RotateForEntity(entity_t* e)
{
    glTranslatef(e->origin[0], e->origin[1], e->origin[2]);

    glRotatef(e->angles[1], 0, 0, 1);
    glRotatef(-e->angles[0], 0, 1, 0);
    glRotatef(e->angles[2], 1, 0, 0);

    if (e->scale != 1.0f && e->scale != 0.0f)
    {
        glScalef(e->scale, e->scale, e->scale);
    }
}

int SignbitsForPlane(mplane_t* plane)
{
    int bits = 0;
    if (plane->normal[0] < 0) bits |= 1;
    if (plane->normal[1] < 0) bits |= 2;
    if (plane->normal[2] < 0) bits |= 4;
    return bits;
}

qboolean R_CullBox(vec3_t mins, vec3_t maxs)
{
    int i;

    if (!gl_cull->value)
        return FALSE;

    for (i = 0; i < 4; i++)
    {
        if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
            return TRUE;
    }

    return FALSE;
}

void R_DrawBrushModel(entity_t* e) {}
void R_DrawStudioModel(entity_t* e) {}
void R_DrawSpriteModel(entity_t* e) {}
void R_DrawDlights(void) {}
void R_DrawParticles(void) {}
void R_DrawWaterSurfaces(void) {}
