// engine/input/in_main.c
#include "quakedef.h"
#include "client.h"
#include "keys.h"
#include "cvar.h"
#include "cmd.h"
#include "console.h"
#include "mathlib.h"
#include "sv_trigger.h"
#include "in.h"

#ifdef _WIN32
#include <windows.h>
#include "gl_local.h"
#endif
#include <server.h>

// Состояние мыши
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static int mouse_dx, mouse_dy;
static qboolean mouse_active;

extern HWND hwnd_main;
extern qboolean mouse_grabbed;

extern cvar_t* sensitivity;
extern cvar_t* m_pitch;
extern cvar_t* m_yaw;

extern server_t sv;
extern server_static_t svs;

// ============================================================================
// Кнопки (+forward, +back, etc.)
// ============================================================================

typedef struct kbutton_s {
    int down[2];
    int state;
} kbutton_t;

static kbutton_t in_forward;
static kbutton_t in_back;
static kbutton_t in_moveleft;
static kbutton_t in_moveright;
static kbutton_t in_left;
static kbutton_t in_right;
static kbutton_t in_lookup;
static kbutton_t in_lookdown;
static kbutton_t in_jump;
static kbutton_t in_duck;
static kbutton_t in_attack;
static kbutton_t in_attack2;
static kbutton_t in_use;
static kbutton_t in_reload;

static void KeyDown(kbutton_t* b)
{
    int k;
    const char* c;

    c = Cmd_Argv(1);
    if (c[0])
        k = atoi(c);
    else
        k = -1;

    if (k == b->down[0] || k == b->down[1])
        return;

    if (!b->down[0])
        b->down[0] = k;
    else if (!b->down[1])
        b->down[1] = k;
    else
        return;

    b->state |= 1;
}

static void KeyUp(kbutton_t* b)
{
    int k;
    const char* c;

    c = Cmd_Argv(1);
    if (c[0])
        k = atoi(c);
    else
    {
        b->down[0] = b->down[1] = 0;
        b->state = 0;
        return;
    }

    if (b->down[0] == k)
        b->down[0] = 0;
    else if (b->down[1] == k)
        b->down[1] = 0;
    else
        return;

    if (b->down[0] || b->down[1])
        return;

    b->state &= ~1;
}

static float KeyState(kbutton_t* key)
{
    return (key->state & 1) ? 1.0f : 0.0f;
}

// Команды кнопок
static void IN_ForwardDown(void) { KeyDown(&in_forward); }
static void IN_ForwardUp(void) { KeyUp(&in_forward); }
static void IN_BackDown(void) { KeyDown(&in_back); }
static void IN_BackUp(void) { KeyUp(&in_back); }
static void IN_MoveLeftDown(void) { KeyDown(&in_moveleft); }
static void IN_MoveLeftUp(void) { KeyUp(&in_moveleft); }
static void IN_MoveRightDown(void) { KeyDown(&in_moveright); }
static void IN_MoveRightUp(void) { KeyUp(&in_moveright); }
static void IN_LeftDown(void) { KeyDown(&in_left); }
static void IN_LeftUp(void) { KeyUp(&in_left); }
static void IN_RightDown(void) { KeyDown(&in_right); }
static void IN_RightUp(void) { KeyUp(&in_right); }
static void IN_LookupDown(void) { KeyDown(&in_lookup); }
static void IN_LookupUp(void) { KeyUp(&in_lookup); }
static void IN_LookdownDown(void) { KeyDown(&in_lookdown); }
static void IN_LookdownUp(void) { KeyUp(&in_lookdown); }
static void IN_JumpDown(void) { KeyDown(&in_jump); }
static void IN_JumpUp(void) { KeyUp(&in_jump); }
static void IN_DuckDown(void) { KeyDown(&in_duck); }
static void IN_DuckUp(void) { KeyUp(&in_duck); }
static void IN_AttackDown(void) { KeyDown(&in_attack); }
static void IN_AttackUp(void) { KeyUp(&in_attack); }
static void IN_Attack2Down(void) { KeyDown(&in_attack2); }
static void IN_Attack2Up(void) { KeyUp(&in_attack2); }
static void IN_UseDown(void) { KeyDown(&in_use); }
static void IN_UseUp(void) { KeyUp(&in_use); }
static void IN_ReloadDown(void) { KeyDown(&in_reload); }
static void IN_ReloadUp(void) { KeyUp(&in_reload); }

// ============================================================================
// Инициализация
// ============================================================================

void IN_Init(void)
{
    Con_Printf("IN_Init\n");

    Cmd_AddCommand("+forward", IN_ForwardDown);
    Cmd_AddCommand("-forward", IN_ForwardUp);
    Cmd_AddCommand("+back", IN_BackDown);
    Cmd_AddCommand("-back", IN_BackUp);
    Cmd_AddCommand("+moveleft", IN_MoveLeftDown);
    Cmd_AddCommand("-moveleft", IN_MoveLeftUp);
    Cmd_AddCommand("+moveright", IN_MoveRightDown);
    Cmd_AddCommand("-moveright", IN_MoveRightUp);
    Cmd_AddCommand("+left", IN_LeftDown);
    Cmd_AddCommand("-left", IN_LeftUp);
    Cmd_AddCommand("+right", IN_RightDown);
    Cmd_AddCommand("-right", IN_RightUp);
    Cmd_AddCommand("+lookup", IN_LookupDown);
    Cmd_AddCommand("-lookup", IN_LookupUp);
    Cmd_AddCommand("+lookdown", IN_LookdownDown);
    Cmd_AddCommand("-lookdown", IN_LookdownUp);
    Cmd_AddCommand("+jump", IN_JumpDown);
    Cmd_AddCommand("-jump", IN_JumpUp);
    Cmd_AddCommand("+duck", IN_DuckDown);
    Cmd_AddCommand("-duck", IN_DuckUp);
    Cmd_AddCommand("+attack", IN_AttackDown);
    Cmd_AddCommand("-attack", IN_AttackUp);
    Cmd_AddCommand("+attack2", IN_Attack2Down);
    Cmd_AddCommand("-attack2", IN_Attack2Up);
    Cmd_AddCommand("+use", IN_UseDown);
    Cmd_AddCommand("-use", IN_UseUp);
    Cmd_AddCommand("+reload", IN_ReloadDown);
    Cmd_AddCommand("-reload", IN_ReloadUp);

    mouse_active = TRUE;
}

void IN_Shutdown(void)
{
    Con_Printf("IN_Shutdown\n");
    mouse_active = FALSE;
}

// ============================================================================
// Мышь
// ============================================================================

void IN_MouseMove(usercmd_t* cmd)
{
    // Пока отключено
}

void IN_Accumulate(void)
{
    // Пока отключено
}

void IN_Frame(void)
{
    IN_Accumulate();
}

void IN_ClearStates(void)
{
    memset(&in_forward, 0, sizeof(in_forward));
    memset(&in_back, 0, sizeof(in_back));
    memset(&in_moveleft, 0, sizeof(in_moveleft));
    memset(&in_moveright, 0, sizeof(in_moveright));
    memset(&in_jump, 0, sizeof(in_jump));
    memset(&in_duck, 0, sizeof(in_duck));
    memset(&in_attack, 0, sizeof(in_attack));
    memset(&in_attack2, 0, sizeof(in_attack2));
    memset(&in_use, 0, sizeof(in_use));
    memset(&in_reload, 0, sizeof(in_reload));

    mouse_dx = 0;
    mouse_dy = 0;
}

// ============================================================================
// Создание команды движения
// ============================================================================

void CL_CreateCmd(void)
{
    usercmd_t newcmd;
    memset(&newcmd, 0, sizeof(newcmd));

    float turn_speed = 120.0f * cls.frametime;

    if (Key_IsDown(K_LEFTARROW))  cl.viewangles[1] += turn_speed;
    if (Key_IsDown(K_RIGHTARROW)) cl.viewangles[1] -= turn_speed;
    if (Key_IsDown(K_PGUP))       cl.viewangles[0] -= turn_speed;
    if (Key_IsDown(K_PGDN))       cl.viewangles[0] += turn_speed;

    if (cl.viewangles[0] > 89.0f)  cl.viewangles[0] = 89.0f;
    if (cl.viewangles[0] < -89.0f) cl.viewangles[0] = -89.0f;

    if (key_dest == key_game)
        IN_Move(cls.frametime, &newcmd);

    VectorCopy(cl.viewangles, newcmd.viewangles);

    cl.cmd = newcmd;

    if (sv.active)
    {
        edict_t* player = EDICT_NUM(1);
        VectorCopy(cl.viewangles, player->v.angles);
    }
}
