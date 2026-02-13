#ifndef NET_H
#define NET_H

#include "quakedef.h"

// Sizebuf
typedef struct sizebuf_s {
    qboolean allowoverflow;
    qboolean overflowed;
    byte* data;
    int maxsize;
    int cursize;
} sizebuf_t;

typedef enum {
    NA_UNUSED,
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
    NA_IPX,
    NA_BROADCAST_IPX
} netadrtype_t;

// Net address
typedef struct netadr_s {
    int type;
    byte ip[4];
    unsigned short port;
} netadr_t;

// Net channel
typedef struct netchan_s {
    int sock;
    float last_received;
    float last_sent;            // Добавил
    netadr_t remote_address;
    int qport;

    // Sequence numbers
    int incoming_sequence;
    int incoming_acknowledged;
    int incoming_reliable_acknowledged;
    int incoming_reliable_sequence;
    int outgoing_sequence;
    int reliable_sequence;
    int last_reliable_sequence;

    sizebuf_t message;
    byte message_buf[MAX_MSGLEN]; // Добавил буфер

    // Reliable buffer
    int reliable_length;
    byte reliable_buf[MAX_MSGLEN]; // Добавил буфер

} netchan_t;

typedef enum { NS_CLIENT, NS_SERVER } netsrc_t;

#define PORT_CLIENT 27005
#define PORT_SERVER 27015
#define clc_move 2

void SZ_Init(sizebuf_t* buf, byte* data, int length);
void MSG_WriteByte(sizebuf_t* sb, int c);
void MSG_WriteShort(sizebuf_t* sb, int c);
void MSG_WriteAngle(sizebuf_t* sb, float f);
void Netchan_Transmit(netchan_t* chan, int length, byte* data);
void NET_SendPacket(netsrc_t sock, int length, void* data, netadr_t to);
void NET_Init(void);
void NET_Shutdown(void);
char* NET_AdrToString(netadr_t a);

// Message reading
void MSG_BeginReading(void);
int MSG_ReadChar(void);
int MSG_ReadByte(void);
int MSG_ReadShort(void);
int MSG_ReadLong(void);
float MSG_ReadFloat(void);
float MSG_ReadCoord(void);
float MSG_ReadAngle(void);
char* MSG_ReadString(void);

// Message writing  
void MSG_WriteChar(sizebuf_t* sb, int c);
void MSG_WriteLong(sizebuf_t* sb, int c);
void MSG_WriteFloat(sizebuf_t* sb, float f);
void MSG_WriteString(sizebuf_t* sb, const char* s);
void MSG_WriteCoord(sizebuf_t* sb, float f);

// Network functions
int NET_GetPacket(netsrc_t sock, netadr_t* from, sizebuf_t* message);
int NET_StringToAdr(const char* s, netadr_t* a);
int NET_CompareAdr(netadr_t a, netadr_t b);

// Netchan
void Netchan_Setup(netsrc_t sock, netchan_t* chan, netadr_t adr, int qport);
qboolean Netchan_Process(netchan_t* chan);

#endif // NET_H
