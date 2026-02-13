// engine/renderer/gl_texture.c
#include "gl_local.h"
#include "common.h"
#include "console.h"
#include <string.h>
#include <stdlib.h>

#define MAX_GLTEXTURES 4096

typedef struct gltexture_s {
    char name[MAX_QPATH];
    int texnum;
    int width, height;
    int flags;
    qboolean used;
} gltexture_t;

static gltexture_t gltextures[MAX_GLTEXTURES];
static int numgltextures = 0;
static int currenttexture = -1;

// ============================================================================
// Управление текстурами
// ============================================================================

void GL_Bind(int texnum)
{
    if (currenttexture == texnum)
        return;

    currenttexture = texnum;
    glBindTexture(GL_TEXTURE_2D, texnum);
}

void GL_SelectTexture(int unit)
{
    if (glActiveTexture)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
    }
}

static int GL_GenerateTexnum(void)
{
    GLuint texnum;
    glGenTextures(1, &texnum);
    return texnum;
}

int GL_LoadTexture(const char* name, byte* data, int width, int height, int flags)
{
    int i;
    gltexture_t* glt;
    GLuint texnum;

    // Поиск существующей
    for (i = 0; i < numgltextures; i++)
    {
        glt = &gltextures[i];
        if (!strcmp(glt->name, name))
        {
            return glt->texnum;
        }
    }

    // Новая текстура
    if (numgltextures >= MAX_GLTEXTURES)
    {
        Sys_Error("GL_LoadTexture: MAX_GLTEXTURES");
    }

    glt = &gltextures[numgltextures];
    numgltextures++;

    Q_strncpy(glt->name, name, sizeof(glt->name));
    glt->width = width;
    glt->height = height;
    glt->flags = flags;
    glt->used = TRUE;

    // Генерация OpenGL текстуры
    glGenTextures(1, &texnum);
    glt->texnum = texnum;

    GL_Bind(texnum);

    // Параметры
    if (flags & TF_NOMIPMAP)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (flags & TF_CLAMP)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Загрузка данных
    if (flags & TF_ALPHA)
    {
        if (flags & TF_NOMIPMAP)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        else
        {
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
    }
    else
    {
        if (flags & TF_NOMIPMAP)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        else
        {
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
    }

    Con_DPrintf("GL_LoadTexture: %s (%dx%d)\n", name, width, height);

    return texnum;
}

void GL_FreeTexture(int texnum)
{
    int i;
    GLuint tex = texnum;

    for (i = 0; i < numgltextures; i++)
    {
        if (gltextures[i].texnum == texnum)
        {
            gltextures[i].used = FALSE;
            gltextures[i].name[0] = 0;
            break;
        }
    }

    glDeleteTextures(1, &tex);

    if (currenttexture == texnum)
        currenttexture = -1;
}

void GL_FreeAllTextures(void)
{
    int i;

    for (i = 0; i < numgltextures; i++)
    {
        if (gltextures[i].used)
        {
            GLuint tex = gltextures[i].texnum;
            glDeleteTextures(1, &tex);
        }
    }

    numgltextures = 0;
    currenttexture = -1;
}
