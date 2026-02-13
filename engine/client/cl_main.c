// engine/client/cl_main.c
#include "quakedef.h"
#include "common.h"
#include "cmd.h"
#include "console.h"
#include "mathlib.h"
#include "client.h"
#include "net.h"
#include <cvar.h>
#include <keys.h>
#include <server.h>

client_state_t cl;
client_static_t cls;

extern server_t sv;
extern server_static_t svs;

// Cvars
cvar_t* cl_name;
cvar_t* cl_rate;
cvar_t* cl_updaterate;
cvar_t* cl_cmdrate;
cvar_t* sensitivity;
cvar_t* m_pitch;
cvar_t* m_yaw;
cvar_t* lookspring;
cvar_t* lookstrafe;

qboolean cl_skip_render = FALSE;

struct model_s* cl_worldmodel = NULL;
extern void Draw_Menu(void);
extern void Draw_Background(void);
extern float mouse_dx_accum;
extern float mouse_dy_accum;
extern qboolean mouse_grabbed;
extern cvar_t* sensitivity;

void CL_Viewpos_f(void);
void CL_ForceActive_f(void);
void SV_Frame(float frametime);
void CL_CreateCmd(void);

// ============================================================================
// Инициализация
// ============================================================================

void CL_Init(void)
{
    Con_Printf("CL_Init\n");

    memset(&cl, 0, sizeof(cl));
    memset(&cls, 0, sizeof(cls));

    cls.state = ca_disconnected;
    cls.initialized = TRUE;

    // Cvars
    cl_name = Cvar_Get("name", "Player", FCVAR_USERINFO | FCVAR_ARCHIVE);
    cl_rate = Cvar_Get("rate", "20000", FCVAR_USERINFO | FCVAR_ARCHIVE);
    cl_updaterate = Cvar_Get("cl_updaterate", "60", FCVAR_ARCHIVE);
    cl_cmdrate = Cvar_Get("cl_cmdrate", "60", FCVAR_ARCHIVE);
    sensitivity = Cvar_Get("sensitivity", "3", FCVAR_ARCHIVE);
    m_pitch = Cvar_Get("m_pitch", "0.022", FCVAR_ARCHIVE);
    m_yaw = Cvar_Get("m_yaw", "0.022", FCVAR_ARCHIVE);
    lookspring = Cvar_Get("lookspring", "0", FCVAR_ARCHIVE);
    lookstrafe = Cvar_Get("lookstrafe", "0", FCVAR_ARCHIVE);

    // Команды
    Cmd_AddCommand("connect", CL_Connect_f);
    Cmd_AddCommand("disconnect", CL_Disconnect_f);
    Cmd_AddCommand("reconnect", CL_Reconnect_f);
    Cmd_AddCommand("cmd", CL_ForwardToServer_f);
    Cmd_AddCommand("viewpos", CL_Viewpos_f);
    Cmd_AddCommand("forceactive", CL_ForceActive_f);

    // Ввод
    IN_Init();
    Key_Init();

    // углы, чтобы смотреть прямо
    cl.viewangles[0] = 0;
    cl.viewangles[1] = 0;
    cl.viewangles[2] = 0;

    VectorClear(cl.vieworg);
    VectorClear(cl.viewangles);
    VectorClear(cl.punchangle);
    VectorClear(cl.velocity);

    Con_Printf("Client initialized\n");
}

void CL_Shutdown(void)
{
    Con_Printf("CL_Shutdown\n");

    CL_Disconnect();

    S_Shutdown();
    R_Shutdown();
    IN_Shutdown();
}

// ============================================================================
// Подключение
// ============================================================================

void CL_ForceActive_f(void)
{
    cls.state = ca_active;
    key_dest = key_game;
    Con_Printf("Forced client to ACTIVE state\n");
}

void CL_Connect_f(void)
{
    const char* server;

    if (Cmd_Argc() < 2)
    {
        Con_Printf("usage: connect <server>\n");
        return;
    }

    server = Cmd_Argv(1);

    CL_Disconnect();

    if (!strcmp(server, "local"))
    {
        // Локальный сервер
        cls.state = ca_connected;
        Con_Printf("Connected to local server\n");
    }
    else
    {
        // Сетевой сервер
        if (!NET_StringToAdr(server, &cls.serveraddress))
        {
            Con_Printf("Bad server address: %s\n", server);
            return;
        }

        if (cls.serveraddress.port == 0)
            cls.serveraddress.port = BigShort(PORT_SERVER);

        cls.state = ca_connecting;
        cls.connect_time = cls.realtime;
        cls.connect_count = 0;

        Con_Printf("Connecting to %s...\n", NET_AdrToString(cls.serveraddress));
    }
}

void CL_Disconnect(void)
{
    if (cls.state == ca_disconnected)
        return;

    Con_Printf("Disconnected\n");

    // Отправка disconnect серверу
    if (cls.state >= ca_connected)
    {
        MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
        MSG_WriteString(&cls.netchan.message, "disconnect");
        Netchan_Transmit(&cls.netchan, 0, NULL);
    }

    cls.state = ca_disconnected;
    cls.demoplayback = FALSE;

    memset(&cl, 0, sizeof(cl));
}

void CL_Disconnect_f(void)
{
    CL_Disconnect();
}

void CL_Reconnect_f(void)
{
    if (cls.state < ca_connecting)
    {
        Con_Printf("No server to reconnect to\n");
        return;
    }

    CL_Disconnect();

    cls.state = ca_connecting;
    cls.connect_time = -9999;   // Немедленное переподключение
    cls.connect_count = 0;
}

// ============================================================================
// Основной кадр клиента
// ============================================================================

void CL_Frame(float frametime)
{
    static double last_print = 0;

    if (!cls.initialized) return;

    if (frametime <= 0.0001f || frametime > 0.1f)
        frametime = 0.01f;

    cls.frametime = frametime;
    cls.realtime += frametime;
    cl.time += frametime;

    if (cls.realtime - last_print > 1.0)
    {
        Con_Printf("Tick! ft=%f time=%f\n", frametime, cls.realtime);
        last_print = cls.realtime;
    }

    if (!cl_skip_render)
        IN_Frame();

    if (cls.state == ca_active)
    {
        CL_CreateCmd();

        if (sv.active && svs.maxclients > 0)
            svs.clients[0].lastcmd = cl.cmd;

        if (sv.active)
            SV_Frame(frametime);
    }

    if (cl_skip_render) return;

    // ============ РЕНДЕР (всё без изменений) ============
    R_BeginFrame();

    if (cls.state == ca_active)
        V_RenderView();
    else
        Draw_Background();

    Con_Draw();

    if (cls.state == ca_active)
        HUD_Draw();

    M_Draw();

    GL_Set2D();
    glDisable(GL_TEXTURE_2D);
    if ((int)(cls.realtime * 2) % 2 == 0)
        glColor3f(0, 1, 0);
    else
        glColor3f(1, 0, 0);

    glBegin(GL_QUADS);
    glVertex2i(10, 10);
    glVertex2i(20, 10);
    glVertex2i(20, 20);
    glVertex2i(10, 20);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);

    R_EndFrame();
}

// ============================================================================
// Команды серверу
// ============================================================================

void CL_ForwardToServer_f(void)
{
    if (cls.state < ca_connected)
    {
        Con_Printf("Not connected\n");
        return;
    }

    MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
    MSG_WriteString(&cls.netchan.message, Cmd_Args());
}

void CL_Viewpos_f(void)
{
    Con_Printf("Pos: %f %f %f\n", cl.vieworg[0], cl.vieworg[1], cl.vieworg[2]);
    Con_Printf("Ang: %f %f %f\n", cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]);
}

void CL_RenderOnly(void)
{
    if (!cls.initialized) return;

    Sys_SendKeyEvents();

    if (mouse_grabbed && (mouse_dx_accum != 0 || mouse_dy_accum != 0))
    {
        float sens = sensitivity ? sensitivity->value : 3.0f;

        cl.viewangles[1] -= mouse_dx_accum * sens * 0.022f;
        cl.viewangles[0] += mouse_dy_accum * sens * 0.022f;

        if (cl.viewangles[0] > 89.0f)  cl.viewangles[0] = 89.0f;
        if (cl.viewangles[0] < -89.0f) cl.viewangles[0] = -89.0f;

        mouse_dx_accum = 0;
        mouse_dy_accum = 0;
    }

    R_BeginFrame();

    if (cls.state == ca_active)
        V_RenderView();
    else
        Draw_Background();

    Con_Draw();

    if (cls.state == ca_active)
        HUD_Draw();

    M_Draw();

    GL_Set2D();
    glDisable(GL_TEXTURE_2D);
    if ((int)(cls.realtime * 2) % 2 == 0)
        glColor3f(0, 1, 0);
    else
        glColor3f(1, 0, 0);

    glBegin(GL_QUADS);
    glVertex2i(10, 10);
    glVertex2i(20, 10);
    glVertex2i(20, 20);
    glVertex2i(10, 20);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);

    R_EndFrame();
}
