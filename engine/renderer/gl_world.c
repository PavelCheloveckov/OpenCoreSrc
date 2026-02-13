// engine/renderer/gl_world.c
// Рендеринг BSP мира

#include "gl_local.h"
#include "bsp.h"
#include "common.h"
#include "mathlib.h"
#include "wad.h"
#include <server.h>
#include <client.h>
#include <cvar.h>

extern model_t* cl_worldmodel;
extern byte host_basepal[768];
extern cvar_t* r_fullbright;

static int r_visframecount;
static int r_framecount;
static mleaf_t* r_viewleaf;

// Sky textures (классический Quake: два слоя)
static GLuint sky_solidtexture = 0;    // задний слой
static GLuint sky_alphatexture = 0;    // передний слой с прозрачностью

static model_t* g_worldmodel = NULL;
static int g_dbg_print_once = 0;

model_t* R_GetWorldModel(void)
{
    return g_worldmodel;
}

void R_SetWorldModel(model_t* m)
{
    g_worldmodel = m;
    g_dbg_print_once = 1;
}

// ============================================================================
// PVS
// ============================================================================

static byte* Mod_LeafPVS(mleaf_t* leaf, model_t* model)
{
    if (!leaf || leaf == model->leafs)
        return NULL;
    return leaf->compressed_vis;
}

static void R_MarkLeaves(model_t* world)
{
    byte* vis;
    mnode_t* node;
    mleaf_t* leaf;
    int i;

    if (!world) return;

    r_visframecount++;
    r_viewleaf = Mod_PointInLeaf(r_refdef.vieworg, world);

    vis = Mod_LeafPVS(r_viewleaf, world);

    if (!vis)
    {
        for (i = 0; i < world->numleafs; i++)
            world->leafs[i].visframe = r_visframecount;
        return;
    }

    for (i = 0; i < world->numleafs - 1; i++)
    {
        if (vis[i >> 3] & (1 << (i & 7)))
        {
            leaf = &world->leafs[i + 1];
            leaf->visframe = r_visframecount;

            node = (mnode_t*)leaf->parent;
            while (node)
            {
                if ((uintptr_t)node < 0x1000) break;
                if (node->visframe == r_visframecount) break;
                node->visframe = r_visframecount;
                node = node->parent;
            }
        }
    }
}
// ============================================================================
// Инициализация классического Quake sky
// ============================================================================

void R_InitSky(texture_t* tex)
{
    int i, j, p;
    byte* src;
    byte* solid, * alpha;

    if (!tex || tex->width != 256 || tex->height != 128)
    {
        Con_Printf("R_InitSky: invalid sky texture\n");
        return;
    }

    solid = (byte*)Mem_Alloc(128 * 128 * 4);
    alpha = (byte*)Mem_Alloc(128 * 128 * 4);

    for (i = 0; i < 128 * 128; i++)
    {
        // Solid: тёмно-синий
        solid[i * 4 + 0] = 50;
        solid[i * 4 + 1] = 80;
        solid[i * 4 + 2] = 140;
        solid[i * 4 + 3] = 255;

        // Alpha: светлые облака
        alpha[i * 4 + 0] = 200;
        alpha[i * 4 + 1] = 200;
        alpha[i * 4 + 2] = 220;
        alpha[i * 4 + 3] = 0;  // полностью прозрачный пока
    }

    // Создаём GL текстуры
    glGenTextures(1, &sky_solidtexture);
    glBindTexture(GL_TEXTURE_2D, sky_solidtexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, solid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenTextures(1, &sky_alphatexture);
    glBindTexture(GL_TEXTURE_2D, sky_alphatexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, alpha);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    Mem_Free(solid);
    Mem_Free(alpha);

    Con_Printf("Sky textures initialized\n");
}

// ============================================================================
// Рисование SKY поверхности
// ============================================================================
void R_LoadTextures(model_t* model)
{
    // Текстуры уже загружены в Mod_LoadTextures() при загрузке BSP
    // Эта функция - заглушка для совместимости со старым кодом
    (void)model;
}

// ============================================================================
// Рисование WATER/LAVA/SLIME
// ============================================================================

static void R_DrawWaterPoly(msurface_t* surf)
{
    glpoly_t* p;
    float* v;
    int i;
    texture_t* tex;
    float s, t;

    if (!surf->polys)
        return;

    tex = surf->texinfo ? surf->texinfo->texture : NULL;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 0.7f);

    if (tex && tex->gl_texturenum)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex->gl_texturenum);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
        glColor4f(0.2f, 0.4f, 0.8f, 0.7f);  // синяя вода
    }

    for (p = surf->polys; p; p = p->next)
    {
        glBegin(GL_POLYGON);
        for (i = 0; i < p->numverts; i++)
        {
            v = p->verts[i];

            // Анимация воды
            s = v[3] + sin(v[0] * 0.05f + cl.time) * 0.1f;
            t = v[4] + sin(v[1] * 0.05f + cl.time) * 0.1f;

            glTexCoord2f(s, t);
            glVertex3fv(v);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glColor4f(1, 1, 1, 1);
    glEnable(GL_TEXTURE_2D);
}

// ============================================================================
// Рисование обычной поверхности (TWO-PASS: base + lightmap)
// ============================================================================

static void R_DrawTexturedPoly(msurface_t* surf)
{
    glpoly_t* p;
    float* v;
    int i;
    texture_t* tex;
    extern int gl_fullbright;

    p = surf->polys;
    if (!p)
        return;

    tex = surf->texinfo ? surf->texinfo->texture : NULL;

    // Анимированные текстуры
    if (tex && tex->anim_total > 0)
    {
        int frame = (int)(cl.time * 5) % tex->anim_total;
        for (i = 0; i < frame && tex->anim_next; i++)
            tex = tex->anim_next;
    }

    // ===== PASS 1: Base texture =====
    if (tex && tex->gl_texturenum)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex->gl_texturenum);
        glColor4f(1, 1, 1, 1);
    }
    else
    {
        // Временный fallback: если текстуры нет, рисуем серым
        glDisable(GL_TEXTURE_2D);
        glColor4f(0.5f, 0.5f, 0.5f, 1.0f); // Серый цвет стен

        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    glDisable(GL_BLEND);

    glBegin(GL_POLYGON);
    for (i = 0; i < p->numverts; i++)
    {
        v = p->verts[i];
        if (tex && tex->gl_texturenum)
            glTexCoord2f(v[3], v[4]);  // base UV только если есть текстура
        glVertex3fv(v);
    }
    glEnd();

    if (!tex || !tex->gl_texturenum)
    {
        glEnable(GL_TEXTURE_2D); // восстанавливаем состояние
        // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // если включал wireframe
        return;
    }

    if (gl_fullbright)
        return;

    // ===== PASS 2: Lightmap (multiply blend) =====
    if (surf->lightmaptexturenum)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);  // dst = dst * src

        glDepthMask(GL_FALSE);
        glDepthFunc(GL_EQUAL);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, surf->lightmaptexturenum);

        glBegin(GL_POLYGON);
        for (i = 0; i < p->numverts; i++)
        {
            v = p->verts[i];
            glTexCoord2f(v[5], v[6]);  // lightmap UV
            glVertex3fv(v);
        }
        glEnd();

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_BLEND);
    }

    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
}

// ============================================================================
// Общая функция рисования поверхности
// ============================================================================

static void R_DrawSurface(msurface_t* surf)
{
    if (surf->flags & SURF_DRAWSKY)
    {
        R_DrawSkyPoly(surf);
        return;
    }

    if (surf->flags & SURF_DRAWTURB)
    {
        R_DrawWaterPoly(surf);
        return;
    }

    R_DrawTexturedPoly(surf);
}

// ============================================================================
// BSP Traversal
// ============================================================================

static void R_RecursiveWorldNode(model_t* world, mnode_t* node)
{
    int c, side;
    mplane_t* plane;
    msurface_t* surf;
    float dot;

    if (!world || !node) return;
    if (node->contents < 0) return;

    plane = node->plane;

    switch (plane->type)
    {
    case PLANE_X: dot = r_refdef.vieworg[0] - plane->dist; break;
    case PLANE_Y: dot = r_refdef.vieworg[1] - plane->dist; break;
    case PLANE_Z: dot = r_refdef.vieworg[2] - plane->dist; break;
    default:      dot = DotProduct(r_refdef.vieworg, plane->normal) - plane->dist; break;
    }

    side = (dot < 0) ? 1 : 0;

    R_RecursiveWorldNode(world, node->children[side]);

    c = node->numsurfaces;
    surf = world->surfaces + node->firstsurface;

    for (; c; c--, surf++)
        R_DrawSurface(surf);

    R_RecursiveWorldNode(world, node->children[!side]);
}

// ============================================================================
// Главные функции
// ============================================================================

void R_DrawWorld(void)
{
    model_t* world = g_worldmodel ? g_worldmodel : cl_worldmodel;
    if (!world) return;

    // Используем VBO если готов
    if (R_WorldVBOReady())
    {
        R_DrawWorldVBO();
        return;
    }

    glDisable(GL_CULL_FACE);

    r_framecount++;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1, 1, 1, 1);

    static int printed = 0;

    if (!printed)
    {
        for (int i = 0; i < world->numsurfaces && i < 20; i++)
        {
            msurface_t* surf = &world->surfaces[i];
            if (!surf->polys) continue;

            int texid = 0;
            char* texname = "NULL";

            if (surf->texinfo && surf->texinfo->texture)
            {
                texid = surf->texinfo->texture->gl_texturenum;
                texname = surf->texinfo->texture->name;
            }

            mplane_t* plane = surf->plane;
            float nz = plane ? fabsf(plane->normal[2]) : -1;

            //Con_Printf("Surf[%d]: tex='%s' glid=%d nz=%.2f verts=%d\n",
            //    i, texname, texid, nz,
            //    surf->polys ? surf->polys->numverts : 0);
        }
        printed = 1;
    }

    for (int i = 0; i < world->numsurfaces; i++)
    {
        msurface_t* surf = &world->surfaces[i];
        if (!surf->polys)
            continue;

        if (surf->texinfo && surf->texinfo->texture)
            GL_Bind(surf->texinfo->texture->gl_texturenum);
        else
            GL_Bind(0);

        glpoly_t* p = surf->polys;
        while (p)
        {
            glBegin(GL_POLYGON);
            for (int j = 0; j < p->numverts; j++)
            {
                float* v = p->verts[j];
                glTexCoord2f(v[3], v[4]);
                glVertex3f(v[0], v[1], v[2]);
            }
            glEnd();

            glBegin(GL_POLYGON);
            for (int j = p->numverts - 1; j >= 0; j--)
            {
                float* v = p->verts[j];
                glTexCoord2f(v[3], v[4]);
                glVertex3f(v[0], v[1], v[2]);
            }
            glEnd();

            p = p->next;
        }
    }

    // ═══ Восстановление состояния ═══
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glColor4f(1, 1, 1, 1);
}

void R_DrawWorldNoSky(void)
{
    // Теперь не нужна - sky рисуется как обычные браши
    R_DrawWorld();
}

mleaf_t* Mod_PointInLeaf(vec3_t p, model_t* model)
{
    mnode_t* node;
    float d;
    mplane_t* plane;

    if (!model || !model->nodes)
        return NULL;

    node = model->nodes;
    while (1)
    {
        if (node->contents < 0)
            return (mleaf_t*)node;

        if (!node->plane)
            return NULL;

        plane = node->plane;
        d = DotProduct(p, plane->normal) - plane->dist;

        if (d > 0)
            node = node->children[0];
        else
            node = node->children[1];
    }

    return NULL;
}
