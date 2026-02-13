// engine/common.c
#include "quakedef.h"
#include "pak.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

char com_token[1024];
qboolean com_eof = FALSE;

// ============================================================================
// String functions
// ============================================================================

int Q_stricmp(const char* s1, const char* s2)
{
    while (1)
    {
        int c1 = tolower((unsigned char)*s1++);
        int c2 = tolower((unsigned char)*s2++);

        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            return 0;
    }
}

int Q_strnicmp(const char* s1, const char* s2, int n)
{
    int c1, c2;

    while (n--)
    {
        c1 = tolower((unsigned char)*s1++);
        c2 = tolower((unsigned char)*s2++);

        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            return 0;
    }
    return 0;
}

void Q_strncpy(char* dest, const char* src, int count)
{
    if (!dest || !src || count <= 0)
        return;

    while (count && (*dest++ = *src++))
        count--;

    if (!count)
        dest[-1] = 0;
}

// ============================================================================
// va function (variable arguments to string)
// ============================================================================

#define VA_BUFFERS 4
#define VA_BUFFER_SIZE 1024

char* va(const char* format, ...)
{
    static char buffers[VA_BUFFERS][VA_BUFFER_SIZE];
    static int index = 0;
    char* buf;
    va_list args;

    buf = buffers[index];
    index = (index + 1) % VA_BUFFERS;

    va_start(args, format);
    vsnprintf(buf, VA_BUFFER_SIZE, format, args);
    va_end(args);

    return buf;
}

// ============================================================================
// Parsing
// ============================================================================

char* COM_Parse(char* data)
{
    int c;
    int len = 0;

    com_token[0] = 0;
    com_eof = FALSE;

    if (!data)
    {
        com_eof = TRUE;
        return NULL;
    }

    // Skip whitespace
skipwhite:
    while ((c = *data) <= ' ')
    {
        if (c == 0)
        {
            com_eof = TRUE;
            return NULL;
        }
        data++;
    }

    // Skip // comments
    if (c == '/' && data[1] == '/')
    {
        while (*data && *data != '\n')
            data++;
        goto skipwhite;
    }

    // Skip /* */ comments
    if (c == '/' && data[1] == '*')
    {
        data += 2;
        while (*data && !(*data == '*' && data[1] == '/'))
            data++;
        if (*data)
            data += 2;
        goto skipwhite;
    }

    // Handle quoted strings
    if (c == '"')
    {
        data++;
        while (1)
        {
            c = *data++;
            if (c == '"' || !c)
            {
                com_token[len] = 0;
                return data;
            }
            if (len < sizeof(com_token) - 1)
                com_token[len++] = c;
        }
    }

    // Parse a regular word
    do
    {
        if (len < sizeof(com_token) - 1)
            com_token[len++] = c;
        data++;
        c = *data;
    } while (c > ' ');

    com_token[len] = 0;
    return data;
}

// ============================================================================
// File functions
// ============================================================================

void* COM_LoadFile(const char* path, int* length)
{
    FILE* f;
    void* buf;
    long len;

    if (length) *length = 0;

    // пробуем gamedir/path
    {
        char full[MAX_OSPATH];
        snprintf(full, sizeof(full), "%s/%s", PAK_GetGameDir(), path);
        f = fopen(full, "rb");
        if (!f)
        {
            // пробуем просто path как есть
            f = fopen(path, "rb");
            if (!f)
                return NULL;
        }
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = malloc(len + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, len, f);
    ((char*)buf)[len] = 0;
    fclose(f);

    if (length)
        *length = (int)len;

    return buf;
}

void COM_FreeFile(void* buffer)
{
    if (buffer)
        free(buffer);
}

int COM_FileExists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f)
    {
        fclose(f);
        return TRUE;
    }
    return FALSE;
}

void COM_CreatePath(const char* path)
{
    char temp[MAX_OSPATH];
    char* ofs;

    Q_strncpy(temp, path, sizeof(temp));

    for (ofs = temp + 1; *ofs; ofs++)
    {
        if (*ofs == '/' || *ofs == '\\')
        {
            *ofs = 0;
#ifdef _WIN32
            CreateDirectoryA(temp, NULL);
#else
            mkdir(temp, 0755);
#endif
            * ofs = '/';
        }
    }
}

// ============================================================================
// Byte order
// ============================================================================

static int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = { 0x01020304 };
    return bint.c[0] == 1;
}

short LittleShort(short l)
{
    if (is_big_endian())
        return ((l >> 8) & 0xff) | ((l << 8) & 0xff00);
    return l;
}

int LittleLong(int l)
{
    if (is_big_endian())
    {
        return ((l >> 24) & 0xff) | ((l >> 8) & 0xff00) |
            ((l << 8) & 0xff0000) | ((l << 24) & 0xff000000);
    }
    return l;
}

float LittleFloat(float f)
{
    if (is_big_endian())
    {
        union { float f; int l; } dat1, dat2;
        dat1.f = f;
        dat2.l = LittleLong(dat1.l);
        return dat2.f;
    }
    return f;
}

short BigShort(short l)
{
    if (!is_big_endian())
        return ((l >> 8) & 0xff) | ((l << 8) & 0xff00);
    return l;
}

int BigLong(int l)
{
    if (!is_big_endian())
    {
        return ((l >> 24) & 0xff) | ((l >> 8) & 0xff00) |
            ((l << 8) & 0xff0000) | ((l << 24) & 0xff000000);
    }
    return l;
}

float BigFloat(float f)
{
    if (!is_big_endian())
    {
        union { float f; int l; } dat1, dat2;
        dat1.f = f;
        dat2.l = BigLong(dat1.l);
        return dat2.f;
    }
    return f;
}

// ============================================================================
// CRC
// ============================================================================

static unsigned short crctable[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    // ... (полная таблица CRC16-CCITT)
};

unsigned short CRC_Block(byte* start, int count)
{
    unsigned short crc = 0xFFFF;
    while (count--)
        crc = (crc << 8) ^ crctable[(crc >> 8) ^ *start++];
    return crc;
}

// ============================================================================
// Memory
// ============================================================================

void* Mem_Alloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

void* Mem_Realloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

void Mem_Free(void* ptr)
{
    if (ptr)
        free(ptr);
}

// ============================================================================
// Info strings (key\value\key\value...)
// ============================================================================

#define MAX_INFO_KEY    256
#define MAX_INFO_VALUE  256
#define MAX_INFO_STRING 256

char* Info_ValueForKey(const char* s, const char* key)
{
    static char value[4][MAX_INFO_VALUE];
    static int valueindex;
    char pkey[MAX_INFO_KEY];
    char* o;

    valueindex = (valueindex + 1) % 4;

    if (*s == '\\')
        s++;

    while (1)
    {
        o = pkey;
        while (*s != '\\')
        {
            if (!*s)
                return "";
            *o++ = *s++;
        }
        *o = 0;
        s++;

        o = value[valueindex];
        while (*s != '\\' && *s)
            *o++ = *s++;
        *o = 0;

        if (!Q_stricmp(key, pkey))
            return value[valueindex];

        if (!*s)
            return "";
        s++;
    }
}

void Info_RemoveKey(char* s, const char* key)
{
    char* start;
    char pkey[MAX_INFO_KEY];
    char* o;

    if (strchr(key, '\\'))
        return;

    while (1)
    {
        start = s;
        if (*s == '\\')
            s++;

        o = pkey;
        while (*s != '\\')
        {
            if (!*s)
                return;
            *o++ = *s++;
        }
        *o = 0;
        s++;

        while (*s != '\\' && *s)
            s++;

        if (!Q_stricmp(key, pkey))
        {
            memmove(start, s, strlen(s) + 1);
            return;
        }

        if (!*s)
            return;
    }
}

void Info_SetValueForKey(char* s, const char* key, const char* value, int maxsize)
{
    char newi[MAX_INFO_STRING];

    if (strchr(key, '\\') || strchr(value, '\\'))
        return;
    if (strchr(key, '"') || strchr(value, '"'))
        return;

    Info_RemoveKey(s, key);

    if (!value || !*value)
        return;

    snprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

    if ((int)(strlen(s) + strlen(newi)) >= maxsize)
        return;

    strcat(s, newi);
}
