// engine/client/cl_input.c
#include "quakedef.h"
#include "net.h"
#include "client.h"
#include "in.h"
#include <keys.h>

void CL_SendCmd(void)
{
    sizebuf_t buf;
    byte data[128];
    usercmd_t* cmd = &cl.cmd;   // важно

    if (cls.state < ca_connected)
        return;

    SZ_Init(&buf, data, sizeof(data));

    if (key_dest != key_game)
        return;

    // углы камеры -> в команду (пока без мыши можно так)
    VectorCopy(cl.viewangles, cmd->viewangles);

    MSG_WriteByte(&buf, clc_move);
    MSG_WriteByte(&buf, cmd->msec);

    MSG_WriteAngle(&buf, cmd->viewangles[0]);
    MSG_WriteAngle(&buf, cmd->viewangles[1]);
    MSG_WriteAngle(&buf, cmd->viewangles[2]);

    MSG_WriteShort(&buf, (int)cmd->forwardmove);
    MSG_WriteShort(&buf, (int)cmd->sidemove);
    MSG_WriteShort(&buf, (int)cmd->upmove);

    MSG_WriteShort(&buf, cmd->buttons);
    MSG_WriteByte(&buf, cmd->impulse);

    Netchan_Transmit(&cls.netchan, buf.cursize, buf.data);
}
