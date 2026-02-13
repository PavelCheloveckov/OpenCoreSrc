// engine/client/cl_view.c
#include "quakedef.h"
#include "common.h"      // Q_strncpy, COM_LoadFile, etc.
#include "cmd.h"         // Cmd_AddCommand, Cmd_Argc, Cmd_Argv
#include "console.h"     // Con_Printf, Con_Draw
#include "mathlib.h"     // VectorCopy, etc.
#include "client.h"
#include "net.h"
#include <gl_local.h>
#include <server.h>

refdef_t r_refdef;
edict_t* EDICT_NUM(int num);

static void CL_UpdateViewFromPlayer(void)
{
    if (!sv.active) return;

    edict_t* player = EDICT_NUM(1);

    VectorCopy(player->v.origin, cl.vieworg);
    cl.vieworg[2] += player->v.view_ofs[2];
}

// ============================================================================
// Расчёт камеры
// ============================================================================

void V_CalcRefdef(void)
{
    CL_UpdateViewFromPlayer();

    memset(&r_refdef, 0, sizeof(r_refdef));

    // Нормальная камера от игрока
    r_refdef.vieworg[0] = cl.vieworg[0];
    r_refdef.vieworg[1] = cl.vieworg[1];
    r_refdef.vieworg[2] = cl.vieworg[2];

    r_refdef.viewangles[0] = cl.viewangles[0];
    r_refdef.viewangles[1] = cl.viewangles[1];
    r_refdef.viewangles[2] = cl.viewangles[2];
}

// ============================================================================
// Рендеринг вида
// ============================================================================

void V_RenderView(void)
{
    V_CalcRefdef();

    // ═══ ОТЛАДКА КАМЕРЫ ═══
    static int cam_printed = 0;
    if (!cam_printed)
    {
        Con_Printf("Camera: pos=(%.1f, %.1f, %.1f) ang=(%.1f, %.1f, %.1f)\n",
            r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2],
            r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]);
        cam_printed = 1;
    }

    r_refdef.x = 0;
    r_refdef.y = 0;
    r_refdef.width = glstate.width;
    r_refdef.height = glstate.height;
    r_refdef.fov_x = 90;
    r_refdef.fov_y = 74;
    r_refdef.time = cl.time;

    R_RenderView(r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2],
        r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]);
}
