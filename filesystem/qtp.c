// qtp.c
#include "qtp.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

// Должны совпадать с тем, что у тебя в qtpe_qtp.h
#define QTP_MAGIC   0x30505451  // "QTP0" little-endian
#define QTP_VERSION 0

typedef struct qtp_texture_s
{
    char name[64];
    int width;
    int height;
    int flags;
    unsigned char* pixels;  // RGBA
    GLuint gl_id;
} qtp_texture_t;

static int qtp_loaded = 0;
static int qtp_num_textures = 0;
static qtp_texture_t* qtp_textures = NULL;

// ===== Helpers =====

static int QTP_ReadInt(FILE* f, int* v)
{
    return fread(v, sizeof(int), 1, f) == 1;
}

static int QTP_ReadString64(FILE* f, char* s)
{
    if (fread(s, 1, 64, f) != 64)
        return 0;
    s[63] = 0;
    return 1;
}

// Загружаем textures/base.qtp один раз
static void QTP_LoadPackOnce(void)
{
    if (qtp_loaded)
        return;

    qtp_loaded = 1;

    const char* path = "textures/base.qtp";
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        // printf("QTP: could not open %s\n", path);
        return;
    }

    int magic = 0, version = 0, numTex = 0;
    if (!QTP_ReadInt(f, &magic) || !QTP_ReadInt(f, &version) || !QTP_ReadInt(f, &numTex))
    {
        fclose(f);
        return;
    }

    if (magic != QTP_MAGIC || version != QTP_VERSION || numTex <= 0)
    {
        fclose(f);
        return;
    }

    qtp_textures = (qtp_texture_t*)calloc(numTex, sizeof(qtp_texture_t));
    if (!qtp_textures)
    {
        fclose(f);
        return;
    }

    qtp_num_textures = 0;

    for (int i = 0; i < numTex; ++i)
    {
        qtp_texture_t* t = &qtp_textures[qtp_num_textures];

        if (!QTP_ReadString64(f, t->name))
            break;
        if (!QTP_ReadInt(f, &t->width) ||
            !QTP_ReadInt(f, &t->height) ||
            !QTP_ReadInt(f, &t->flags))
            break;

        if (t->width <= 0 || t->height <= 0)
            break;

        int pixelSize = t->width * t->height * 4;
        t->pixels = (unsigned char*)malloc(pixelSize);
        if (!t->pixels)
            break;

        if (fread(t->pixels, 1, pixelSize, f) != (size_t)pixelSize)
        {
            free(t->pixels);
            t->pixels = NULL;
            break;
        }

        t->gl_id = 0;
        qtp_num_textures++;
    }

    fclose(f);

    // Если ни одной текстуры не удалось корректно прочитать - очищаем
    if (qtp_num_textures == 0)
    {
        if (qtp_textures)
        {
            for (int i = 0; i < numTex; ++i)
            {
                if (qtp_textures[i].pixels)
                    free(qtp_textures[i].pixels);
            }
            free(qtp_textures);
            qtp_textures = NULL;
        }
    }
}

// Поиск по имени (case-insensitive, как QTPE_FindTexture)
static int QTP_FindTextureIndex(const char* name)
{
    if (!name || !qtp_textures)
        return -1;

    for (int i = 0; i < qtp_num_textures; ++i)
    {
#ifdef _WIN32
        if (_stricmp(qtp_textures[i].name, name) == 0)
            return i;
#else
        if (strcasecmp(qtp_textures[i].name, name) == 0)
            return i;
#endif
    }
    return -1;
}

// ===== Публичное API для движка =====

int QTP_LoadTexture(const char* name, int expectedWidth, int expectedHeight)
{
    (void)expectedWidth;
    (void)expectedHeight;

    QTP_LoadPackOnce();

    if (!qtp_textures || qtp_num_textures == 0)
        return 0;

    int idx = QTP_FindTextureIndex(name);
    if (idx < 0)
    {
        // printf("QTP_LoadTexture: '%s' not found in base.qtp\n", name);
        return 0;
    }

    qtp_texture_t* t = &qtp_textures[idx];

    if (t->gl_id != 0)
        return (int)t->gl_id;

    if (!t->pixels)
        return 0;

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // Базовые GL-параметры
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Создаём мипмапы из RGBA‑пикселей
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA,
        t->width, t->height,
        GL_RGBA, GL_UNSIGNED_BYTE,
        t->pixels);

    t->gl_id = id;

    // Если не нужны пиксели на CPU после загрузки - можно их освободить:
    // free(t->pixels);
    // t->pixels = NULL;

    return (int)id;
}
