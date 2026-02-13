// engine/input/in_move.c
#include "quakedef.h"
#include "in.h"
#include "keys.h"
#include "client.h"
#include <cvar.h>

extern float mouse_dx_accum;
extern float mouse_dy_accum;
extern qboolean mouse_grabbed;
extern cvar_t* sensitivity;

void IN_Move(float frametime, usercmd_t* cmd)
{
    float speed = 300.0f;

    cmd->forwardmove = 0;
    cmd->sidemove = 0;
    cmd->upmove = 0;

    if (Key_IsDown('w'))     cmd->forwardmove += speed;
    if (Key_IsDown('s'))     cmd->forwardmove -= speed;
    if (Key_IsDown('d'))     cmd->sidemove += speed;
    if (Key_IsDown('a'))     cmd->sidemove -= speed;
    if (Key_IsDown(K_SPACE)) cmd->upmove += speed;
    if (Key_IsDown(K_CTRL))  cmd->upmove -= speed;

    cmd->msec = (byte)(frametime * 1000.0f);
    if (cmd->msec > 250) cmd->msec = 250;
}
