#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "pak.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char id[4];
    int diroffset;
    int dirlen;
} dpackheader_t;

typedef struct {
    char name[56];
    int filepos;
    int filelen;
} dpackfile_t;

typedef struct {
    char name[56];
    int filepos;
    int filelen;
} packfile_t;

typedef struct {
    char filename[MAX_OSPATH];
    FILE* handle;
    int numfiles;
    packfile_t* files;
} pack_t;

#define MAX_PAKS 16

static pack_t* paks[MAX_PAKS];
static int numpaks = 0;
static char pak_gamedir[MAX_OSPATH] = "id1";
const char* PAK_GetGameDir(void) { return pak_gamedir; }
static pack_t* PAK_LoadOnePak(const char* filename);

static void NormalizePath(char* out, const char* in, int outSize)
{
    int i = 0;
    while (*in && i < outSize - 1)
    {
        char c = *in++;
        if (c == '\\') c = '/';
        // пак имена обычно в lower-case; сравнение у тебя Q_stricmp - ок
        out[i++] = c;
    }
    out[i] = 0;
}

static pack_t* PAK_LoadPackFile(const char* pakpath)
{
    dpackheader_t header;
    dpackfile_t* dfiles;
    pack_t* pak;
    FILE* f;
    int i;

    f = fopen(pakpath, "rb");
    if (!f) return NULL;

    if (fread(&header, 1, sizeof(header), f) != sizeof(header))
    {
        fclose(f);
        return NULL;
    }

    if (strncmp(header.id, "PACK", 4))
    {
        fclose(f);
        return NULL;
    }

    pak = (pack_t*)malloc(sizeof(pack_t));
    memset(pak, 0, sizeof(*pak));

    Q_strncpy(pak->filename, pakpath, sizeof(pak->filename));
    pak->handle = f;
    pak->numfiles = header.dirlen / sizeof(dpackfile_t);
    pak->files = (packfile_t*)malloc(pak->numfiles * sizeof(packfile_t));

    fseek(f, header.diroffset, SEEK_SET);

    dfiles = (dpackfile_t*)malloc(header.dirlen);
    fread(dfiles, 1, header.dirlen, f);

    for (i = 0; i < pak->numfiles; i++)
    {
        // гарантируем \0
        Q_strncpy(pak->files[i].name, dfiles[i].name, sizeof(pak->files[i].name));
        pak->files[i].filepos = dfiles[i].filepos;
        pak->files[i].filelen = dfiles[i].filelen;
    }

    free(dfiles);

    Con_Printf("Loaded %s (%d files)\n", pak->filename, pak->numfiles);
    return pak;
}

void PAK_ListMaps(void)
{
    int count = 0;
    WIN32_FIND_DATAA fd;
    HANDLE hFind;

    Con_Printf("--- Maps ---\n");

    // Ищем в папке maps/
    hFind = FindFirstFileA("maps/*.qbsp", &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                // Убираем расширение для красоты
                char name[256];
                strncpy(name, fd.cFileName, sizeof(name));
                char* dot = strrchr(name, '.');
                if (dot) *dot = 0;

                Con_Printf("  %s\n", name);
                count++;
            }
        } while (FindNextFileA(hFind, &fd));

        FindClose(hFind);
    }

    int p, i;
    for (p = 0; p < numpaks; p++)
    {
        pack_t* pak = paks[p];
        if (!pak) continue;

        for (i = 0; i < pak->numfiles; i++)
        {
            const char* fname = pak->files[i].name;
            int len = (int)strlen(fname);

            if (len > 5 &&
                !strncmp(fname, "maps/", 5) &&
                (!Q_stricmp(fname + len - 5, ".qbsp") || !Q_stricmp(fname + len - 4, ".bsp")))
            {
                Con_Printf("  %s (pak)\n", fname);
                count++;
            }
        }
    }

    if (count == 0)
        Con_Printf("  No maps found.\n");
    else
        Con_Printf("%d map(s) found.\n", count);
}

void PAK_Shutdown(void)
{
    for (int i = 0; i < numpaks; i++)
    {
        pack_t* p = paks[i];
        if (!p) continue;
        if (p->handle) fclose(p->handle);
        if (p->files) free(p->files);
        free(p);
        paks[i] = NULL;
    }
    numpaks = 0;
}

void PAK_InitForDirEx(const char* gamedir, int usePak0, int usePak1)
{
    char path[MAX_OSPATH];

    PAK_Shutdown();

    if (gamedir && gamedir[0])
        Q_strncpy(pak_gamedir, gamedir, sizeof(pak_gamedir));

    numpaks = 0;

    if (usePak0)
    {
        snprintf(path, sizeof(path), "%s/pak0.pak", pak_gamedir);
        pack_t* p = PAK_LoadOnePak(path);
        if (p && numpaks < 2) paks[numpaks++] = p;
    }

    if (usePak1)
    {
        snprintf(path, sizeof(path), "%s/pak1.pak", pak_gamedir);
        pack_t* p = PAK_LoadOnePak(path);
        if (p && numpaks < 2) paks[numpaks++] = p;
    }

    Con_Printf("PAK: total loaded paks = %d\n", numpaks);
}

void PAK_InitForDir(const char* gamedir)
{
    int i;
    char pakpath[MAX_OSPATH];

    PAK_Shutdown();

    if (gamedir && gamedir[0])
        Q_strncpy(pak_gamedir, gamedir, sizeof(pak_gamedir));

    // грузим pak0..pak9 (как в Quake)
    for (i = 0; i <= 9; i++)
    {
        snprintf(pakpath, sizeof(pakpath), "%s/pak%d.pak", pak_gamedir, i);

        pack_t* p = PAK_LoadPackFile(pakpath);
        if (!p) continue;

        if (numpaks < MAX_PAKS)
            paks[numpaks++] = p;
        else
        {
            // если переполнили список - просто выгружаем
            if (p->handle) fclose(p->handle);
            if (p->files) free(p->files);
            free(p);
            break;
        }
    }
}

void PAK_Init(void)
{
    PAK_InitForDirEx("id1", 1, 1);
}

static pack_t* PAK_LoadOnePak(const char* filename)
{
    dpackheader_t header;
    dpackfile_t* files;
    FILE* f;
    int i;

    f = fopen(filename, "rb");
    if (!f) return NULL;

    fread(&header, 1, sizeof(header), f);
    if (strncmp(header.id, "PACK", 4)) { fclose(f); return NULL; }

    pack_t* pak = (pack_t*)malloc(sizeof(pack_t));
    memset(pak, 0, sizeof(*pak));

    Q_strncpy(pak->filename, filename, sizeof(pak->filename));
    pak->handle = f;
    pak->numfiles = header.dirlen / sizeof(dpackfile_t);
    pak->files = (packfile_t*)malloc(pak->numfiles * sizeof(packfile_t));

    fseek(f, header.diroffset, SEEK_SET);

    files = (dpackfile_t*)malloc(header.dirlen);
    fread(files, 1, header.dirlen, f);

    for (i = 0; i < pak->numfiles; i++)
    {
        Q_strncpy(pak->files[i].name, files[i].name, sizeof(pak->files[i].name));
        pak->files[i].filepos = files[i].filepos;
        pak->files[i].filelen = files[i].filelen;
    }

    free(files);
    Con_Printf("Loaded %s (%d files)\n", filename, pak->numfiles);
    return pak;
}

int PAK_GetNumPaks(void) { return numpaks; }

byte* PAK_LoadFile(const char* path, int* len)
{
    if (len) *len = 0;
    if (!path) return NULL;

    char npath[256];
    NormalizePath(npath, path, sizeof(npath));

    for (int p = numpaks - 1; p >= 0; --p)
    {
        pack_t* pak = paks[p];
        if (!pak) continue;

        for (int i = 0; i < pak->numfiles; i++)
        {
            if (!Q_stricmp(pak->files[i].name, npath))
            {
                fseek(pak->handle, pak->files[i].filepos, SEEK_SET);
                byte* buf = (byte*)malloc(pak->files[i].filelen + 1);
                fread(buf, 1, pak->files[i].filelen, pak->handle);
                buf[pak->files[i].filelen] = 0;
                if (len) *len = pak->files[i].filelen;
                return buf;
            }
        }
    }

    return NULL;
}

int PAK_EnumFiles(void (*cb)(const char* name, int filelen, void* user), void* user)
{
    int p, i, count = 0;
    if (!cb) return 0;

    for (p = 0; p < numpaks; p++)
    {
        pack_t* pak = paks[p];
        if (!pak) continue;

        for (i = 0; i < pak->numfiles; i++)
        {
            cb(pak->files[i].name, pak->files[i].filelen, user);
            count++;
        }
    }
    return count;
}
