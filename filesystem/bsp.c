// filesystem/bsp.c
#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "mathlib.h"
#include "bsp.h"
#include "qtp.h"
#include "wad.h"
#include "gl_local.h"

static model_t* loadmodel;
static byte mod_base[64 * 1024 * 1024]; // 64MB buffer for loading
extern byte host_basepal[768];

extern void SCR_UpdateLoading(const char* msg, float progress);

// === Lightmaps ===
void GL_CreateLightmaps(model_t* model);
void GL_FreeLightmaps(model_t* model);

// === Surface rendering ===
void R_DrawTexturedPoly(msurface_t* surf);
void R_DrawSkyPoly(msurface_t* surf);
void R_DrawWaterPoly(msurface_t* surf);
void R_DrawWorld(model_t* world);

// ============================================================================
// Loading helper macros
// ============================================================================

#define LOAD_LUMP(lump, dest, type) \
    dest = (type *)(mod_base + l->offset); \
    if (l->length % sizeof(type)) \
        Sys_Error("Mod_LoadBrushModel: funny lump size");

// ============================================================================
// Load functions for each lump
// ============================================================================

static void Mod_LoadVertexes(lump_t* l)
{
    dvertex_t* in;
    mvertex_t* out;
    int i, count;

    in = (dvertex_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadVertexes: funny lump size");

    count = l->length / sizeof(*in);
    out = (mvertex_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->vertexes = out;
    loadmodel->numvertexes = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        out->position[0] = LittleFloat(in->point[0]);
        out->position[1] = LittleFloat(in->point[1]);
        out->position[2] = LittleFloat(in->point[2]);
    }

    Con_Printf("Loaded %d vertices. First: %f %f %f\n", count,
        loadmodel->vertexes[0].position[0],
        loadmodel->vertexes[0].position[1],
        loadmodel->vertexes[0].position[2]);
}

static void Mod_LoadEdges(lump_t* l)
{
    dedge_t* in;
    medge_t* out;
    int i, count;

    in = (dedge_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadEdges: funny lump size");

    count = l->length / sizeof(*in);
    out = (medge_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->edges = out;
    loadmodel->numedges = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        out->v[0] = LittleShort(in->v[0]);
        out->v[1] = LittleShort(in->v[1]);
    }
}

static void Mod_LoadSurfedges(lump_t* l)
{
    int* in, * out;
    int i, count;

    in = (int*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadSurfedges: funny lump size");

    count = l->length / sizeof(*in);
    out = (int*)Mem_Alloc(count * sizeof(*out));

    loadmodel->surfedges = out;
    loadmodel->numsurfedges = count;

    for (i = 0; i < count; i++)
        out[i] = LittleLong(in[i]);
}

static void Mod_LoadPlanes(lump_t* l)
{
    dplane_t* in;
    mplane_t* out;
    int i, j, count;
    int bits;

    in = (dplane_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadPlanes: funny lump size");

    count = l->length / sizeof(*in);
    out = (mplane_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->planes = out;
    loadmodel->numplanes = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        bits = 0;
        for (j = 0; j < 3; j++)
        {
            out->normal[j] = LittleFloat(in->normal[j]);
            if (out->normal[j] < 0)
                bits |= (1 << j);
        }

        out->dist = LittleFloat(in->dist);
        out->type = LittleLong(in->type);
        out->signbits = bits;
    }
}

static void Mod_LoadNodes(lump_t* l)
{
    dnode_t* in;
    mnode_t* out;
    int i, j, count, p;

    in = (dnode_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadNodes: funny lump size");

    count = l->length / sizeof(*in);
    out = (mnode_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->nodes = out;
    loadmodel->numnodes = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        for (j = 0; j < 3; j++)
        {
            out->mins[j] = LittleShort(in->mins[j]);
            out->maxs[j] = LittleShort(in->maxs[j]);
        }

        p = LittleLong(in->planenum);
        out->plane = loadmodel->planes + p;

        out->firstsurface = LittleShort(in->firstface);
        out->numsurfaces = LittleShort(in->numfaces);

        for (j = 0; j < 2; j++)
        {
            p = LittleLong(in->children[j]);
            if (p >= 0)
                out->children[j] = loadmodel->nodes + p;
            else
                out->children[j] = (mnode_t*)(loadmodel->leafs + (-1 - p));
        }
    }
}

static void Mod_LoadLeafs(lump_t* l)
{
    byte* data;
    mleaf_t* out;
    int i, count;
    int raw_leaf_size;

    data = mod_base + l->offset;

    // Определяем размер листа по данным
    // Пробуем 28 байт (стандартный BSP29 с ambient)
    if (l->length % 28 == 0)
        raw_leaf_size = 28;
    else if (l->length % 24 == 0)
        raw_leaf_size = 24;
    else
        Sys_Error("Mod_LoadLeafs: bad lump size %d", l->length);

    count = l->length / raw_leaf_size;
    out = (mleaf_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->leafs = out;
    loadmodel->numleafs = count;

    Con_Printf("Loading %d leafs (raw size=%d)\n", count, raw_leaf_size);

    for (i = 0; i < count; i++, out++)
    {
        byte* src = data + i * raw_leaf_size;

        int* ints = (int*)src;
        short* shorts = (short*)(src + 8);
        unsigned short* ushorts = (unsigned short*)(src + 20);

        out->contents = LittleLong(ints[0]);
        int visofs = LittleLong(ints[1]);

        for (int j = 0; j < 3; j++)
        {
            out->mins[j] = LittleShort(shorts[j]);
            out->maxs[j] = LittleShort(shorts[j + 3]);
        }

        out->firstmarksurface = loadmodel->marksurfaces +
            LittleShort(ushorts[0]);
        out->nummarksurfaces = LittleShort(ushorts[1]);

        if (visofs == -1)
            out->compressed_vis = NULL;
        else
            out->compressed_vis = loadmodel->visdata + visofs;

        for (int j = 0; j < 4; j++)
            out->ambient_sound_level[j] = 0;
    }
}

static void Mod_LoadClipnodes(lump_t* l)
{
    dclipnode_t* in, * out;
    int i, count;
    hull_t* hull;

    in = (dclipnode_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadClipnodes: funny lump size");

    count = l->length / sizeof(*in);
    out = (dclipnode_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->clipnodes = out;
    loadmodel->numclipnodes = count;

    // Setup hulls (player hitboxes)
    // Hull 0 - точечный hull (использует nodes)
    hull = &loadmodel->hulls[0];
    hull->clipnodes = (dclipnode_t*)loadmodel->nodes;  // type cast для совместимости
    hull->planes = loadmodel->planes;
    hull->firstclipnode = 0;
    hull->lastclipnode = loadmodel->numnodes - 1;
    VectorSet(hull->clip_mins, 0, 0, 0);
    VectorSet(hull->clip_maxs, 0, 0, 0);

    // Hull 1 - стоящий игрок (32x32x72)
    hull = &loadmodel->hulls[1];
    hull->clipnodes = out;
    hull->planes = loadmodel->planes;
    hull->firstclipnode = 0;
    hull->lastclipnode = count - 1;
    VectorSet(hull->clip_mins, -16, -16, -24);
    VectorSet(hull->clip_maxs, 16, 16, 32);

    // Hull 2 - большие монстры (64x64x64)
    hull = &loadmodel->hulls[2];
    hull->clipnodes = out;
    hull->planes = loadmodel->planes;
    hull->firstclipnode = 0;
    hull->lastclipnode = count - 1;
    VectorSet(hull->clip_mins, -32, -32, -32);
    VectorSet(hull->clip_maxs, 32, 32, 32);

    // Hull 3 - сидящий игрок (32x32x36)
    hull = &loadmodel->hulls[3];
    hull->clipnodes = out;
    hull->planes = loadmodel->planes;
    hull->firstclipnode = 0;
    hull->lastclipnode = count - 1;
    VectorSet(hull->clip_mins, -16, -16, -18);
    VectorSet(hull->clip_maxs, 16, 16, 18);

    for (i = 0; i < count; i++, in++, out++)
    {
        out->planenum = LittleLong(in->planenum);
        out->children[0] = LittleShort(in->children[0]);
        out->children[1] = LittleShort(in->children[1]);
    }
}

static void Mod_LoadEntities(lump_t* l)
{
    if (!l->length)
    {
        loadmodel->entities = NULL;
        return;
    }

    loadmodel->entities = (char*)Mem_Alloc(l->length);
    memcpy(loadmodel->entities, mod_base + l->offset, l->length);
}

static void Mod_LoadVisibility(lump_t* l)
{
    if (!l->length)
    {
        loadmodel->visdata = NULL;
        return;
    }

    loadmodel->visdata = (byte*)Mem_Alloc(l->length);
    memcpy(loadmodel->visdata, mod_base + l->offset, l->length);
}

static void Mod_LoadLighting(lump_t* l)
{
    if (!l->length)
    {
        loadmodel->lightdata = NULL;
        return;
    }

    loadmodel->lightdata = (byte*)Mem_Alloc(l->length);
    memcpy(loadmodel->lightdata, mod_base + l->offset, l->length);

    for (int i = 0; i < loadmodel->numsurfaces; i++) {
        msurface_t* s = &loadmodel->surfaces[i];
        if (!loadmodel->lightdata || s->lightofs == -1)
            s->samples = NULL;
        else
            s->samples = loadmodel->lightdata + s->lightofs;
    }
}

static void Mod_LoadTextures(lump_t* l)
{
    Con_Printf("=== Loading textures: offset=%d, length=%d ===\n", l->offset, l->length);

    miptex_t* mt;
    texture_t* tx;
    texture_t* anims[10];
    texture_t* altanims[10];
    int i, j, num, max, altmax;
    int* miptex_offsets;

    if (!l->length)
    {
        loadmodel->numtextures = 0;
        loadmodel->textures = NULL;
        return;
    }

    miptex_offsets = (int*)(mod_base + l->offset);
    loadmodel->numtextures = LittleLong(miptex_offsets[0]);
    loadmodel->textures = (texture_t**)Mem_Alloc(loadmodel->numtextures * sizeof(texture_t*));

    for (i = 0; i < loadmodel->numtextures; i++)
    {
        int ofs = LittleLong(miptex_offsets[i + 1]);
        if (ofs == -1)
            continue;

        mt = (miptex_t*)(mod_base + l->offset + ofs);

        tx = (texture_t*)Mem_Alloc(sizeof(texture_t));
        loadmodel->textures[i] = tx;

        Q_strncpy(tx->name, mt->name, sizeof(tx->name));
        tx->width = LittleLong(mt->width);
        tx->height = LittleLong(mt->height);

        // Загрузка пикселей
        int pixels_ofs = LittleLong(mt->offsets[0]);
        if (pixels_ofs > 0)
        {
            byte* src = (byte*)mt + pixels_ofs;
            byte* rgba = (byte*)Mem_Alloc(tx->width * tx->height * 4);

            for (j = 0; j < tx->width * tx->height; j++)
            {
                byte idx = src[j];
                rgba[j * 4 + 0] = host_basepal[idx * 3 + 0];
                rgba[j * 4 + 1] = host_basepal[idx * 3 + 1];
                rgba[j * 4 + 2] = host_basepal[idx * 3 + 2];

                // Прозрачность для неба (индекс 0) и fence-текстур (индекс 255)
                if (idx == 0 || idx == 255)
                    rgba[j * 4 + 3] = 0;    // Прозрачный
                else
                    rgba[j * 4 + 3] = 255;  // Непрозрачный
            }

            tx->gl_texturenum = GL_LoadTexture(tx->name, rgba, tx->width, tx->height, TF_ALPHA);
            if (!strncmp(tx->name, "sky", 3))
            {
                R_InitSkyFromRGBA(rgba, tx->width, tx->height);
            }
            Mem_Free(rgba);
        }
        else 
        {
            tx->gl_texturenum = QTP_LoadTexture(tx->name, tx->width, tx->height);
        }

        // Инициализация анимации
        tx->anim_total = 0;
        tx->anim_min = 0;
        tx->anim_max = 0;
        tx->anim_next = NULL;
        tx->alternate_anims = NULL;
    }

    // === СВЯЗЫВАНИЕ АНИМИРОВАННЫХ ТЕКСТУР ===
    for (i = 0; i < loadmodel->numtextures; i++)
    {
        tx = loadmodel->textures[i];
        if (!tx || tx->name[0] != '+')
            continue;
        if (tx->anim_next)
            continue;

        memset(anims, 0, sizeof(anims));
        memset(altanims, 0, sizeof(altanims));

        max = 0;
        altmax = 0;

        for (j = i; j < loadmodel->numtextures; j++)
        {
            texture_t* tx2 = loadmodel->textures[j];
            if (!tx2 || tx2->name[0] != '+')
                continue;
            if (strcmp(tx->name + 2, tx2->name + 2))
                continue;

            num = tx2->name[1];

            if (num >= 'a' && num <= 'j')
            {
                num -= 'a';
                altanims[num] = tx2;
                if (num + 1 > altmax)
                    altmax = num + 1;
            }
            else if (num >= '0' && num <= '9')
            {
                num -= '0';
                anims[num] = tx2;
                if (num + 1 > max)
                    max = num + 1;
            }
        }

        for (j = 0; j < max; j++)
        {
            tx = anims[j];
            if (!tx)
                continue;
            tx->anim_total = max;
            tx->anim_min = j;
            tx->anim_max = (j + 1) % max;
            tx->anim_next = anims[(j + 1) % max];
            if (altmax)
                tx->alternate_anims = altanims[0];
        }

        for (j = 0; j < altmax; j++)
        {
            tx = altanims[j];
            if (!tx)
                continue;
            tx->anim_total = altmax;
            tx->anim_min = j;
            tx->anim_max = (j + 1) % altmax;
            tx->anim_next = altanims[(j + 1) % altmax];
            if (max)
                tx->alternate_anims = anims[0];
        }
    }

    Con_Printf("Loaded %d textures\n", loadmodel->numtextures);
}

static void Mod_LoadTexinfo(lump_t* l)
{
    texinfo_t* in;
    mtexinfo_t* out;
    int i, count;

    in = (texinfo_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadTexinfo: funny lump size");

    count = l->length / sizeof(*in);
    out = (mtexinfo_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->numtexinfo = count;
    loadmodel->texinfo = out;

    Con_Printf("Loading %d texinfo, numtextures=%d\n", count, loadmodel->numtextures);

    for (i = 0; i < count; i++, in++, out++)
    {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 4; k++)
                out->vecs[j][k] = LittleFloat(in->vecs[j][k]);
        }

        int miptex = LittleLong(in->miptex);

        if (miptex < 0 || miptex >= loadmodel->numtextures) {
            Con_Printf("Texinfo %d: bad miptex=%d (max=%d)\n", i, miptex, loadmodel->numtextures);
            miptex = 0;
        }

        out->flags = LittleLong(in->flags);

        if (miptex >= loadmodel->numtextures) {
            Con_Printf("Texinfo with bad texture index\n");
            miptex = 0;
        }

        if (loadmodel->textures)
            out->texture = loadmodel->textures[miptex];
    }
}

static void CalcSurfaceExtents(msurface_t* s)
{
    float mins[2], maxs[2], val;
    int i, j, e;
    mvertex_t* v;
    mtexinfo_t* tex;

    mins[0] = mins[1] = 999999;
    maxs[0] = maxs[1] = -999999;

    tex = s->texinfo;

    // Проходим по всем вершинам поверхности
    for (i = 0; i < s->numedges; i++)
    {
        e = loadmodel->surfedges[s->firstedge + i];

        if (e >= 0)
            v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
        else
            v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

        // Считаем S и T для этой вершины
        for (j = 0; j < 2; j++)
        {
            // S = dot(pos, vec[j].xyz) + vec[j].w
            val = v->position[0] * tex->vecs[j][0] +
                v->position[1] * tex->vecs[j][1] +
                v->position[2] * tex->vecs[j][2] +
                tex->vecs[j][3];

            if (val < mins[j]) mins[j] = val;
            if (val > maxs[j]) maxs[j] = val;
        }
    }

    // Округляем до границ 16 (размер одного лайтмап-сэмпла в world units)
    for (i = 0; i < 2; i++)
    {
        int bmins = (int)floor(mins[i] / 16.0f);
        int bmaxs = (int)ceil(maxs[i] / 16.0f);

        s->texturemins[i] = bmins * 16;
        s->extents[i] = (bmaxs - bmins) * 16;

        // Проверка на слишком большие поверхности
        if (s->extents[i] > 512)
        {
            Con_Printf("WARNING: surface extents %d > 512\n", s->extents[i]);
        }
    }
}

static void Mod_LoadFaces(lump_t* l)
{
    dface_t* in;
    msurface_t* out;
    int i, count, planenum, side;

    in = (dface_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadFaces: funny lump size");

    count = l->length / sizeof(*in);
    out = (msurface_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->surfaces = out;
    loadmodel->numsurfaces = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        planenum = LittleShort(in->planenum);
        side = LittleShort(in->side);

        out->plane = &loadmodel->planes[planenum];
        out->flags = 0;
        if (side) out->flags |= SURF_PLANEBACK;

        out->firstedge = LittleLong(in->firstedge);
        out->numedges = LittleShort(in->numedges);

        // Texinfo
        int texinfo_idx = LittleShort(in->texinfo);
        out->texinfo = &loadmodel->texinfo[texinfo_idx];

        out->lightofs = LittleLong(in->lightofs);

        // Стили освещения (для анимированного света)
        for (int j = 0; j < 4; j++)
            out->styles[j] = in->styles[j];

        // Флаги по имени текстуры (sky, water, etc.)
        if (out->texinfo && out->texinfo->texture)
        {
            char* name = out->texinfo->texture->name;
            if (name[0] == 's' && name[1] == 'k' && name[2] == 'y')
                out->flags |= SURF_DRAWSKY;
            else if (name[0] == '*')
                out->flags |= SURF_DRAWTURB;
        }

        CalcSurfaceExtents(out);
    }

    Con_Printf("Loaded %d faces\n", count);
}

// Создание glpoly_t для поверхности
void Mod_MakeSurfaces(model_t* model)
{
    msurface_t* surf;
    glpoly_t* poly;
    int i, j, e_idx;
    mvertex_t* v;

    for (i = 0; i < model->numsurfaces; i++)
    {
        surf = &model->surfaces[i];

        // Размер лайтмапа в texels
        int smax = (surf->extents[0] >> 4) + 1;
        int tmax = (surf->extents[1] >> 4) + 1;

        // Выделяем полигон
        poly = (glpoly_t*)Mem_Alloc(sizeof(glpoly_t) +
            (surf->numedges - 4) * VERTEXSIZE * sizeof(float));

        poly->next = surf->polys;
        surf->polys = poly;
        poly->numverts = surf->numedges;

        for (j = 0; j < surf->numedges; j++)
        {
            e_idx = model->surfedges[surf->firstedge + j];

            if (e_idx >= 0)
                v = &model->vertexes[model->edges[e_idx].v[0]];
            else
                v = &model->vertexes[model->edges[-e_idx].v[1]];

            float* dst = poly->verts[j];

            // Позиция
            dst[0] = v->position[0];
            dst[1] = v->position[1];
            dst[2] = v->position[2];

            // Считаем "сырые" S/T
            float s = DotProduct(v->position, surf->texinfo->vecs[0])
                + surf->texinfo->vecs[0][3];
            float t = DotProduct(v->position, surf->texinfo->vecs[1])
                + surf->texinfo->vecs[1][3];

            // Base texture UV
            if (surf->texinfo->texture)
            {
                dst[3] = s / surf->texinfo->texture->width;
                dst[4] = t / surf->texinfo->texture->height;
            }
            else
            {
                dst[3] = s / 64.0f;
                dst[4] = t / 64.0f;
            }

            // === LIGHTMAP UV ===
            // Переводим world coords -> lightmap texel coords -> 0..1
            dst[5] = (s - surf->texturemins[0]) / 16.0f / (float)smax;
            dst[6] = (t - surf->texturemins[1]) / 16.0f / (float)tmax;

            // Сдвиг на пол-пикселя (чтобы сэмплить центр texel)
            dst[5] += 0.5f / smax;
            dst[6] += 0.5f / tmax;
        }
    }
}

static void Mod_LoadMarksurfaces(lump_t* l)
{
    short* in;
    msurface_t** out;
    int i, count;

    in = (short*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadMarksurfaces: funny lump size");

    count = l->length / sizeof(*in);
    out = (msurface_t**)Mem_Alloc(count * sizeof(*out));

    loadmodel->marksurfaces = out;
    loadmodel->nummarksurfaces = count;

    for (i = 0; i < count; i++)
    {
        int j = LittleShort(in[i]);
        if (j >= loadmodel->numsurfaces)
            Sys_Error("Mod_LoadMarksurfaces: bad surface number");
        out[i] = loadmodel->surfaces + j;
    }
}

static void Mod_LoadSubmodels(lump_t* l)
{
    dmodel_t* in;
    dmodel_t* out;
    int i, count;

    in = (dmodel_t*)(mod_base + l->offset);
    if (l->length % sizeof(*in))
        Sys_Error("Mod_LoadSubmodels: funny lump size");

    count = l->length / sizeof(*in);
    out = (dmodel_t*)Mem_Alloc(count * sizeof(*out));

    loadmodel->submodels = out;
    loadmodel->numsubmodels = count;

    for (i = 0; i < count; i++, in++, out++)
    {
        for (int j = 0; j < 3; j++)
        {
            out->mins[j] = LittleFloat(in->mins[j]);
            out->maxs[j] = LittleFloat(in->maxs[j]);
            out->origin[j] = LittleFloat(in->origin[j]);
        }

        out->headnode = LittleLong(in->headnode);

        out->firstface = LittleLong(in->firstface);
        out->numfaces = LittleLong(in->numfaces);
    }

    Con_Printf("Loaded %d submodels\n", count);
}

// ============================================================================
// Main loader
// ============================================================================

model_t* Mod_LoadBrushModel(const char* name)
{
    SCR_UpdateLoading("Loading file...", 0.1f);

    int length = 0;
    byte* buf = (byte*)COM_LoadFile(name, &length);
    if (!buf)
    {
        Con_Printf("Mod_LoadBrushModel: %s not found\n", name);
        return NULL;
    }

    if (length < (int)sizeof(dheader_t))
    {
        COM_FreeFile(buf);
        Sys_Error("Mod_LoadBrushModel: %s is too small", name);
    }

    if (length > (int)sizeof(mod_base))
    {
        COM_FreeFile(buf);
        Sys_Error("Mod_LoadBrushModel: %s is too large", name);
    }

    // Копируем BSP целиком в mod_base, как раньше
    memcpy(mod_base, buf, length);
    COM_FreeFile(buf);

    dheader_t* header = (dheader_t*)mod_base;

    // Проверяем magic/version твоего формата
    int magic = header->magic;
    int version = header->version;

    if (magic != QBSP_MAGIC || version != QBSP_VERSION)
    {
        Sys_Error("Mod_LoadBrushModel: %s has wrong magic/version (%08X/%d, expected %08X/%d)",
            name, magic, version, QBSP_MAGIC, QBSP_VERSION);
    }

    for (int i = 0; i < LUMP_COUNT; i++)
    {
        header->lumps[i].offset = LittleLong(header->lumps[i].offset);
        header->lumps[i].length = LittleLong(header->lumps[i].length);
    }

    // Выделяем модель
    loadmodel = (model_t*)Mem_Alloc(sizeof(model_t));
    Q_strncpy(loadmodel->name, name, sizeof(loadmodel->name));

    for (int i = 0; i < 4; i++)
        loadmodel->hulls[i].firstclipnode = -1;

    SCR_UpdateLoading("Loading vertices...", 0.05f);
    Mod_LoadVertexes(&header->lumps[LUMP_VERTICES]);
    Con_Printf("DEBUG: Vertices loaded\n");

    SCR_UpdateLoading("Loading edges...", 0.10f);
    Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
    Con_Printf("DEBUG: Edges loaded\n");

    SCR_UpdateLoading("Loading surfedges...", 0.15f);
    Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
    Con_Printf("DEBUG: surfedges loaded\n");

    SCR_UpdateLoading("Loading planes...", 0.20f);
    Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
    Con_Printf("DEBUG: planes loaded\n");

    SCR_UpdateLoading("Loading textures...", 0.25f);
    Mod_LoadTextures(&header->lumps[LUMP_TEXTURES]);
    Con_Printf("DEBUG: textures loaded\n");

    SCR_UpdateLoading("Loading texinfo...", 0.30f);
    Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
    Con_Printf("DEBUG: texinfo loaded\n");

    SCR_UpdateLoading("Loading faces...", 0.35f);
    Mod_LoadFaces(&header->lumps[LUMP_FACES]);
    Con_Printf("DEBUG: faces loaded\n");

    SCR_UpdateLoading("Loading marksurfaces...", 0.40f);
    Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
    Con_Printf("DEBUG: marksurfaces loaded\n");

    SCR_UpdateLoading("Loading visibility...", 0.45f);
    Mod_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
    Con_Printf("DEBUG: visibility loaded\n");

    SCR_UpdateLoading("Loading leafs...", 0.50f);
    Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
    Con_Printf("DEBUG: Vertices loaded\n");

    SCR_UpdateLoading("Loading nodes...", 0.55f);
    Mod_LoadNodes(&header->lumps[LUMP_NODES]);
    Con_Printf("DEBUG: nodes loaded\n");

    // Генерация clipnodes для Hull 0
    loadmodel->numclipnodes = loadmodel->numnodes;
    loadmodel->clipnodes = (dclipnode_t*)Mem_Alloc(loadmodel->numclipnodes * sizeof(dclipnode_t));

    for (int i = 0; i < loadmodel->numnodes; i++)
    {
        mnode_t* node = &loadmodel->nodes[i];
        dclipnode_t* clip = &loadmodel->clipnodes[i];

        clip->planenum = (int)(node->plane - loadmodel->planes);

        for (int j = 0; j < 2; j++)
        {
            mnode_t* child = node->children[j];

            // Вычисляем, попадает ли указатель в массив nodes
            int node_index = (int)(child - loadmodel->nodes);

            if (node_index >= 0 && node_index < loadmodel->numnodes)
            {
                // Это нода
                clip->children[j] = (short)node_index;
            }
            else
            {
                // Это лист - извлекаем contents
                mleaf_t* leaf = (mleaf_t*)child;
                clip->children[j] = (short)leaf->contents;
            }
        }
    }

    // Настраиваем Hull 0
    hull_t* hull = &loadmodel->hulls[0];
    hull->clipnodes = loadmodel->clipnodes;
    hull->planes = loadmodel->planes;
    hull->firstclipnode = 0;
    hull->lastclipnode = loadmodel->numclipnodes - 1;
    VectorSet(hull->clip_mins, 0, 0, 0);
    VectorSet(hull->clip_maxs, 0, 0, 0);

    // Временно используем Hull 0 для всех размеров (Hull 1, 2, 3)
    // Чтобы игрок (Hull 1) не проваливался
    for (int i = 1; i < 4; i++)
    {
        loadmodel->hulls[i] = loadmodel->hulls[0];

        // Размеры можно оставить правильными, но дерево то же
        if (i == 1) { VectorSet(loadmodel->hulls[i].clip_mins, -16, -16, -24); VectorSet(loadmodel->hulls[i].clip_maxs, 16, 16, 32); }
        if (i == 2) { VectorSet(loadmodel->hulls[i].clip_mins, -32, -32, -32); VectorSet(loadmodel->hulls[i].clip_maxs, 32, 32, 32); }
    }

    SCR_UpdateLoading("Loading entities...", 0.70f);
    Mod_LoadEntities(&header->lumps[LUMP_ENTITIES]);
    Con_Printf("DEBUG: entities loaded\n");

    SCR_UpdateLoading("Loading lighting...", 0.80f);
    Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
    Con_Printf("DEBUG: lighting loaded\n");

    SCR_UpdateLoading("Linking lightmaps...", 0.82f);
    for (int i = 0; i < loadmodel->numsurfaces; i++)
    {
        msurface_t* s = &loadmodel->surfaces[i];

        if (!loadmodel->lightdata || s->lightofs == -1)
            s->samples = NULL;
        else
            s->samples = loadmodel->lightdata + s->lightofs;
    }
    Con_Printf("DEBUG: lightmaps loaded\n");

    SCR_UpdateLoading("Building polygons...", 0.90f);
    Mod_MakeSurfaces(loadmodel);
    Con_Printf("DEBUG: polygons loaded\n");

    SCR_UpdateLoading("Loading models...", 0.92f);
    Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);
    Con_Printf("DEBUG: models loaded\n");

    SCR_UpdateLoading("Creating lightmaps...", 0.95f);
    GL_CreateLightmaps(loadmodel);
    Con_Printf("DEBUG: lightmaps loaded\n");

    SCR_UpdateLoading("Done!", 1.0f);

    Con_Printf("Loaded %s (magic=%08X, version=%d)\n", name, magic, version);

    R_BuildWorldVBO(loadmodel);

    return loadmodel;
}

void Mod_FreeModel(model_t* mod) {
    if (!mod)
        return;

    // Освобождаем лайтмапы
    GL_FreeLightmaps(mod);
    free(mod);
}
