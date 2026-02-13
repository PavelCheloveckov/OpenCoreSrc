// tools/png2wad.c
// Конвертер PNG → WAD3
// Компиляция: cl png2wad.c /Fe:png2wad.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Использование stb_image для загрузки PNG (header-only библиотека)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#pragma pack(push, 1)
typedef struct {
    int identification;
    int numlumps;
    int infotableofs;
} wadheader_t;

typedef struct {
    int filepos;
    int disksize;
    int size;
    char type;
    char compression;
    char pad1, pad2;
    char name[16];
} lumpinfo_t;

typedef struct {
    char name[16];
    unsigned int width;
    unsigned int height;
    unsigned int offsets[4];
} miptex_t;
#pragma pack(pop)

#define WAD3_ID (('W') | ('A'<<8) | ('D'<<16) | ('3'<<24))
#define TYP_MIPTEX 67

// Простая квантизация цветов (можно улучшить)
void quantize_image(byte* rgba, int width, int height,
    byte* indexed, byte* palette, int* pal_count)
{
    int i, j, pixel_count = width * height;
    int colors_used = 0;

    // Простой алгоритм: первые 256 уникальных цветов
    for (i = 0; i < pixel_count; i++)
    {
        byte r = rgba[i * 4 + 0];
        byte g = rgba[i * 4 + 1];
        byte b = rgba[i * 4 + 2];
        byte a = rgba[i * 4 + 3];

        // Прозрачные пиксели → индекс 255
        if (a < 128)
        {
            indexed[i] = 255;
            continue;
        }

        // Поиск цвета в палитре
        int found = -1;
        for (j = 0; j < colors_used; j++)
        {
            if (palette[j * 3 + 0] == r &&
                palette[j * 3 + 1] == g &&
                palette[j * 3 + 2] == b)
            {
                found = j;
                break;
            }
        }

        if (found >= 0)
        {
            indexed[i] = found;
        }
        else if (colors_used < 255)
        {
            palette[colors_used * 3 + 0] = r;
            palette[colors_used * 3 + 1] = g;
            palette[colors_used * 3 + 2] = b;
            indexed[i] = colors_used;
            colors_used++;
        }
        else
        {
            // Слишком много цветов - ищем ближайший
            int best = 0;
            int best_dist = 999999;
            for (j = 0; j < 255; j++)
            {
                int dr = (int)palette[j * 3 + 0] - r;
                int dg = (int)palette[j * 3 + 1] - g;
                int db = (int)palette[j * 3 + 2] - b;
                int dist = dr * dr + dg * dg + db * db;
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best = j;
                }
            }
            indexed[i] = best;
        }
    }

    // Индекс 255 = прозрачный (синий)
    palette[255 * 3 + 0] = 0;
    palette[255 * 3 + 1] = 0;
    palette[255 * 3 + 2] = 255;

    *pal_count = 256;
}

// Генерация mipmap (простое усреднение 2x2)
void generate_mipmap(byte* src, int src_w, int src_h, byte* dst)
{
    int x, y;
    int dst_w = src_w / 2;
    int dst_h = src_h / 2;

    for (y = 0; y < dst_h; y++)
    {
        for (x = 0; x < dst_w; x++)
        {
            // Берём 4 пикселя, выбираем самый частый
            byte p0 = src[(y * 2 + 0) * src_w + (x * 2 + 0)];
            byte p1 = src[(y * 2 + 0) * src_w + (x * 2 + 1)];
            byte p2 = src[(y * 2 + 1) * src_w + (x * 2 + 0)];
            byte p3 = src[(y * 2 + 1) * src_w + (x * 2 + 1)];

            // Простой выбор - первый непрозрачный или p0
            if (p0 != 255) dst[y * dst_w + x] = p0;
            else if (p1 != 255) dst[y * dst_w + x] = p1;
            else if (p2 != 255) dst[y * dst_w + x] = p2;
            else if (p3 != 255) dst[y * dst_w + x] = p3;
            else dst[y * dst_w + x] = 255;
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Usage: png2wad output.wad input1.png [input2.png ...]\n");
        printf("\nExample:\n");
        printf("  png2wad mywad.wad wall01.png floor01.png door01.png\n");
        return 1;
    }

    char* output_file = argv[1];
    int num_textures = argc - 2;

    FILE* out = fopen(output_file, "wb");
    if (!out)
    {
        printf("Cannot create %s\n", output_file);
        return 1;
    }

    // Резервируем место под заголовок
    wadheader_t header = { 0 };
    header.identification = WAD3_ID;
    header.numlumps = num_textures;
    fwrite(&header, sizeof(header), 1, out);

    lumpinfo_t* lumps = calloc(num_textures, sizeof(lumpinfo_t));

    for (int i = 0; i < num_textures; i++)
    {
        char* input_file = argv[i + 2];

        // Загрузка PNG
        int width, height, channels;
        byte* rgba = stbi_load(input_file, &width, &height, &channels, 4);

        if (!rgba)
        {
            printf("Cannot load %s\n", input_file);
            continue;
        }

        // Проверка размеров
        if (width % 16 != 0 || height % 16 != 0)
        {
            printf("Warning: %s size (%dx%d) not multiple of 16\n",
                input_file, width, height);
        }

        printf("Processing %s (%dx%d)...\n", input_file, width, height);

        // Извлекаем имя текстуры из файла
        char tex_name[16] = { 0 };
        char* base = strrchr(input_file, '/');
        if (!base) base = strrchr(input_file, '\\');
        if (!base) base = input_file - 1;
        base++;

        strncpy(tex_name, base, 15);
        char* dot = strrchr(tex_name, '.');
        if (dot) *dot = 0;

        // Конвертация в indexed
        int mip0_size = width * height;
        int mip1_size = (width / 2) * (height / 2);
        int mip2_size = (width / 4) * (height / 4);
        int mip3_size = (width / 8) * (height / 8);
        int total_pixels = mip0_size + mip1_size + mip2_size + mip3_size;

        byte* indexed = malloc(total_pixels);
        byte palette[768];
        int pal_count;

        quantize_image(rgba, width, height, indexed, palette, &pal_count);

        // Генерация mipmap
        byte* mip0 = indexed;
        byte* mip1 = indexed + mip0_size;
        byte* mip2 = mip1 + mip1_size;
        byte* mip3 = mip2 + mip2_size;

        generate_mipmap(mip0, width, height, mip1);
        generate_mipmap(mip1, width / 2, height / 2, mip2);
        generate_mipmap(mip2, width / 4, height / 4, mip3);

        // Запись в WAD
        lumps[i].filepos = ftell(out);
        lumps[i].type = TYP_MIPTEX;
        lumps[i].compression = 0;
        strncpy(lumps[i].name, tex_name, 16);

        // Заголовок miptex
        miptex_t miptex = { 0 };
        strncpy(miptex.name, tex_name, 16);
        miptex.width = width;
        miptex.height = height;
        miptex.offsets[0] = sizeof(miptex_t);
        miptex.offsets[1] = miptex.offsets[0] + mip0_size;
        miptex.offsets[2] = miptex.offsets[1] + mip1_size;
        miptex.offsets[3] = miptex.offsets[2] + mip2_size;

        fwrite(&miptex, sizeof(miptex), 1, out);
        fwrite(indexed, 1, total_pixels, out);

        // Палитра
        unsigned short pal_size = 256;
        fwrite(&pal_size, 2, 1, out);
        fwrite(palette, 1, 768, out);

        lumps[i].disksize = ftell(out) - lumps[i].filepos;
        lumps[i].size = lumps[i].disksize;

        stbi_image_free(rgba);
        free(indexed);
    }

    // Записываем таблицу
    header.infotableofs = ftell(out);
    fwrite(lumps, sizeof(lumpinfo_t), num_textures, out);

    // Обновляем заголовок
    fseek(out, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, out);

    fclose(out);
    free(lumps);

    printf("Created %s with %d textures\n", output_file, num_textures);
    return 0;
}
