#define STB_IMAGE_IMPLEMENTATION
#include "../tools/stb_image.h"
#include "quakedef.h"
#include "gl_local.h"

// Функция загрузки изображения с диска (PNG/JPG/TGA/BMP)
int GL_LoadImage(const char* filename)
{
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4); // Принудительно 4 канала (RGBA)

    if (!data)
    {
        Con_Printf("Failed to load image: %s\n", filename);
        return 0;
    }

    int texnum = GL_LoadTexture(filename, data, width, height, TF_NOMIPMAP | TF_ALPHA);

    stbi_image_free(data);

    return texnum;
}

// Новая функция специально для шрифта
int GL_LoadFontImage(const char* filename)
{
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);

    if (!data) return 0;

    /* for (int i = 0; i < width * height; i++)
    {
        // Если пиксель черный
        if (data[i * 4 + 0] < 10 && data[i * 4 + 1] < 10 && data[i * 4 + 2] < 10)
        {
            data[i * 4 + 3] = 0; // Прозрачный
        }
        else
        {
            data[i * 4 + 0] = 255;
            data[i * 4 + 1] = 255;
            data[i * 4 + 2] = 255;
            data[i * 4 + 3] = 255; //
        }
    } */
    // ==================

    int tex = GL_LoadTexture("font", data, width, height, TF_NOMIPMAP | TF_ALPHA);
    stbi_image_free(data);
    return tex;
}
