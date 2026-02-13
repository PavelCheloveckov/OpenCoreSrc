// renderer/gl_lightmap.c
#include "quakedef.h"
#include "gl_local.h"
#include <bsp.h>

void GL_CreateLightmaps(model_t* model)
{
    int i, j;

    for (i = 0; i < model->numsurfaces; i++)
    {
        msurface_t* surf = &model->surfaces[i];

        // Пропускаем sky/water и поверхности без света
        if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
        {
            surf->lightmaptexturenum = 0;
            continue;
        }

        if (!surf->samples)
        {
            surf->lightmaptexturenum = 0;
            continue;
        }

        // Размер лайтмапа в texels
        int smax = (surf->extents[0] >> 4) + 1;
        int tmax = (surf->extents[1] >> 4) + 1;
        int size = smax * tmax;

        // Конвертируем grayscale -> RGBA
        byte* rgba = (byte*)Mem_Alloc(size * 4);

        for (j = 0; j < size; j++)
        {
            byte lum = surf->samples[j];

            // Overbright: умножаем на 2 для яркости
            int val = lum * 2;
            if (val > 255) val = 255;

            rgba[j * 4 + 0] = (byte)val;
            rgba[j * 4 + 1] = (byte)val;
            rgba[j * 4 + 2] = (byte)val;
            rgba[j * 4 + 3] = 255;
        }

        // Создаём GL текстуру
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
            smax, tmax, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, rgba);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        surf->lightmaptexturenum = tex;

        Mem_Free(rgba);
    }

    Con_Printf("Created lightmaps for %d surfaces\n", model->numsurfaces);
}

void GL_FreeLightmaps(model_t* model)
{
    int i;

    for (i = 0; i < model->numsurfaces; i++)
    {
        msurface_t* surf = &model->surfaces[i];

        if (surf->lightmaptexturenum)
        {
            glDeleteTextures(1, &surf->lightmaptexturenum);
            surf->lightmaptexturenum = 0;
        }
    }
}
