// engine/client/cl_parse.c
#include "quakedef.h"
#include "common.h"
#include "cmd.h"
#include "console.h"
#include "mathlib.h"
#include "client.h"
#include "net.h"
#include <entity_state.h>
#include <server.h>

// ============================================================================
// Чтение пакетов
// ============================================================================

void CL_ReadPackets(void)
{
    netadr_t from;
    sizebuf_t net_message;
    byte net_message_buffer[MAX_MSGLEN];

    SZ_Init(&net_message, net_message_buffer, sizeof(net_message_buffer));

    while (NET_GetPacket(NS_CLIENT, &from, &net_message))
    {
        // Проверка адреса
        if (!NET_CompareAdr(from, cls.serveraddress))
        {
            Con_DPrintf("Packet from unknown source\n");
            continue;
        }

        // Обработка через netchan
        if (!Netchan_Process(&cls.netchan))
            continue;

        // Парсинг сообщения
        CL_ParseServerMessage();
    }

    // Проверка timeout
    if (cls.state >= ca_connected)
    {
        if (cls.realtime - cls.netchan.last_received > 30)
        {
            Con_Printf("Server connection timed out\n");
            CL_Disconnect();
        }
    }
}

// ============================================================================
// Парсинг сообщений сервера
// ============================================================================

void CL_ParseServerMessage(void)
{
    int cmd;

    MSG_BeginReading();

    while (1)
    {
        if (msg_badread)
        {
            Con_Printf("CL_ParseServerMessage: Bad read\n");
            break;
        }

        cmd = MSG_ReadByte();

        if (cmd == -1)
            break;  // Конец сообщения

        switch (cmd)
        {
        case svc_nop:
            break;

        case svc_disconnect:
            Con_Printf("Server disconnected\n");
            CL_Disconnect();
            return;

        case svc_print:
            Con_Printf("%s", MSG_ReadString());
            break;

        case svc_stufftext:
            Cbuf_AddText(MSG_ReadString());
            break;

        case svc_serverinfo:
            CL_ParseServerInfo();
            break;

        case svc_time:
            cl.time = MSG_ReadFloat();
            break;

        case svc_clientdata:
            CL_ParseClientData();
            break;

        case svc_packetentities:
            CL_ParsePacketEntities();
            break;

        default:
            Con_Printf("CL_ParseServerMessage: Unknown command %d\n", cmd);
            return;
        }
    }
}

// ============================================================================
// Парсинг данных
// ============================================================================

void CL_ParseServerInfo(void)
{
    char* str;
    int i;

    Con_Printf("Parsing server info\n");

    cl.servercount = MSG_ReadLong();

    // Имя карты
    str = MSG_ReadString();
    Con_Printf("Map: %s\n", str);

    // Загрузка карты
    // cl.worldmodel = Mod_LoadBrushModel(va("maps/%s.bsp", str));

    // Precache моделей
    for (i = 1; ; i++)
    {
        str = MSG_ReadString();
        if (!str[0])
            break;

        if (i >= MAX_MODELS)
        {
            Con_Printf("Server sent too many models\n");
            break;
        }

        Q_strncpy(cl.model_name[i], str, sizeof(cl.model_name[i]));
        Con_DPrintf("Model %d: %s\n", i, str);
    }

    // Precache звуков
    for (i = 1; ; i++)
    {
        str = MSG_ReadString();
        if (!str[0])
            break;

        if (i >= MAX_SOUNDS)
        {
            Con_Printf("Server sent too many sounds\n");
            break;
        }

        Q_strncpy(cl.sound_name[i], str, sizeof(cl.sound_name[i]));
        Con_DPrintf("Sound %d: %s\n", i, str);
    }

    cls.state = ca_active;
    Con_Printf("Client active\n");
}

void CL_ParseClientData(void)
{
    // Данные о состоянии игрока
    cl.vieworg[0] = MSG_ReadCoord();
    cl.vieworg[1] = MSG_ReadCoord();
    cl.vieworg[2] = MSG_ReadCoord();

    cl.velocity[0] = MSG_ReadCoord();
    cl.velocity[1] = MSG_ReadCoord();
    cl.velocity[2] = MSG_ReadCoord();

    cl.onground = MSG_ReadByte();
    cl.waterlevel = MSG_ReadByte();

    cl.punchangle[0] = MSG_ReadAngle();
    cl.punchangle[1] = MSG_ReadAngle();
    cl.punchangle[2] = MSG_ReadAngle();

    // Статистика
    for (int i = 0; i < 8; i++)
    {
        cl.stats[i] = MSG_ReadShort();
    }
}

void CL_ParsePacketEntities(void)
{
    int num;

    while (1)
    {
        num = MSG_ReadShort();

        if (num == 0)
            break;

        if (num >= MAX_EDICTS)
        {
            Con_Printf("CL_ParsePacketEntities: bad entity number %d\n", num);
            break;
        }

        // Чтение состояния сущности (используем упрощённое хранение)
        int bits = MSG_ReadByte();

        if (bits & 1) cl.ent_origins[num][0] = MSG_ReadCoord();
        if (bits & 2) cl.ent_origins[num][1] = MSG_ReadCoord();
        if (bits & 4) cl.ent_origins[num][2] = MSG_ReadCoord();
        if (bits & 8) cl.ent_angles[num][0] = MSG_ReadAngle();
        if (bits & 16) cl.ent_angles[num][1] = MSG_ReadAngle();
        if (bits & 32) cl.ent_angles[num][2] = MSG_ReadAngle();
        if (bits & 64) cl.ent_models[num] = MSG_ReadByte();
        if (bits & 128) {
            MSG_ReadByte();
        }
    }
}
