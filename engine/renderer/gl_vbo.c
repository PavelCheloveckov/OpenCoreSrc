// engine/renderer/gl_vbo.c
#include "gl_local.h"

// Определения VBO/VAO для старых заголовков
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER           0x8892
#define GL_STATIC_DRAW            0x88E4
#endif

#ifndef GLsizeiptr
typedef ptrdiff_t GLsizeiptr;
#endif

// Указатели на функции (загружаются через wglGetProcAddress)
typedef void (APIENTRY* PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY* PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);

typedef void (APIENTRY* PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef void (APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint);

typedef void (APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void (APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

static PFNGLGENBUFFERSPROC          pglGenBuffers = NULL;
static PFNGLDELETEBUFFERSPROC       pglDeleteBuffers = NULL;
static PFNGLBINDBUFFERPROC          pglBindBuffer = NULL;
static PFNGLBUFFERDATAPROC          pglBufferData = NULL;
static PFNGLGENVERTEXARRAYSPROC     pglGenVertexArrays = NULL;
static PFNGLDELETEVERTEXARRAYSPROC  pglDeleteVertexArrays = NULL;
static PFNGLBINDVERTEXARRAYPROC     pglBindVertexArray = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC    pglVertexAttribPointer = NULL;

static qboolean vbo_funcs_loaded = FALSE;

static void VBO_LoadFunctions(void)
{
    if (vbo_funcs_loaded) return;

    pglGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    pglDeleteBuffers = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
    pglBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    pglBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    pglGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
    pglDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)wglGetProcAddress("glDeleteVertexArrays");
    pglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    pglEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
    pglVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");

    if (pglGenBuffers && pglBindBuffer && pglBufferData)
        vbo_funcs_loaded = TRUE;
    else
        Con_Printf("WARNING: VBO functions not available!\n");
}

// Макросы для удобства
#define glGenBuffers          pglGenBuffers
#define glDeleteBuffers       pglDeleteBuffers
#define glBindBuffer          pglBindBuffer
#define glBufferData          pglBufferData
#define glGenVertexArrays     pglGenVertexArrays
#define glDeleteVertexArrays  pglDeleteVertexArrays
#define glBindVertexArray     pglBindVertexArray
#define glEnableVertexAttribArray pglEnableVertexAttribArray
#define glVertexAttribPointer    pglVertexAttribPointer

// Вершина для VBO
typedef struct {
    float pos[3];      // xyz
    float texcoord[2]; // st
    float lmcoord[2];  // lightmap st
} worldvert_t;

// Группа поверхностей с одной текстурой
typedef struct {
    int gl_texturenum;
    int lightmaptexturenum;
    int first_index;   // начало в массиве вершин
    int num_indices;   // количество вершин (triangles)
} texgroup_t;

#define MAX_WORLD_VERTS   (256 * 1024)
#define MAX_TEX_GROUPS    4096

static GLuint world_vao = 0;
static GLuint world_vbo = 0;

static worldvert_t  world_verts[MAX_WORLD_VERTS];
static int          num_world_verts = 0;

static texgroup_t   tex_groups[MAX_TEX_GROUPS];
static int          num_tex_groups = 0;

static qboolean     vbo_ready = FALSE;

// ============================================================================
// Найти или создать группу для текстуры
// ============================================================================
static int FindOrCreateGroup(int texnum, int lmnum)
{
    int i;
    for (i = 0; i < num_tex_groups; i++)
    {
        if (tex_groups[i].gl_texturenum == texnum &&
            tex_groups[i].lightmaptexturenum == lmnum)
            return i;
    }

    if (num_tex_groups >= MAX_TEX_GROUPS)
        return 0;

    i = num_tex_groups++;
    tex_groups[i].gl_texturenum = texnum;
    tex_groups[i].lightmaptexturenum = lmnum;
    tex_groups[i].first_index = 0;
    tex_groups[i].num_indices = 0;
    return i;
}

// ============================================================================
// Собрать геометрию мира в VBO (вызывается один раз при загрузке карты)
// ============================================================================
void R_BuildWorldVBO(model_t* world)
{
    int i, j;
    msurface_t* surf;
    glpoly_t* p;
    float* v;

    if (!world) return;

    VBO_LoadFunctions();

    num_world_verts = 0;
    num_tex_groups = 0;
    vbo_ready = FALSE;

    // Первый проход: подсчитать вершины и сгруппировать по текстурам
    // Полигоны разбиваем на треугольники (fan: 0-1-2, 0-2-3, 0-3-4...)

    // Временный массив для привязки surface -> group
    // Сначала просто складываем все треугольники подряд

    for (i = 0; i < world->numsurfaces; i++)
    {
        surf = &world->surfaces[i];
        p = surf->polys;
        if (!p) continue;
        if (p->numverts < 3) continue;

        int texnum = 0;
        int lmnum = 0;

        if (surf->texinfo && surf->texinfo->texture)
            texnum = surf->texinfo->texture->gl_texturenum;
        lmnum = surf->lightmaptexturenum;

        // Каждый полигон -> triangle fan
        // N вершин -> (N-2) треугольников -> (N-2)*3 вершин
        int num_tris = p->numverts - 2;
        int verts_needed = num_tris * 3;

        // Front face
        if (num_world_verts + verts_needed * 2 > MAX_WORLD_VERTS)
        {
            Con_Printf("WARNING: MAX_WORLD_VERTS exceeded!\n");
            break;
        }

        // Front face triangles
        for (j = 0; j < num_tris; j++)
        {
            // Triangle: v0, v(j+1), v(j+2)
            float* v0 = p->verts[0];
            float* v1 = p->verts[j + 1];
            float* v2 = p->verts[j + 2];

            worldvert_t* dst;

            dst = &world_verts[num_world_verts++];
            dst->pos[0] = v0[0]; dst->pos[1] = v0[1]; dst->pos[2] = v0[2];
            dst->texcoord[0] = v0[3]; dst->texcoord[1] = v0[4];
            dst->lmcoord[0] = v0[5]; dst->lmcoord[1] = v0[6];

            dst = &world_verts[num_world_verts++];
            dst->pos[0] = v1[0]; dst->pos[1] = v1[1]; dst->pos[2] = v1[2];
            dst->texcoord[0] = v1[3]; dst->texcoord[1] = v1[4];
            dst->lmcoord[0] = v1[5]; dst->lmcoord[1] = v1[6];

            dst = &world_verts[num_world_verts++];
            dst->pos[0] = v2[0]; dst->pos[1] = v2[1]; dst->pos[2] = v2[2];
            dst->texcoord[0] = v2[3]; dst->texcoord[1] = v2[4];
            dst->lmcoord[0] = v2[5]; dst->lmcoord[1] = v2[6];
        }

        // Back face triangles (reversed winding)
        for (j = 0; j < num_tris; j++)
        {
            float* v0 = p->verts[0];
            float* v1 = p->verts[j + 2];  // reversed
            float* v2 = p->verts[j + 1];  // reversed

            worldvert_t* dst;

            dst = &world_verts[num_world_verts++];
            dst->pos[0] = v0[0]; dst->pos[1] = v0[1]; dst->pos[2] = v0[2];
            dst->texcoord[0] = v0[3]; dst->texcoord[1] = v0[4];
            dst->lmcoord[0] = v0[5]; dst->lmcoord[1] = v0[6];

            dst = &world_verts[num_world_verts++];
            dst->pos[0] = v1[0]; dst->pos[1] = v1[1]; dst->pos[2] = v1[2];
            dst->texcoord[0] = v1[3]; dst->texcoord[1] = v1[4];
            dst->lmcoord[0] = v1[5]; dst->lmcoord[1] = v1[6];

            dst = &world_verts[num_world_verts++];
            dst->pos[0] = v2[0]; dst->pos[1] = v2[1]; dst->pos[2] = v2[2];
            dst->texcoord[0] = v2[3]; dst->texcoord[1] = v2[4];
            dst->lmcoord[0] = v2[5]; dst->lmcoord[1] = v2[6];
        }
    }

    if (num_world_verts == 0)
        return;

    // Теперь нужно сгруппировать по текстурам
    // Пересобираем - сортируем по текстуре
    // Простой подход: второй проход, записываем в группы

    // Сброс
    num_tex_groups = 0;
    int sorted_count = 0;

    // Временный буфер для сортированных вершин
    static worldvert_t sorted_verts[MAX_WORLD_VERTS];

    for (i = 0; i < world->numsurfaces; i++)
    {
        surf = &world->surfaces[i];
        p = surf->polys;
        if (!p || p->numverts < 3) continue;

        int texnum = 0;
        int lmnum = 0;
        if (surf->texinfo && surf->texinfo->texture)
            texnum = surf->texinfo->texture->gl_texturenum;
        lmnum = surf->lightmaptexturenum;

        int gi = FindOrCreateGroup(texnum, lmnum);
        texgroup_t* g = &tex_groups[gi];

        int num_tris = p->numverts - 2;

        // Пока просто запоминаем количество
        g->num_indices += num_tris * 3 * 2; // front + back
    }

    // Вычисляем смещения
    int offset = 0;
    for (i = 0; i < num_tex_groups; i++)
    {
        tex_groups[i].first_index = offset;
        offset += tex_groups[i].num_indices;
        tex_groups[i].num_indices = 0; // сбросим для заполнения
    }

    // Третий проход: заполняем sorted_verts по группам
    for (i = 0; i < world->numsurfaces; i++)
    {
        surf = &world->surfaces[i];
        p = surf->polys;
        if (!p || p->numverts < 3) continue;

        int texnum = 0;
        int lmnum = 0;
        if (surf->texinfo && surf->texinfo->texture)
            texnum = surf->texinfo->texture->gl_texturenum;
        lmnum = surf->lightmaptexturenum;

        int gi = FindOrCreateGroup(texnum, lmnum);
        texgroup_t* g = &tex_groups[gi];

        int num_tris = p->numverts - 2;

        for (j = 0; j < num_tris; j++)
        {
            float* v0 = p->verts[0];
            float* v1 = p->verts[j + 1];
            float* v2 = p->verts[j + 2];

            int idx = g->first_index + g->num_indices;
            worldvert_t* dst;

            // Front
            dst = &sorted_verts[idx++];
            dst->pos[0] = v0[0]; dst->pos[1] = v0[1]; dst->pos[2] = v0[2];
            dst->texcoord[0] = v0[3]; dst->texcoord[1] = v0[4];
            dst->lmcoord[0] = v0[5]; dst->lmcoord[1] = v0[6];

            dst = &sorted_verts[idx++];
            dst->pos[0] = v1[0]; dst->pos[1] = v1[1]; dst->pos[2] = v1[2];
            dst->texcoord[0] = v1[3]; dst->texcoord[1] = v1[4];
            dst->lmcoord[0] = v1[5]; dst->lmcoord[1] = v1[6];

            dst = &sorted_verts[idx++];
            dst->pos[0] = v2[0]; dst->pos[1] = v2[1]; dst->pos[2] = v2[2];
            dst->texcoord[0] = v2[3]; dst->texcoord[1] = v2[4];
            dst->lmcoord[0] = v2[5]; dst->lmcoord[1] = v2[6];

            // Back
            dst = &sorted_verts[idx++];
            dst->pos[0] = v0[0]; dst->pos[1] = v0[1]; dst->pos[2] = v0[2];
            dst->texcoord[0] = v0[3]; dst->texcoord[1] = v0[4];
            dst->lmcoord[0] = v0[5]; dst->lmcoord[1] = v0[6];

            dst = &sorted_verts[idx++];
            dst->pos[0] = v2[0]; dst->pos[1] = v2[1]; dst->pos[2] = v2[2];
            dst->texcoord[0] = v2[3]; dst->texcoord[1] = v2[4];
            dst->lmcoord[0] = v2[5]; dst->lmcoord[1] = v2[6];

            dst = &sorted_verts[idx++];
            dst->pos[0] = v1[0]; dst->pos[1] = v1[1]; dst->pos[2] = v1[2];
            dst->texcoord[0] = v1[3]; dst->texcoord[1] = v1[4];
            dst->lmcoord[0] = v1[5]; dst->lmcoord[1] = v1[6];

            g->num_indices += 6;
        }
    }

    // Создаём VAO/VBO
    if (world_vao) glDeleteVertexArrays(1, &world_vao);
    if (world_vbo) glDeleteBuffers(1, &world_vbo);

    glGenVertexArrays(1, &world_vao);
    glGenBuffers(1, &world_vbo);

    glBindVertexArray(world_vao);
    glBindBuffer(GL_ARRAY_BUFFER, world_vbo);
    glBufferData(GL_ARRAY_BUFFER, offset * sizeof(worldvert_t),
        sorted_verts, GL_STATIC_DRAW);

    // pos = attribute 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        sizeof(worldvert_t), (void*)0);

    // texcoord = attribute 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
        sizeof(worldvert_t), (void*)(3 * sizeof(float)));

    // lmcoord = attribute 2
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
        sizeof(worldvert_t), (void*)(5 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vbo_ready = TRUE;

    Con_Printf("VBO: %d vertices, %d texture groups\n",
        offset, num_tex_groups);
}

// ============================================================================
// Рисование мира через VBO (вызывается каждый кадр)
// ============================================================================
void R_DrawWorldVBO(void)
{
    int i;

    if (!vbo_ready)
        return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1, 1, 1, 1);

    glBindVertexArray(world_vao);

    // Для совместимости с fixed-function pipeline
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindBuffer(GL_ARRAY_BUFFER, world_vbo);

    // Position
    glVertexPointer(3, GL_FLOAT, sizeof(worldvert_t), (void*)0);

    // Texcoord
    glTexCoordPointer(2, GL_FLOAT, sizeof(worldvert_t),
        (void*)(3 * sizeof(float)));

    for (i = 0; i < num_tex_groups; i++)
    {
        texgroup_t* g = &tex_groups[i];
        if (g->num_indices == 0) continue;

        GL_Bind(g->gl_texturenum);

        glDrawArrays(GL_TRIANGLES, g->first_index, g->num_indices);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glColor4f(1, 1, 1, 1);
}

// ============================================================================
// Очистка
// ============================================================================
void R_FreeWorldVBO(void)
{
    if (world_vao) { glDeleteVertexArrays(1, &world_vao); world_vao = 0; }
    if (world_vbo) { glDeleteBuffers(1, &world_vbo); world_vbo = 0; }
    num_world_verts = 0;
    num_tex_groups = 0;
    vbo_ready = FALSE;
}

qboolean R_WorldVBOReady(void)
{
    return vbo_ready;
}
