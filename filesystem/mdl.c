// mdl.c

#include "quakedef.h"
#include "mdl.h"
#include "common.h"
#include "console.h"
#include "gl_local.h"

extern byte host_basepal[768];

static void Mod_CalcBounds(aliasmodel_t* mod);

static void Mod_CalcBounds(aliasmodel_t* mod)
{
    int i, j;

    mod->mins[0] = mod->mins[1] = mod->mins[2] = 999999.0f;
    mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = -999999.0f;

    for (i = 0; i < mod->numframes; i++)
    {
        trivertx_t* verts = mod->frames[i];
        if (!verts) continue;

        for (j = 0; j < mod->numverts; j++)
        {
            float x = verts[j].v[0] * mod->scale[0] + mod->scale_origin[0];
            float y = verts[j].v[1] * mod->scale[1] + mod->scale_origin[1];
            float z = verts[j].v[2] * mod->scale[2] + mod->scale_origin[2];

            if (x < mod->mins[0]) mod->mins[0] = x;
            if (y < mod->mins[1]) mod->mins[1] = y;
            if (z < mod->mins[2]) mod->mins[2] = z;

            if (x > mod->maxs[0]) mod->maxs[0] = x;
            if (y > mod->maxs[1]) mod->maxs[1] = y;
            if (z > mod->maxs[2]) mod->maxs[2] = z;
        }
    }
}

aliasmodel_t* Mod_LoadAliasModel(const char* name)
{
    byte* buf;
    int length;
    mdl_header_t* header;
    aliasmodel_t* mod;
    int i, j;
    byte* ptr;

    buf = (byte*)COM_LoadFile(name, &length);
    if (!buf)
    {
        Con_Printf("Mod_LoadAliasModel: %s not found\n", name);
        return NULL;
    }

    header = (mdl_header_t*)buf;

    if (LittleLong(header->ident) != IDPOLYHEADER)
    {
        Con_Printf("Mod_LoadAliasModel: %s is not an alias model\n", name);
        COM_FreeFile(buf);
        return NULL;
    }

    if (LittleLong(header->version) != ALIAS_VERSION)
    {
        Con_Printf("Mod_LoadAliasModel: %s has wrong version\n", name);
        COM_FreeFile(buf);
        return NULL;
    }

    mod = (aliasmodel_t*)Mem_Alloc(sizeof(aliasmodel_t));
    Q_strncpy(mod->name, name, sizeof(mod->name));

    mod->numframes = LittleLong(header->numframes);
    mod->numverts = LittleLong(header->numverts);
    mod->numtris = LittleLong(header->numtris);
    mod->skinwidth = LittleLong(header->skinwidth);
    mod->skinheight = LittleLong(header->skinheight);
    mod->numskins = LittleLong(header->numskins);

    mod->scale[0] = LittleFloat(header->scale[0]);
    mod->scale[1] = LittleFloat(header->scale[1]);
    mod->scale[2] = LittleFloat(header->scale[2]);
    mod->scale_origin[0] = LittleFloat(header->scale_origin[0]);
    mod->scale_origin[1] = LittleFloat(header->scale_origin[1]);
    mod->scale_origin[2] = LittleFloat(header->scale_origin[2]);

    ptr = buf + sizeof(mdl_header_t);

    // === Загрузка скинов ===
    mod->skin_textures = (GLuint*)Mem_Alloc(sizeof(GLuint) * mod->numskins);

    for (i = 0; i < mod->numskins; i++)
    {
        int skintype = LittleLong(*(int*)ptr);
        ptr += 4;

        int pixels = mod->skinwidth * mod->skinheight;

        if (skintype == 0)  // single skin
        {
            byte* rgba = (byte*)Mem_Alloc(pixels * 4);

            for (j = 0; j < pixels; j++)
            {
                byte idx = ptr[j];
                rgba[j * 4 + 0] = host_basepal[idx * 3 + 0];
                rgba[j * 4 + 1] = host_basepal[idx * 3 + 1];
                rgba[j * 4 + 2] = host_basepal[idx * 3 + 2];
                rgba[j * 4 + 3] = 255;
            }

            char texname[128];
            snprintf(texname, sizeof(texname), "%s_skin%d", name, i);
            mod->skin_textures[i] = GL_LoadTexture(texname, rgba,
                mod->skinwidth, mod->skinheight, 0);

            Mem_Free(rgba);
            ptr += pixels;
        }
        else  // skin group
        {
            int numskins = LittleLong(*(int*)ptr);
            ptr += 4;
            ptr += numskins * 4;  // intervals

            byte* rgba = (byte*)Mem_Alloc(pixels * 4);

            for (j = 0; j < pixels; j++)
            {
                byte idx = ptr[j];
                rgba[j * 4 + 0] = host_basepal[idx * 3 + 0];
                rgba[j * 4 + 1] = host_basepal[idx * 3 + 1];
                rgba[j * 4 + 2] = host_basepal[idx * 3 + 2];
                rgba[j * 4 + 3] = 255;
            }

            char texname[128];
            snprintf(texname, sizeof(texname), "%s_skin%d", name, i);
            mod->skin_textures[i] = GL_LoadTexture(texname, rgba,
                mod->skinwidth, mod->skinheight, 0);

            Mem_Free(rgba);
            ptr += pixels * numskins;
        }
    }

    // === Текстурные координаты ===
    mod->texcoords = (stvert_t*)Mem_Alloc(sizeof(stvert_t) * mod->numverts);
    memcpy(mod->texcoords, ptr, sizeof(stvert_t) * mod->numverts);

    for (i = 0; i < mod->numverts; i++)
    {
        mod->texcoords[i].onseam = LittleLong(mod->texcoords[i].onseam);
        mod->texcoords[i].s = LittleLong(mod->texcoords[i].s);
        mod->texcoords[i].t = LittleLong(mod->texcoords[i].t);
    }
    ptr += sizeof(stvert_t) * mod->numverts;

    // === Треугольники ===
    mod->triangles = (dtriangle_t*)Mem_Alloc(sizeof(dtriangle_t) * mod->numtris);
    memcpy(mod->triangles, ptr, sizeof(dtriangle_t) * mod->numtris);

    for (i = 0; i < mod->numtris; i++)
    {
        mod->triangles[i].facesfront = LittleLong(mod->triangles[i].facesfront);
        mod->triangles[i].vertindex[0] = LittleLong(mod->triangles[i].vertindex[0]);
        mod->triangles[i].vertindex[1] = LittleLong(mod->triangles[i].vertindex[1]);
        mod->triangles[i].vertindex[2] = LittleLong(mod->triangles[i].vertindex[2]);
    }
    ptr += sizeof(dtriangle_t) * mod->numtris;

    // Кадры анимации
    mod->frame_mins = (vec3_t*)Mem_Alloc(sizeof(vec3_t) * mod->numframes);
    mod->frame_maxs = (vec3_t*)Mem_Alloc(sizeof(vec3_t) * mod->numframes);

    mod->frames = (trivertx_t**)Mem_Alloc(sizeof(trivertx_t*) * mod->numframes);
    mod->frame_names = (char**)Mem_Alloc(sizeof(char*) * mod->numframes);

    for (i = 0; i < mod->numframes; i++)
    {
        int frametype = LittleLong(*(int*)ptr);
        ptr += 4;

        if (frametype == 0)
        {
            // single frame
            daliasframe_t* frame = (daliasframe_t*)ptr;

            mod->frame_mins[i][0] = frame->bboxmin.v[0] * mod->scale[0] + mod->scale_origin[0];
            mod->frame_mins[i][1] = frame->bboxmin.v[1] * mod->scale[1] + mod->scale_origin[1];
            mod->frame_mins[i][2] = frame->bboxmin.v[2] * mod->scale[2] + mod->scale_origin[2];

            mod->frame_maxs[i][0] = frame->bboxmax.v[0] * mod->scale[0] + mod->scale_origin[0];
            mod->frame_maxs[i][1] = frame->bboxmax.v[1] * mod->scale[1] + mod->scale_origin[1];
            mod->frame_maxs[i][2] = frame->bboxmax.v[2] * mod->scale[2] + mod->scale_origin[2];

            mod->frame_names[i] = (char*)Mem_Alloc(17);
            Q_strncpy(mod->frame_names[i], frame->name, 16);

            ptr += (sizeof(trivertx_t) * 2 + 16); // bboxmin+bboxmax+name

            mod->frames[i] = (trivertx_t*)Mem_Alloc(sizeof(trivertx_t) * mod->numverts);
            memcpy(mod->frames[i], ptr, sizeof(trivertx_t) * mod->numverts);
            ptr += sizeof(trivertx_t) * mod->numverts;
        }
        else
        {
            // frame group
            int groupframes = LittleLong(*(int*)ptr);
            ptr += 4;

            ptr += sizeof(trivertx_t) * 2;      // group bboxmin/bboxmax
            ptr += groupframes * 4;             // intervals

            // берём первый кадр группы
            daliasframe_t* frame = (daliasframe_t*)ptr;

            mod->frame_mins[i][0] = frame->bboxmin.v[0] * mod->scale[0] + mod->scale_origin[0];
            mod->frame_mins[i][1] = frame->bboxmin.v[1] * mod->scale[1] + mod->scale_origin[1];
            mod->frame_mins[i][2] = frame->bboxmin.v[2] * mod->scale[2] + mod->scale_origin[2];

            mod->frame_maxs[i][0] = frame->bboxmax.v[0] * mod->scale[0] + mod->scale_origin[0];
            mod->frame_maxs[i][1] = frame->bboxmax.v[1] * mod->scale[1] + mod->scale_origin[1];
            mod->frame_maxs[i][2] = frame->bboxmax.v[2] * mod->scale[2] + mod->scale_origin[2];

            mod->frame_names[i] = (char*)Mem_Alloc(17);
            Q_strncpy(mod->frame_names[i], frame->name, 16);

            ptr += (sizeof(trivertx_t) * 2 + 16); // bboxmin+bboxmax+name

            mod->frames[i] = (trivertx_t*)Mem_Alloc(sizeof(trivertx_t) * mod->numverts);
            memcpy(mod->frames[i], ptr, sizeof(trivertx_t) * mod->numverts);
            ptr += sizeof(trivertx_t) * mod->numverts;

            // пропускаем остальные кадры группы
            // каждый кадр = (bboxmin+bboxmax+name) + verts
            for (j = 1; j < groupframes; j++)
                ptr += (sizeof(trivertx_t) * 2 + 16) + sizeof(trivertx_t) * mod->numverts;
        }
    }
    Mod_CalcBounds(mod);
    COM_FreeFile(buf);

    Mod_FindAnimations(mod);

    Con_Printf("Loaded MDL: %s (%d frames, %d tris)\n",
        name, mod->numframes, mod->numtris);

    return mod;
}

void Mod_FindAnimations(aliasmodel_t* mod)
{
    int i;

    // Инициализация - всё на idle (кадр 0)
    for (i = 0; i < ANIM_COUNT; i++)
    {
        mod->anims[i].start = 0;
        mod->anims[i].end = 0;
        mod->anims[i].framerate = 10.0f;
        mod->anims[i].loop = true;
    }

    // Ищем анимации по именам кадров
    int idle_start = -1, idle_end = -1;
    int walk_start = -1, walk_end = -1;
    int run_start = -1, run_end = -1;

    for (i = 0; i < mod->numframes; i++)
    {
        const char* name = mod->frame_names[i];
        if (!name) continue;

        // IDLE: stand, idle
        if (!strncmp(name, "stand", 5) || !strncmp(name, "idle", 4))
        {
            if (idle_start < 0) idle_start = i;
            idle_end = i;
        }
        // WALK
        else if (!strncmp(name, "walk", 4))
        {
            if (walk_start < 0) walk_start = i;
            walk_end = i;
        }
        // RUN
        else if (!strncmp(name, "run", 3))
        {
            if (run_start < 0) run_start = i;
            run_end = i;
        }
    }

    // Устанавливаем найденные анимации
    if (idle_start >= 0)
    {
        mod->anims[ANIM_IDLE].start = idle_start;
        mod->anims[ANIM_IDLE].end = idle_end;
        mod->anims[ANIM_IDLE].loop = true;
    }

    if (walk_start >= 0)
    {
        mod->anims[ANIM_WALK].start = walk_start;
        mod->anims[ANIM_WALK].end = walk_end;
        mod->anims[ANIM_WALK].loop = true;
    }
    else if (run_start >= 0)
    {
        // Нет walk - используем run
        mod->anims[ANIM_WALK].start = run_start;
        mod->anims[ANIM_WALK].end = run_end;
        mod->anims[ANIM_WALK].loop = true;
    }

    if (run_start >= 0)
    {
        mod->anims[ANIM_RUN].start = run_start;
        mod->anims[ANIM_RUN].end = run_end;
        mod->anims[ANIM_RUN].loop = true;
    }

    Con_Printf("  Anims: idle=%d-%d, walk=%d-%d, run=%d-%d\n",
        mod->anims[ANIM_IDLE].start, mod->anims[ANIM_IDLE].end,
        mod->anims[ANIM_WALK].start, mod->anims[ANIM_WALK].end,
        mod->anims[ANIM_RUN].start, mod->anims[ANIM_RUN].end);
}

void Mod_FreeAliasModel(aliasmodel_t* mod)
{
    int i;

    if (!mod) return;

    if (mod->skin_textures)
    {
        for (i = 0; i < mod->numskins; i++)
            if (mod->skin_textures[i])
                glDeleteTextures(1, &mod->skin_textures[i]);
        Mem_Free(mod->skin_textures);
    }

    if (mod->texcoords) Mem_Free(mod->texcoords);
    if (mod->triangles) Mem_Free(mod->triangles);

    if (mod->frames)
    {
        for (i = 0; i < mod->numframes; i++)
            if (mod->frames[i]) Mem_Free(mod->frames[i]);
        Mem_Free(mod->frames);
    }

    if (mod->frame_names)
    {
        for (i = 0; i < mod->numframes; i++)
            if (mod->frame_names[i]) Mem_Free(mod->frame_names[i]);
        Mem_Free(mod->frame_names);
    }

    Mem_Free(mod);
}
