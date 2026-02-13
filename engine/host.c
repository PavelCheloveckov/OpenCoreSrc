// engine/host.c
#include "common.h"
#include "console.h"
#include "cmd.h"
#include "cvar.h"
#include "server.h"
#include "client.h"
#include "net.h"
#include "pak.h"
#include "gl_local.h"
#include <string.h>
#include <keys.h>

// Host state
qboolean host_initialized = FALSE;
double host_frametime;
double realtime;
double oldrealtime;
int host_framecount;
double host_time = 0;

// Cvars
cvar_t* host_speeds;
cvar_t* host_framerate;
cvar_t* developer;
cvar_t* fps_max;

// ============================================================================
// Initialization
// ============================================================================

void Host_Maps_f(void)
{
    PAK_ListMaps();
}

void Host_Init(void)
{
    // Console
    Con_Init();

    PAK_Init();
    Con_Printf("Host_Init\n");

    // Random seed
    SeedRandomNumberGenerator();

    // Memory init
    // Mem_Init();  // if using custom allocator

    // Commands and cvars
    Cmd_Init();
    Con_CheckResize();
    Cvar_Init();

    // Cvars
    host_speeds = Cvar_Get("host_speeds", "0", 0);
    host_framerate = Cvar_Get("host_framerate", "0", 0);
    developer = Cvar_Get("developer", "0", 0);
    fps_max = Cvar_Get("fps_max", "100", FCVAR_ARCHIVE);

    // Network
    NET_Init();

    // Server
    SV_Init();

    // Client (if not dedicated)
#ifndef DEDICATED
    CL_Init();
#endif

    M_Init();

    // Register commands
    Cmd_AddCommand("quit", Host_Quit_f);
    Cmd_AddCommand("map", Host_Map_f);
    Cmd_AddCommand("maps", Host_Maps_f);

    host_initialized = TRUE;

    // Execute configs
    Cbuf_AddText("exec config.cfg\n");
    Cbuf_AddText("exec autoexec.cfg\n");
    Cbuf_Execute();

    Con_Printf("======== Host Initialized ========\n");
}

void Host_Shutdown(void)
{
    if (!host_initialized)
        return;

    host_initialized = FALSE;

    Con_Printf("Host_Shutdown\n");

    // Write config
    Host_WriteConfig();

    // Shutdown subsystems
#ifndef DEDICATED
    CL_Shutdown();
#endif
    SV_Shutdown();
    NET_Shutdown();

    Con_Shutdown();
    Cvar_Shutdown();
    Cmd_Shutdown();
}

// ============================================================================
// Commands
// ============================================================================

void Host_Quit_f(void)
{
    Con_Printf("Shutting down...\n");
    host_initialized = FALSE;  // Signal main loop to exit
}

void Host_Map_f(void)
{
    char name[MAX_QPATH];

    if (Cmd_Argc() < 2)
    {
        Con_Printf("map <mapname> : start a new map\n");
        return;
    }

    // Disconnect clients
    Host_Disconnect_f();

    Q_strncpy(name, Cmd_Argv(1), sizeof(name));

    SV_SpawnServer(name, NULL);

    if (sv.active)
    {
        // Сразу делаем клиента активным, так как мы и есть сервер
        cls.state = ca_active; 
        
        // Очищаем консоль, чтобы видеть игру
        key_dest = key_game;
        
        Con_Printf("Local game started.\n");
    }
}

void Host_Disconnect_f(void)
{
#ifndef DEDICATED
    CL_Disconnect();
#endif

    // If running a server, shut it down
    if (sv.active)
    {
        SV_Shutdown();
    }
}

void Host_WriteConfig(void)
{
    FILE* f;

    f = fopen("config.cfg", "w");
    if (!f)
    {
        Con_Printf("Couldn't write config.cfg\n");
        return;
    }

    fprintf(f, "// This file is overwritten whenever you exit.\n");
    fprintf(f, "// Use autoexec.cfg for custom settings.\n\n");

    // Write cvars with FCVAR_ARCHIVE
    Cvar_WriteVariables(f);

    // Write key bindings
    // Key_WriteBindings(f);

    fclose(f);
}

// ============================================================================
// Frame
// ============================================================================

void Host_FilterTime(float* time)
{
    float fps;

    realtime = Sys_FloatTime();
    *time = realtime - oldrealtime;
    oldrealtime = realtime;

    // Ограничение fps
    if (fps_max->value > 0)
    {
        fps = 1.0f / fps_max->value;
        if (*time < fps)
        {
            Sys_Sleep((int)((fps - *time) * 1000));
            *time = fps;
        }
    }

    // Ограничение на случай лагов
    if (*time > 0.25f)
        *time = 0.25f;

    // Фиксированный framerate для отладки
    if (host_framerate->value > 0)
        *time = host_framerate->value;
}

void Host_Frame(float time)
{
    static double time1 = 0;
    static double time2 = 0;
    static double time3 = 0;

    if (!host_initialized)
        return;

    if (time <= 0.0001f || time > 0.1f)
        time = 0.01f;

    host_frametime = (double)time;
    host_time += host_frametime;
    host_framecount++;

    if (host_speeds->value)
        time1 = Sys_FloatTime();

    Cbuf_Execute();

    if (sv.active && sv.edicts)
    {
    }

    if (host_speeds->value)
        time2 = Sys_FloatTime();

#ifndef DEDICATED
    CL_Frame(time);
#endif

    if (host_speeds->value)
    {
        time3 = Sys_FloatTime();
        Con_Printf("host_speeds: sv=%5.2f cl=%5.2f total=%5.2f\n",
            (time2 - time1) * 1000,
            (time3 - time2) * 1000,
            (time3 - time1) * 1000);
    }
}

// ============================================================================
// Main loop (platform specific entry point calls this)
// ============================================================================

void Host_MainLoop(void)
{
    float time;

    oldrealtime = Sys_FloatTime();

    while (host_initialized)
    {
        Host_FilterTime(&time);
        Host_Frame(time);
    }
}

// ============================================================================
// Entry point (example for console app)
// ============================================================================

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    // Parse command line
    // COM_InitArgv(argc, argv);

    Host_Init();
    Host_MainLoop();
    Host_Shutdown();

    return 0;
}
