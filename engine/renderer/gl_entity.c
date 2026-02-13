// gl_entity.c

#include "quakedef.h"
#include "gl_local.h"
#include "mdl.h"
#include <client.h>
#include <cvar.h>

render_entity_t r_entities[MAX_RENDER_ENTITIES];
int r_numentities = 0;
extern cvar_t* sv_gravity;
extern double cl_time;

float SV_TraceDown(vec3_t start, float maxdist);
int SV_PointContents(vec3_t p);

render_entity_t* R_AllocRenderEntity(void)
{
    if (r_numentities >= MAX_RENDER_ENTITIES)
        return NULL;

    render_entity_t* ent = &r_entities[r_numentities++];
    memset(ent, 0, sizeof(*ent));
    ent->active = true;

    return ent;
}

void R_ClearRenderEntities(void)
{
    r_numentities = 0;
}

void R_DrawEntities(void)
{
    int i;
    int drawn = 0;

    for (i = 0; i < r_numentities; i++)
    {
        render_entity_t* ent = &r_entities[i];

        if (!ent->active || !ent->model)
            continue;

        R_DrawAliasModel(ent->model, ent->origin, ent->angles,
            ent->frame, ent->skin);
        drawn++;
    }

    // Выводим раз в секунду
    static float lastprint = 0;
    if (cl.time - lastprint > 1.0f)
    {
        Con_Printf("Drew %d entities (total %d)\n", drawn, r_numentities);
        lastprint = (float)cl.time;
    }
}

void R_AnimateEntities(void)
{
    int i;

    for (i = 0; i < r_numentities; i++)
    {
        render_entity_t* ent = &r_entities[i];

        if (!ent->active || !ent->model)
            continue;

        // Статичные модели (1 кадр) - пропускаем
        if (ent->model->numframes <= 1)
            continue;

        // Получаем текущую анимацию
        anim_sequence_t* anim = &ent->model->anims[ent->current_anim];

        // Если анимация не найдена (start == end == 0), пробуем idle
        if (anim->start == 0 && anim->end == 0 && ent->current_anim != ANIM_IDLE)
        {
            ent->current_anim = ANIM_IDLE;
            anim = &ent->model->anims[ANIM_IDLE];
        }

        // Инициализация времени
        if (ent->anim_time == 0)
            ent->anim_time = (float)cl.time;

        // Вычисляем кадр
        float deltatime = (float)cl.time - ent->anim_time;
        int framecount = anim->end - anim->start + 1;

        if (framecount <= 0)
            framecount = 1;

        int localframe;
        if (anim->loop)
        {
            localframe = (int)(deltatime * anim->framerate) % framecount;
        }
        else
        {
            localframe = (int)(deltatime * anim->framerate);
            if (localframe >= framecount)
                localframe = framecount - 1;
        }

        ent->frame = anim->start + localframe;

        // Ограничиваем кадр
        if (ent->frame < 0)
            ent->frame = 0;
        if (ent->frame >= ent->model->numframes)
            ent->frame = ent->model->numframes - 1;
    }
}

void R_PhysicsEntities(float frametime)
{
    int i;
    float gravity = sv_gravity ? sv_gravity->value : 800.0f;

    if (frametime > 0.05f) frametime = 0.05f;

    for (i = 0; i < r_numentities; i++)
    {
        render_entity_t* ent = &r_entities[i];
        vec3_t p;
        float mins_z;

        if (!ent->active || !ent->model) continue;
        if (ent->gravity_scale <= 0) continue;

        mins_z = ent->model->mins[2];

        // точка под "ногами"
        p[0] = ent->origin[0];
        p[1] = ent->origin[1];
        p[2] = ent->origin[2] + mins_z - 1.0f;

        if (SV_PointContents(p) == CONTENTS_SOLID)
        {
            ent->onground = true;
            ent->velocity[2] = 0;
            continue;
        }

        ent->onground = false;

        ent->velocity[2] -= gravity * frametime;
        if (ent->velocity[2] < -1000.0f) ent->velocity[2] = -1000.0f;

        // сколько падаем за кадр
        float fall = -ent->velocity[2] * frametime;
        if (fall < 1.0f) fall = 1.0f;
        if (fall > 32.0f) fall = 32.0f;

        // идём вниз по 1 юниту и ищем пол по "ногам"
        float step;
        for (step = 1.0f; step <= fall; step += 1.0f)
        {
            p[2] = ent->origin[2] + mins_z - step;

            if (SV_PointContents(p) == CONTENTS_SOLID)
            {
                // ставим так, чтобы низ модели был на 1 юнит выше solid
                ent->origin[2] = (p[2] + 1.0f) - mins_z;
                ent->velocity[2] = 0;
                ent->onground = true;
                break;
            }
        }

        if (!ent->onground)
            ent->origin[2] -= fall;
    }
}
