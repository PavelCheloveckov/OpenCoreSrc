// filesystem/wad.h
#ifndef WAD_H
#define WAD_H

// WAD3 - формат Half-Life
#define WAD3_ID         (('W') | ('A'<<8) | ('D'<<16) | ('3'<<24))

// Типы записей
#define TYP_QPIC    66      // Картинка
#define TYP_MIPTEX  67      // Текстура с mipmap
#define TYP_RAW     68      // Сырые данные

#pragma pack(push, 1)

// Заголовок WAD
typedef struct wadheader_s {
    int     identification;     // WAD3
    int     numlumps;          // Количество записей
    int     infotableofs;      // Смещение до таблицы
} wadheader_t;

// Запись в таблице
typedef struct lumpinfo_s {
    int     filepos;           // Позиция в файле
    int     disksize;          // Размер на диске
    int     size;              // Распакованный размер
    char    type;              // Тип (TYP_MIPTEX)
    char    compression;       // 0 = нет сжатия
    char    pad1, pad2;
    char    name[16];          // Имя текстуры
} lumpinfo_t;

// Текстура с mipmap
typedef struct miptex_s {
    char    name[16];
    unsigned int width;
    unsigned int height;
    unsigned int offsets[4];   // Смещения до 4 уровней mipmap
    // После структуры идут пиксели:
    // [mip0] [mip1] [mip2] [mip3] [palette_size: 2 bytes] [palette: 768 bytes]
} miptex_t;

#pragma pack(pop)

// Загруженная текстура (runtime)
typedef struct wad_texture_s {
    char    name[16];
    int     width, height;
    byte* data;
    unsigned int gl_texturenum;
} wad_texture_t;

// Функции
typedef struct wad_s wad_t;

wad_t* WAD_Open(const char* filename);
void WAD_Close(wad_t* wad);
int WAD_GetLumpCount(wad_t* wad);
lumpinfo_t* WAD_GetLumpInfo(wad_t* wad, int index);
lumpinfo_t* WAD_FindLump(wad_t* wad, const char* name);
byte* WAD_LoadLump(wad_t* wad, const char* name, int* size);
wad_texture_t* WAD_LoadTexture(wad_t* wad, const char* name);
void WAD_FreeTexture(wad_texture_t* tex);

#endif // WAD_H
