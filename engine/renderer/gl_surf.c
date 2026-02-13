// renderer/gl_surf.c
#include "quakedef.h"
#include "gl_local.h"
#include <bsp.h>

int gl_fullbright = 0;

// =============================================================================
// Рисование одной поверхности (two-pass: base + lightmap)
// =============================================================================

void R_DrawTexturedPoly(msurface_t* surf)
{
    glpoly_t* p = surf->polys;
    float* v;
    int i;

    if (!p)
        return;

    if (!surf->texinfo || !surf->texinfo->texture)
        return;

    // ===== Base texture (fullbright) =====
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, surf->texinfo->texture->gl_texturenum);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_POLYGON);
    for (i = 0; i < p->numverts; i++)
    {
        v = p->verts[i];
        glTexCoord2f(v[3], v[4]);
        glVertex3fv(v);
    }
    glEnd();
}
// =============================================================================
// Рисование sky-поверхности (пока заглушка)
// =============================================================================

void R_DrawSkyBackground(msurface_t* surf)
{
    glpoly_t* p = surf->polys;
    float* v;
    int i;

    if (!p)
        return;

    // Пока просто синий цвет
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.3f, 0.5f, 0.8f);

    glBegin(GL_POLYGON);
    for (i = 0; i < p->numverts; i++)
    {
        v = p->verts[i];
        glVertex3fv(v);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
}

// =============================================================================
// Рисование water/lava/slime (пока заглушка)
// =============================================================================

void R_DrawWaterPoly(msurface_t* surf)
{
    glpoly_t* p = surf->polys;
    float* v;
    int i;

    if (!p)
        return;

    // Текстура воды без лайтмапа
    if (surf->texinfo && surf->texinfo->texture)
        glBindTexture(GL_TEXTURE_2D, surf->texinfo->texture->gl_texturenum);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 1, 0.5f);  // полупрозрачность

    glBegin(GL_POLYGON);
    for (i = 0; i < p->numverts; i++)
    {
        v = p->verts[i];
        glTexCoord2f(v[3], v[4]);
        glVertex3fv(v);
    }
    glEnd();

    glDisable(GL_BLEND);
    glColor3f(1, 1, 1);
}
