#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "quakedef.h"
#include "net.h"
#include <winsock2.h>

netadr_t	net_local_adr;

// Перевод адреса в строку "127.0.0.1:27015"
char* NET_AdrToString(netadr_t a)
{
    static char	s[64];

    if (a.type == NA_LOOPBACK)
        sprintf(s, "loopback");
    else if (a.type == NA_IP)
        sprintf(s, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
    else
        sprintf(s, "%02x:%02x:%02x:%02x:%02x:%02x", a.ip[0], a.ip[1], a.ip[2], a.ip[3], a.ip[4], a.ip[5]);

    return s;
}

// Перевод строки в адрес
int NET_StringToAdr(const char* s, netadr_t* a)
{
    struct sockaddr_in sa;
    int port = 0;
    char copy[128];
    char* colon;

    if (!strcmp(s, "localhost"))
    {
        a->type = NA_LOOPBACK;
        return 1;
    }

    strcpy(copy, s);
    colon = strchr(copy, ':');
    if (colon)
    {
        *colon = 0;
        port = atoi(colon + 1);
    }

    a->type = NA_IP;
    a->port = htons((short)port);

    if (copy[0] >= '0' && copy[0] <= '9')
    {
        *(int*)&a->ip = inet_addr(copy);
    }
    else
    {
        struct hostent* h = gethostbyname(copy);
        if (!h) return 0;
        *(int*)&a->ip = *(int*)h->h_addr_list[0];
    }

    return 1;
}

// Сравнение двух адресов
int NET_CompareAdr(netadr_t a, netadr_t b)
{
    if (a.type != b.type)
        return 0;

    if (a.type == NA_LOOPBACK)
        return 1;

    if (a.type == NA_IP)
    {
        if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
            return 1;
        return 0;
    }
    return 0;
}

int NET_GetPacket(netsrc_t sock, netadr_t* from, sizebuf_t* message)
{
    return 0;  // Нет пакетов
}

void NET_SendPacket(int sock, int len, void* data, netadr_t to)
{
    struct sockaddr_in addr;

    if (to.type == NA_LOOPBACK) return; // Loopback обрабатывается отдельно

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = *(int*)&to.ip;
    addr.sin_port = to.port;

    sendto(sock, (char*)data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
}

// Заглушки инициализации, если их нет
void NET_Init(void)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    // Тут обычно создается сокет
}

void NET_Shutdown(void)
{
    WSACleanup();
}
