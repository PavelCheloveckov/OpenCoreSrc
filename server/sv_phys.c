// server/sv_phys.c
#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "mathlib.h"
#include "server.h"
#include "progs.h"
#include <bsp.h>
#include <client.h>

#define STOP_EPSILON    0.1f
#define MAX_CLIP_PLANES 5

// Используем указатели из sv_main.c
extern cvar_t* sv_gravity;
extern cvar_t* sv_friction;
extern cvar_t* sv_stopspeed;
extern cvar_t* sv_maxspeed;
extern cvar_t* sv_accelerate;
extern cvar_t* sv_stepsize;
extern model_t* cl_worldmodel;

// ============================================================================
// Утилиты физики
// ============================================================================

static void SV_CheckVelocity(edict_t* ent)
{
    int i;
    float wishspeed;

    for (i = 0; i < 3; i++)
    {
        if (IS_NAN(ent->v.velocity[i]))
        {
            Con_Printf("Got NaN velocity on %s\n", STRING(ent->v.classname));
            ent->v.velocity[i] = 0;
        }
        if (IS_NAN(ent->v.origin[i]))
        {
            Con_Printf("Got NaN origin on %s\n", STRING(ent->v.classname));
            ent->v.origin[i] = 0;
        }
    }

    // Ограничение скорости
    wishspeed = VectorLength(ent->v.velocity);
    if (wishspeed > 2000)
    {
        VectorScale(ent->v.velocity, 2000.0f / wishspeed, ent->v.velocity);
    }
}

static void SV_CheckGround(edict_t* ent)
{
    vec3_t end;
    trace_t tr;

    if (ent->v.velocity[2] > 0)
    {
        ent->v.flags &= ~FL_ONGROUND;
        ent->v.groundentity = NULL;
        return;
    }

    VectorCopy(ent->v.origin, end);
    end[2] -= 2.0f;

    tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

    static int cnt = 0;
    if (++cnt % 100 == 0)
    {
        Con_Printf("GROUND: frac=%.3f allsolid=%d nz=%.2f startsolid=%d\n",
            tr.fraction, tr.allsolid, tr.plane.normal[2], tr.startsolid);
    }

    if (tr.fraction < 1.0f && !tr.startsolid && tr.plane.normal[2] > 0.7f)
    {
        ent->v.flags |= FL_ONGROUND;
        ent->v.groundentity = tr.ent;
        if (ent->v.velocity[2] < 0)
            ent->v.velocity[2] = 0;
    }
    else
    {
        ent->v.flags &= ~FL_ONGROUND;
        ent->v.groundentity = NULL;
    }
}

static qboolean SV_RunThink(edict_t* ent)
{
    float thinktime;

    thinktime = ent->v.nextthink;
    if (thinktime <= 0 || thinktime > sv.time)
        return TRUE;

    if (thinktime < sv.time)
        thinktime = sv.time;

    ent->v.nextthink = 0;

    // Вызов think функции через GameDLL
    if (gEntityInterface.pfnThink)
        gEntityInterface.pfnThink(ent);

    return !ent->free;
}

static void SV_Impact(edict_t* e1, edict_t* e2)
{
    // Вызов touch функций
    if (e1->v.solid != SOLID_NOT && gEntityInterface.pfnTouch)
    {
        gEntityInterface.pfnTouch(e1, e2);
    }

    if (e2->v.solid != SOLID_NOT && gEntityInterface.pfnTouch)
    {
        gEntityInterface.pfnTouch(e2, e1);
    }
}

// ============================================================================
// Clip Velocity - отражение скорости от плоскости
// ============================================================================

static int SV_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
    float backoff;
    float change;
    int i, blocked;

    blocked = 0;
    if (normal[2] > 0)
        blocked |= 1;   // floor
    if (!normal[2])
        blocked |= 2;   // step/wall

    backoff = DotProduct(in, normal) * overbounce;

    for (i = 0; i < 3; i++)
    {
        change = normal[i] * backoff;
        out[i] = in[i] - change;

        if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
            out[i] = 0;
    }

    return blocked;
}

// ============================================================================
// FlyMove - основное движение с коллизиями
// ============================================================================

static int SV_FlyMove(edict_t* ent, float time, trace_t* steptrace)
{
    int bumpcount, numbumps;
    vec3_t dir;
    float d;
    int numplanes;
    vec3_t planes[MAX_CLIP_PLANES];
    vec3_t primal_velocity, original_velocity, new_velocity;
    int i, j;
    trace_t trace;
    vec3_t end;
    float time_left;
    int blocked;

    numbumps = 4;
    blocked = 0;

    VectorCopy(ent->v.velocity, original_velocity);
    VectorCopy(ent->v.velocity, primal_velocity);
    numplanes = 0;

    time_left = time;

    for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
    {
        if (!ent->v.velocity[0] && !ent->v.velocity[1] && !ent->v.velocity[2])
            break;

        VectorMA(ent->v.origin, time_left, ent->v.velocity, end);

        trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end,
            MOVE_NORMAL, ent);

        if (trace.allsolid)
        {
            VectorClear(ent->v.velocity);
            return 3;
        }

        if (trace.fraction > 0)
        {
            VectorCopy(trace.endpos, ent->v.origin);
            VectorCopy(ent->v.velocity, original_velocity);
            numplanes = 0;
        }

        if (trace.fraction == 1)
            break;

        if (!trace.ent)
            Sys_Error("SV_FlyMove: !trace.ent");

        if (trace.plane.normal[2] > 0.7f)
        {
            blocked |= 1;   // floor
            if (trace.ent->v.solid == SOLID_BSP)
            {
                ent->v.flags |= FL_ONGROUND;
                ent->v.groundentity = trace.ent;
            }
        }

        if (!trace.plane.normal[2])
        {
            blocked |= 2;   // step
            if (steptrace)
                *steptrace = trace;
        }

        // Столкновение
        SV_Impact(ent, trace.ent);
        if (ent->free)
            break;

        time_left -= time_left * trace.fraction;

        if (numplanes >= MAX_CLIP_PLANES)
        {
            VectorClear(ent->v.velocity);
            return 3;
        }

        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;

        // Модификация original_velocity для обхода всех плоскостей
        for (i = 0; i < numplanes; i++)
        {
            SV_ClipVelocity(original_velocity, planes[i], new_velocity, 1);

            for (j = 0; j < numplanes; j++)
            {
                if (j != i)
                {
                    if (DotProduct(new_velocity, planes[j]) < 0)
                        break;
                }
            }
            if (j == numplanes)
                break;
        }

        if (i != numplanes)
        {
            VectorCopy(new_velocity, ent->v.velocity);
        }
        else
        {
            if (numplanes != 2)
            {
                VectorClear(ent->v.velocity);
                return 7;
            }
            CrossProduct(planes[0], planes[1], dir);
            d = DotProduct(dir, ent->v.velocity);
            VectorScale(dir, d, ent->v.velocity);
        }

        if (DotProduct(ent->v.velocity, primal_velocity) <= 0)
        {
            VectorClear(ent->v.velocity);
            return blocked;
        }
    }

    return blocked;
}

// ============================================================================
// WalkMove - движение с учётом ступенек
// ============================================================================

static void SV_WalkMove_Internal(edict_t* ent)
{
    vec3_t upmove, downmove;
    vec3_t oldorg, oldvel;
    vec3_t nosteporg, nostepvel;
    int clip;
    int oldonground;
    trace_t steptrace, downtrace;

    // инициализация oldonground
    oldonground = (ent->v.flags & FL_ONGROUND) ? 1 : 0;

    VectorCopy(ent->v.origin, oldorg);
    VectorCopy(ent->v.velocity, oldvel);

    clip = SV_FlyMove(ent, sv.frametime, &steptrace);

    if (!(clip & 2))
        return;     // Не заблокирован стеной

    if (!oldonground)  // Если не был на земле
        return;

    if (ent->v.movetype != MOVETYPE_WALK)
        return;

    // Попытка подняться на ступеньку
    VectorCopy(ent->v.origin, nosteporg);
    VectorCopy(ent->v.velocity, nostepvel);

    VectorCopy(oldorg, ent->v.origin);

    VectorCopy(vec3_origin, upmove);
    VectorCopy(vec3_origin, downmove);
    upmove[2] = sv_stepsize->value;
    downmove[2] = -sv_stepsize->value + oldvel[2] * sv.frametime;

    // Движение вверх
    SV_PushEntity(ent, upmove);

    // Движение вперёд
    VectorCopy(oldvel, ent->v.velocity);
    clip = SV_FlyMove(ent, sv.frametime, &steptrace);

    // Движение вниз
    downtrace = SV_PushEntity(ent, downmove);

    if (downtrace.plane.normal[2] > 0.7f)
    {
        if (ent->v.solid == SOLID_BSP)
        {
            ent->v.flags |= FL_ONGROUND;
            ent->v.groundentity = downtrace.ent;
        }
    }
    else
    {
        // Неудачный подъём - возврат
        VectorCopy(nosteporg, ent->v.origin);
        VectorCopy(nostepvel, ent->v.velocity);
    }
}

// ============================================================================
// Физика для разных типов movetype
// ============================================================================

static void SV_Physics_None(edict_t* ent)
{
    SV_RunThink(ent);
}

static void SV_Physics_Noclip(edict_t* ent)
{
    if (!SV_RunThink(ent))
        return;

    // Для игрока - читаем команды
    int entnum = NUM_FOR_EDICT(ent);
    if (entnum > 0 && entnum <= svs.maxclients)
    {
        usercmd_t* cmd = &svs.clients[entnum - 1].lastcmd;

        VectorCopy(cmd->viewangles, ent->v.angles);

        vec3_t fwd, right, up;
        AngleVectors(ent->v.angles, fwd, right, up);

        float speed = 500.0f;  // скорость полёта

        ent->v.velocity[0] = fwd[0] * cmd->forwardmove + right[0] * cmd->sidemove;
        ent->v.velocity[1] = fwd[1] * cmd->forwardmove + right[1] * cmd->sidemove;
        ent->v.velocity[2] = fwd[2] * cmd->forwardmove + right[2] * cmd->sidemove;

        // Вверх/вниз
        if (cmd->upmove > 0)
            ent->v.velocity[2] += speed;
        else if (cmd->upmove < 0)
            ent->v.velocity[2] -= speed;
    }

    // Двигаем без коллизий
    VectorMA(ent->v.origin, sv.frametime, ent->v.velocity, ent->v.origin);

    SV_LinkEdict(ent, FALSE);
}

static void SV_Physics_Toss(edict_t* ent)
{
    trace_t trace;
    vec3_t move;
    float backoff;

    if (!SV_RunThink(ent))
        return;

    // Применение гравитации
    if (ent->v.movetype != MOVETYPE_FLY &&
        ent->v.movetype != MOVETYPE_FLYMISSILE)
    {
        ent->v.velocity[2] -= sv_gravity->value * ent->v.gravity * sv.frametime;
    }

    // Угловая скорость
    VectorMA(ent->v.angles, sv.frametime, ent->v.avelocity, ent->v.angles);

    // Движение
    VectorScale(ent->v.velocity, sv.frametime, move);
    trace = SV_PushEntity(ent, move);

    if (ent->free)
        return;

    if (trace.fraction < 1)
    {
        if (ent->v.movetype == MOVETYPE_BOUNCE)
            backoff = 1.5f;
        else if (ent->v.movetype == MOVETYPE_BOUNCEMISSILE)
            backoff = 2.0f;
        else
            backoff = 1.0f;

        SV_ClipVelocity(ent->v.velocity, trace.plane.normal,
            ent->v.velocity, backoff);

        // Остановка на земле
        if (trace.plane.normal[2] > 0.7f)
        {
            if (ent->v.velocity[2] < 60 ||
                ent->v.movetype != MOVETYPE_BOUNCE)
            {
                ent->v.flags |= FL_ONGROUND;
                ent->v.groundentity = trace.ent;
                VectorClear(ent->v.velocity);
                VectorClear(ent->v.avelocity);
            }
        }
    }

    // Проверка воды
    SV_CheckWaterTransition(ent);
}

static void SV_Physics_Step(edict_t* ent)
{
    qboolean wasonground;
    float* vel;
    float speed, newspeed, control;
    float friction;

    if (!SV_RunThink(ent))
        return;

    SV_CheckGround(ent);
    wasonground = (ent->v.flags & FL_ONGROUND) ? TRUE : FALSE;

    vel = ent->v.velocity;

    // трение на земле
    if (wasonground)
    {
        speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
        if (speed)
        {
            friction = sv_friction->value;
            control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
            newspeed = speed - sv.frametime * control * friction;

            if (newspeed < 0) newspeed = 0;
            newspeed /= speed;

            vel[0] *= newspeed;
            vel[1] *= newspeed;
        }
        vel[2] = 0; // на земле z-скорость должна быть 0
    }
    else
    {
        float grav = (ent->v.gravity != 0.0f) ? ent->v.gravity : 1.0f;
        vel[2] -= sv_gravity->value * grav * sv.frametime;
    }

    SV_WalkMove_Internal(ent);

    SV_CheckGround(ent);

    SV_LinkEdict(ent, TRUE);
    SV_CheckWaterTransition(ent);
}

static void SV_AccelerateLocal(edict_t* ent, vec3_t wishdir, float wishspeed, float accel)
{
    float currentspeed = DotProduct(ent->v.velocity, wishdir);
    float addspeed = wishspeed - currentspeed;
    if (addspeed <= 0)
        return;

    float accelspeed = accel * sv.frametime * wishspeed;
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    ent->v.velocity[0] += accelspeed * wishdir[0];
    ent->v.velocity[1] += accelspeed * wishdir[1];
    ent->v.velocity[2] += accelspeed * wishdir[2];
}

static void SV_CheckGroundSimpleLocal(edict_t* ent)
{
    vec3_t end;
    trace_t tr;

    VectorCopy(ent->v.origin, end);
    end[2] -= 2.0f;

    tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

    static int dbg;
    dbg++;
    if ((dbg % 100) == 0)
        Con_Printf("SV_Move fraction=%f allsolid=%d\n", tr.fraction, tr.allsolid);

    if (tr.fraction < 1.0f && tr.plane.normal[2] > 0.7f)
    {
        ent->v.flags |= FL_ONGROUND;
        ent->v.groundentity = tr.ent;
    }
    else
    {
        ent->v.flags &= ~FL_ONGROUND;
        ent->v.groundentity = NULL;
    }
}

static void SV_Physics_Pusher(edict_t* ent)
{
    float thinktime;
    float oldtime;
    float movetime;
    vec3_t oldorg, move;

    oldtime = ent->v.ltime;

    thinktime = ent->v.nextthink;
    if (thinktime < ent->v.ltime + sv.frametime)
    {
        movetime = thinktime - ent->v.ltime;
        if (movetime < 0)
            movetime = 0;
    }
    else
    {
        movetime = sv.frametime;
    }

    if (movetime)
    {
        VectorCopy(ent->v.origin, oldorg);
        VectorScale(ent->v.velocity, movetime, move);
        SV_PushMove(ent, move, movetime);
    }

    if (thinktime > oldtime && thinktime <= ent->v.ltime)
    {
        ent->v.nextthink = 0;
        if (gEntityInterface.pfnThink)
            gEntityInterface.pfnThink(ent);
    }
}

// Трассировка вниз
float SV_TraceDown(vec3_t start, float maxdist)
{
    vec3_t point;
    float dist;
    float step = 1.0f;  // шаг проверки

    VectorCopy(start, point);

    for (dist = 0; dist < maxdist; dist += step)
    {
        point[2] = start[2] - dist;

        if (SV_PointContents(point) == CONTENTS_SOLID)
        {
            // Нашли твёрдую поверхность
            return dist - step;  // возвращаем расстояние до неё
        }
    }

    return maxdist;  // пол не найден
}

static void SV_PlayerPhysics(edict_t* ent)
{
    float* vel;
    float speed, newspeed, control, friction;

    if (!SV_RunThink(ent))
        return;

    vel = ent->v.velocity;

    // Гравитация - ВСЕГДА если не на земле
    if (!(ent->v.flags & FL_ONGROUND))
    {
        float grav = (ent->v.gravity != 0.0f) ? ent->v.gravity : 1.0f;
        vel[2] -= sv_gravity->value * grav * sv.frametime;
    }
    else
    {
        // На земле - трение
        speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
        if (speed > 0)
        {
            friction = sv_friction->value;
            control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
            newspeed = speed - sv.frametime * control * friction;
            if (newspeed < 0) newspeed = 0;
            newspeed /= speed;
            vel[0] *= newspeed;
            vel[1] *= newspeed;
        }
    }

    // Движение
    SV_WalkMove_Internal(ent);

    // Проверка земли ПОСЛЕ движения
    SV_CheckGround(ent);

    SV_LinkEdict(ent, TRUE);
    SV_CheckWaterTransition(ent);
}

// ============================================================================
// Главный цикл физики
// ============================================================================

void SV_Physics(void)
{
    int i;
    edict_t* ent;

    // Обработка каждой сущности
    for (i = 0; i < sv.num_edicts; i++)
    {
        ent = EDICT_NUM(i);

        if (ent->free)
            continue;

        // Глобальная скорость влияет на физику
        if (ent->v.basevelocity[0] || ent->v.basevelocity[1] || ent->v.basevelocity[2])
        {
            VectorAdd(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);
        }

        // Выбор физики по movetype
        switch ((int)ent->v.movetype)
        {
        case MOVETYPE_PUSH:
            SV_Physics_Pusher(ent);
            break;
        case MOVETYPE_NONE:
            SV_Physics_None(ent);
            break;
        case MOVETYPE_NOCLIP:
            SV_Physics_Noclip(ent);
            break;
        case MOVETYPE_STEP:
            SV_Physics_Step(ent);
            break;
        case MOVETYPE_WALK:
            if (i > 0 && i <= svs.maxclients)
            {
                usercmd_t* cmd = &svs.clients[i - 1].lastcmd;

                VectorCopy(cmd->viewangles, ent->v.angles);

                vec3_t fwd, right, up;
                AngleVectors(ent->v.angles, fwd, right, up);

                // Горизонтальное движение
                ent->v.velocity[0] = fwd[0] * cmd->forwardmove + right[0] * cmd->sidemove;
                ent->v.velocity[1] = fwd[1] * cmd->forwardmove + right[1] * cmd->sidemove;

                // Прыжок - только если на земле и ещё не прыгали
                if ((ent->v.flags & FL_ONGROUND) && cmd->upmove > 0
                    && !(ent->v.flags & FL_JUMPED))
                {
                    ent->v.velocity[2] = 270.0f;
                    ent->v.flags &= ~FL_ONGROUND;
                    ent->v.flags |= FL_JUMPED;  // помечаем что прыгнули
                }

                // Сброс флага прыжка когда кнопка отпущена
                if (cmd->upmove <= 0)
                {
                    ent->v.flags &= ~FL_JUMPED;
                }

                SV_PlayerPhysics(ent);
            }
            else
            {
                SV_Physics_Step(ent);
            }
            break;
        case MOVETYPE_TOSS:
        case MOVETYPE_BOUNCE:
        case MOVETYPE_BOUNCEMISSILE:
        case MOVETYPE_FLY:
        case MOVETYPE_FLYMISSILE:
            SV_Physics_Toss(ent);
            break;
        default:
            Sys_Error("SV_Physics: bad movetype %d", (int)ent->v.movetype);
        }

        static float next = 0;
        if (i == 1 && sv.time >= next)
        {
            Con_Printf("pl z=%.1f onground=%d velz=%.1f grav=%.1f mins=(%.0f %.0f %.0f) maxs=(%.0f %.0f %.0f)\n",
                ent->v.origin[2],
                (ent->v.flags & FL_ONGROUND) ? 1 : 0,
                ent->v.velocity[2],
                ent->v.gravity,
                ent->v.mins[0], ent->v.mins[1], ent->v.mins[2],
                ent->v.maxs[0], ent->v.maxs[1], ent->v.maxs[2]);
            next = sv.time + 1.0f;
        }

        // Убираем basevelocity
        VectorClear(ent->v.basevelocity);

        SV_CheckVelocity(ent);
    }
}
