// network/net_chan.c
#include "quakedef.h"
#include "server.h" 

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif
#include <cvar.h>

int msg_badread = 0;
static int ip_socket = -1;
static cvar_t* net_showpackets;
sizebuf_t net_message;

// ============================================================================
// Size Buffer
// ============================================================================

void SZ_Init(sizebuf_t* buf, byte* data, int length)
{
    memset(buf, 0, sizeof(*buf));
    buf->data = data;
    buf->maxsize = length;
}

void SZ_Clear(sizebuf_t* buf)
{
    buf->cursize = 0;
    buf->overflowed = FALSE;
}

void* SZ_GetSpace(sizebuf_t* buf, int length)
{
    void* data;

    if (buf->cursize + length > buf->maxsize)
    {
        if (!buf->allowoverflow)
            Sys_Error("SZ_GetSpace: overflow without allowoverflow set");

        if (length > buf->maxsize)
            Sys_Error("SZ_GetSpace: %d is > full buffer size", length);

        buf->overflowed = TRUE;
        Con_Printf("SZ_GetSpace: overflow\n");
        SZ_Clear(buf);
    }

    data = buf->data + buf->cursize;
    buf->cursize += length;

    return data;
}

void SZ_Write(sizebuf_t* buf, const void* data, int length)
{
    memcpy(SZ_GetSpace(buf, length), data, length);
}

// ============================================================================
// Message writing
// ============================================================================

void MSG_WriteChar(sizebuf_t* sb, int c)
{
    byte* buf = (byte*)SZ_GetSpace(sb, 1);
    buf[0] = c;
}

void MSG_WriteByte(sizebuf_t* sb, int c)
{
    byte* buf = (byte*)SZ_GetSpace(sb, 1);
    buf[0] = c;
}

void MSG_WriteShort(sizebuf_t* sb, int c)
{
    byte* buf = (byte*)SZ_GetSpace(sb, 2);
    buf[0] = c & 0xff;
    buf[1] = (c >> 8) & 0xff;
}

void MSG_WriteLong(sizebuf_t* sb, int c)
{
    byte* buf = (byte*)SZ_GetSpace(sb, 4);
    buf[0] = c & 0xff;
    buf[1] = (c >> 8) & 0xff;
    buf[2] = (c >> 16) & 0xff;
    buf[3] = (c >> 24) & 0xff;
}

void MSG_WriteFloat(sizebuf_t* sb, float f)
{
    union {
        float f;
        int l;
    } dat;

    dat.f = f;
    MSG_WriteLong(sb, dat.l);
}

void MSG_WriteString(sizebuf_t* sb, const char* s)
{
    if (!s)
        SZ_Write(sb, "", 1);
    else
        SZ_Write(sb, s, strlen(s) + 1);
}

void MSG_WriteCoord(sizebuf_t* sb, float f)
{
    // 1/8 единицы точность (как в GoldSrc)
    MSG_WriteShort(sb, (int)(f * 8));
}

void MSG_WriteAngle(sizebuf_t* sb, float f)
{
    MSG_WriteByte(sb, (int)(f * 256 / 360) & 255);
}

// ============================================================================
// Message reading
// ============================================================================

static byte* msg_readdata;
static int msg_readcount;

void MSG_BeginReading(void)
{
    msg_readcount = 0;
    msg_badread = FALSE;
}

int MSG_ReadChar(void)
{
    int c;
    if (msg_readcount + 1 > net_message.cursize)
    {
        msg_badread = TRUE;
        return -1;
    }
    c = (signed char)net_message.data[msg_readcount];
    msg_readcount++;
    return c;
}

int MSG_ReadByte(void)
{
    int c;
    if (msg_readcount + 1 > net_message.cursize)
    {
        msg_badread = TRUE;
        return -1;
    }
    c = (unsigned char)net_message.data[msg_readcount];
    msg_readcount++;
    return c;
}

int MSG_ReadShort(void)
{
    int c;
    if (msg_readcount + 2 > net_message.cursize)
    {
        msg_badread = TRUE;
        return -1;
    }
    c = (short)(net_message.data[msg_readcount]
        + (net_message.data[msg_readcount + 1] << 8));
    msg_readcount += 2;
    return c;
}

int MSG_ReadLong(void)
{
    int c;
    if (msg_readcount + 4 > net_message.cursize)
    {
        msg_badread = TRUE;
        return -1;
    }
    c = net_message.data[msg_readcount]
        + (net_message.data[msg_readcount + 1] << 8)
        + (net_message.data[msg_readcount + 2] << 16)
        + (net_message.data[msg_readcount + 3] << 24);
    msg_readcount += 4;
    return c;
}

float MSG_ReadFloat(void)
{
    union {
        byte b[4];
        float f;
    } dat;

    dat.b[0] = MSG_ReadByte();
    dat.b[1] = MSG_ReadByte();
    dat.b[2] = MSG_ReadByte();
    dat.b[3] = MSG_ReadByte();

    return dat.f;
}

float MSG_ReadCoord(void)
{
    return MSG_ReadShort() * (1.0f / 8);
}

float MSG_ReadAngle(void)
{
    return MSG_ReadChar() * (360.0f / 256);
}

char* MSG_ReadString(void)
{
    static char string[2048];
    int l, c;

    l = 0;
    do
    {
        c = MSG_ReadChar();
        if (c == -1 || c == 0)
            break;
        string[l] = c;
        l++;
    } while (l < sizeof(string) - 1);

    string[l] = 0;
    return string;
}

// ============================================================================
// Net channel
// ============================================================================

void Netchan_Setup(netsrc_t sock, netchan_t* chan, netadr_t adr, int qport)
{
    memset(chan, 0, sizeof(*chan));

    chan->sock = sock;
    chan->remote_address = adr;
    chan->qport = qport;
    chan->last_received = svs.realtime;

    SZ_Init(&chan->message, chan->message_buf, sizeof(chan->message_buf));
    chan->message.allowoverflow = TRUE;
}

void Netchan_Transmit(netchan_t* chan, int length, byte* data)
{
    sizebuf_t send;
    byte send_buf[MAX_MSGLEN + 8];
    qboolean send_reliable;
    int w1, w2;

    // Проверка необходимости отправки надёжного сообщения
    send_reliable = FALSE;

    if (chan->incoming_acknowledged > chan->last_reliable_sequence &&
        chan->incoming_reliable_acknowledged != chan->reliable_sequence)
    {
        send_reliable = TRUE;
    }

    // Если есть данные для надёжной отправки
    if (!chan->reliable_length && chan->message.cursize)
    {
        memcpy(chan->reliable_buf, chan->message_buf, chan->message.cursize);
        chan->reliable_length = chan->message.cursize;
        chan->message.cursize = 0;
        chan->reliable_sequence ^= 1;
        send_reliable = TRUE;
    }

    // Составление заголовка
    w1 = chan->outgoing_sequence | (send_reliable << 31);
    w2 = chan->incoming_sequence | (chan->incoming_reliable_sequence << 31);

    chan->outgoing_sequence++;
    chan->last_sent = svs.realtime;

    SZ_Init(&send, send_buf, sizeof(send_buf));

    MSG_WriteLong(&send, w1);
    MSG_WriteLong(&send, w2);

    // Добавление надёжных данных
    if (send_reliable)
    {
        SZ_Write(&send, chan->reliable_buf, chan->reliable_length);
        chan->last_reliable_sequence = chan->outgoing_sequence;
    }

    // Добавление ненадёжных данных
    if (length)
    {
        SZ_Write(&send, data, length);
    }

    // Отправка
    NET_SendPacket(chan->sock, send.cursize, send.data, chan->remote_address);

    chan->message.cursize = 0;
}

qboolean Netchan_Process(netchan_t* chan)
{
    int sequence, sequence_ack;
    int reliable_ack, reliable_message;

    MSG_BeginReading();

    sequence = MSG_ReadLong();
    sequence_ack = MSG_ReadLong();

    reliable_message = sequence >> 31;
    reliable_ack = sequence_ack >> 31;

    sequence &= ~(1 << 31);
    sequence_ack &= ~(1 << 31);

    // Дублированный пакет
    if (sequence <= chan->incoming_sequence)
    {
        return FALSE;
    }

    // Пропущенные пакеты
    if (sequence > chan->incoming_sequence + 1)
    {
        // Потеря пакетов
        Con_DPrintf("Dropped %d packets\n", sequence - chan->incoming_sequence - 1);
    }

    // Обновление состояния
    chan->incoming_sequence = sequence;
    chan->incoming_acknowledged = sequence_ack;
    chan->incoming_reliable_acknowledged = reliable_ack;

    if (reliable_message)
    {
        chan->incoming_reliable_sequence ^= 1;
    }

    // Очистка надёжного буфера если подтверждено
    if (reliable_ack == chan->reliable_sequence)
    {
        chan->reliable_length = 0;
    }

    chan->last_received = svs.realtime;

    return TRUE;
}
