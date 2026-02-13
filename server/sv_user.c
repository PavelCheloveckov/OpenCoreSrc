// server/sv_user.c
#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "mathlib.h"
#include "server.h"
#include "progs.h"

// Текущий обрабатываемый клиент
client_t* sv_client;
edict_t* sv_player;

extern cvar_t* sv_gravity;
extern cvar_t* sv_friction;
extern cvar_t* sv_stopspeed;
extern cvar_t* sv_maxspeed;
extern cvar_t* sv_accelerate;
extern cvar_t* sv_stepsize;

// ============================================================================
// Выполнение команды игрока
// ============================================================================

void SV_RunCmd(usercmd_t* ucmd, unsigned int random_seed)
{
    edict_t* ent = sv_player;
    vec3_t wishvel, wishdir;
    float wishspeed;
    vec3_t forward, right, up;
    float fmove, smove;
    int i;

    // Сохранение углов обзора
    VectorCopy(ucmd->viewangles, ent->v.v_angle);

    ent->v.angles[0] = 0;
    ent->v.angles[1] = ucmd->viewangles[1];
    ent->v.angles[2] = 0;

    // Проверка noclip
    if (ent->v.movetype == MOVETYPE_NOCLIP)
    {
        VectorCopy(ucmd->viewangles, ent->v.angles);
        ent->v.angles[0] *= -0.333f;
    }

    // Базовые кнопки
    ent->v.button = ucmd->buttons;
    ent->v.impulse = ucmd->impulse;

    // Направления движения
    AngleVectors(ucmd->viewangles, forward, right, up);

    fmove = ucmd->forwardmove;
    smove = ucmd->sidemove;

    // Максимальная скорость
    if (fmove > sv_maxspeed->value)
        fmove = sv_maxspeed->value;
    else if (fmove < -sv_maxspeed->value)
        fmove = -sv_maxspeed->value;

    if (smove > sv_maxspeed->value)
        smove = sv_maxspeed->value;
    else if (smove < -sv_maxspeed->value)
        smove = -sv_maxspeed->value;

    // Направление движения
    for (i = 0; i < 3; i++)
        wishvel[i] = forward[i] * fmove + right[i] * smove;

    wishvel[2] += ucmd->upmove;

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    if (wishspeed > sv_maxspeed->value)
    {
        VectorScale(wishvel, sv_maxspeed->value / wishspeed, wishvel);
        wishspeed = sv_maxspeed->value;
    }

    // Обработка состояний
    if (ent->v.movetype == MOVETYPE_NOCLIP)
    {
        // Noclip - просто установка скорости
        VectorCopy(wishvel, ent->v.velocity);
    }
    else if (ent->v.flags & FL_ONGROUND)
    {
        // На земле - применение ускорения и трения
        SV_UserFriction(ent);
        SV_Accelerate(ent, wishdir, wishspeed, sv_accelerate->value);
    }
    else
    {
        // В воздухе - воздушное ускорение
        SV_AirAccelerate(ent, wishdir, wishspeed, sv_accelerate->value);
    }

    // Прыжок
    if (ucmd->buttons & IN_JUMP)
    {
        if (ent->v.flags & FL_ONGROUND)
        {
            ent->v.velocity[2] = 270;  // Скорость прыжка
            ent->v.flags &= ~FL_ONGROUND;
        }
    }

    // Присед
    if (ucmd->buttons & IN_DUCK)
    {
        ent->v.flags |= FL_DUCKING;
        ent->v.view_ofs[2] = 12;  // Опускаем камеру
    }
    else
    {
        ent->v.flags &= ~FL_DUCKING;
        ent->v.view_ofs[2] = 28;  // Стандартная высота
    }

    // Физика движения применяется автоматически в SV_Physics()
}

// ============================================================================
// Трение на земле
// ============================================================================

void SV_UserFriction(edict_t* ent)
{
    float* vel;
    float speed, newspeed, control;
    float friction;
    float drop;

    vel = ent->v.velocity;

    speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
    if (speed < 1)
    {
        vel[0] = 0;
        vel[1] = 0;
        return;
    }

    friction = sv_friction->value;

    // Если на краю
    if (ent->v.friction != 1.0f)
        friction *= ent->v.friction;

    control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
    drop = control * friction * sv.frametime;

    newspeed = speed - drop;
    if (newspeed < 0)
        newspeed = 0;
    newspeed /= speed;

    vel[0] *= newspeed;
    vel[1] *= newspeed;
}

// ============================================================================
// Ускорение
// ============================================================================

static void SV_Accelerate(edict_t* ent, vec3_t wishdir, float wishspeed, float accel)
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

static void SV_CheckGroundSimple(edict_t* ent)
{
    vec3_t end;
    trace_t tr;

    VectorCopy(ent->v.origin, end);
    end[2] -= 2.0f;

    tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

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

void SV_AirAccelerate(edict_t* ent, vec3_t wishdir, float wishspeed, float accel)
{
    float addspeed, accelspeed, currentspeed, wishspd;

    wishspd = wishspeed;
    if (wishspd > 30)
        wishspd = 30;

    currentspeed = DotProduct(ent->v.velocity, wishdir);
    addspeed = wishspd - currentspeed;

    if (addspeed <= 0)
        return;

    accelspeed = accel * wishspeed * sv.frametime;
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    ent->v.velocity[0] += accelspeed * wishdir[0];
    ent->v.velocity[1] += accelspeed * wishdir[1];
}
