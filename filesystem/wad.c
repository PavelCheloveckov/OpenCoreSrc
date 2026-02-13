// filesystem/wad.c
#include "quakedef.h"
#include <wad.h>

struct wad_s {
    FILE* file;
    wadheader_t header;
    lumpinfo_t* lumps;
};

wad_t* WAD_Open(const char* filename)
{
    wad_t* wad;
    FILE* f;

    f = fopen(filename, "rb");
    if (!f)
    {
        Con_Printf("WAD_Open: cannot open %s\n", filename);
        return NULL;
    }

    wad = (wad_t*)malloc(sizeof(wad_t));
    memset(wad, 0, sizeof(wad_t));
    wad->file = f;

    // Читаем заголовок
    fread(&wad->header, sizeof(wadheader_t), 1, f);

    // Проверяем сигнатуру
    if (wad->header.identification != WAD3_ID)
    {
        Con_Printf("WAD_Open: %s is not a WAD3 file\n", filename);
        fclose(f);
        free(wad);
        return NULL;
    }

    // Читаем таблицу записей
    wad->lumps = (lumpinfo_t*)malloc(sizeof(lumpinfo_t) * wad->header.numlumps);
    fseek(f, wad->header.infotableofs, SEEK_SET);
    fread(wad->lumps, sizeof(lumpinfo_t), wad->header.numlumps, f);

    Con_Printf("WAD_Open: loaded %s (%d lumps)\n", filename, wad->header.numlumps);

    return wad;
}

void WAD_Close(wad_t* wad)
{
    if (!wad)
        return;

    if (wad->file)
        fclose(wad->file);
    if (wad->lumps)
        free(wad->lumps);
    free(wad);
}

int WAD_GetLumpCount(wad_t* wad)
{
    return wad ? wad->header.numlumps : 0;
}

lumpinfo_t* WAD_GetLumpInfo(wad_t* wad, int index)
{
    if (!wad || index < 0 || index >= wad->header.numlumps)
        return NULL;
    return &wad->lumps[index];
}

lumpinfo_t* WAD_FindLump(wad_t* wad, const char* name)
{
    int i;

    if (!wad)
        return NULL;

    for (i = 0; i < wad->header.numlumps; i++)
    {
        if (Q_strnicmp(wad->lumps[i].name, name, 16) == 0)
            return &wad->lumps[i];
    }

    return NULL;
}

byte* WAD_LoadLump(wad_t* wad, const char* name, int* size)
{
    lumpinfo_t* lump;
    byte* data;

    lump = WAD_FindLump(wad, name);
    if (!lump)
    {
        if (size) *size = 0;
        return NULL;
    }

    data = (byte*)malloc(lump->disksize);
    fseek(wad->file, lump->filepos, SEEK_SET);
    fread(data, 1, lump->disksize, wad->file);

    if (size)
        *size = lump->disksize;

    return data;
}

// Загрузка текстуры с конвертацией в RGBA
wad_texture_t* WAD_LoadTexture(wad_t* wad, const char* name)
{
    lumpinfo_t* lump;
    miptex_t* mip;
    wad_texture_t* tex;
    byte* raw_data;
    byte* pixels;
    byte* palette;
    int i, pixel_count;
    int pal_offset;
    unsigned short pal_size;

    lump = WAD_FindLump(wad, name);
    if (!lump || lump->type != TYP_MIPTEX)
        return NULL;

    // Читаем сырые данные
    raw_data = (byte*)malloc(lump->disksize);
    fseek(wad->file, lump->filepos, SEEK_SET);
    fread(raw_data, 1, lump->disksize, wad->file);

    mip = (miptex_t*)raw_data;

    // Находим палитру (после всех mipmap уровней)
    pal_offset = mip->offsets[3] + (mip->width >> 3) * (mip->height >> 3);
    pal_size = *(unsigned short*)(raw_data + pal_offset);
    palette = raw_data + pal_offset + 2;

    // Создаём текстуру
    tex = (wad_texture_t*)malloc(sizeof(wad_texture_t));
    memset(tex, 0, sizeof(wad_texture_t));

    strncpy(tex->name, mip->name, 16);
    tex->width = mip->width;
    tex->height = mip->height;

    // Конвертация indexed → RGBA
    pixel_count = tex->width * tex->height;
    tex->data = (byte*)malloc(pixel_count * 4);
    pixels = raw_data + mip->offsets[0];  // mip level 0

    for (i = 0; i < pixel_count; i++)
    {
        byte index = pixels[i];
        byte* color = &palette[index * 3];

        tex->data[i * 4 + 0] = color[0];  // R
        tex->data[i * 4 + 1] = color[1];  // G
        tex->data[i * 4 + 2] = color[2];  // B

        // Прозрачность: индекс 255 или синий цвет {0,0,255} = прозрачный
        if (index == 255 || (color[0] == 0 && color[1] == 0 && color[2] == 255))
            tex->data[i * 4 + 3] = 0;     // A = 0 (прозрачный)
        else
            tex->data[i * 4 + 3] = 255;   // A = 255 (непрозрачный)
    }

    free(raw_data);

    Con_Printf("WAD_LoadTexture: %s (%dx%d)\n", tex->name, tex->width, tex->height);

    return tex;
}

void WAD_FreeTexture(wad_texture_t* tex)
{
    if (!tex)
        return;
    if (tex->data)
        free(tex->data);
    free(tex);
}
