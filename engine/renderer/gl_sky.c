// gl_sky.c
#include "gl_local.h"
#include "common.h"
#include "console.h"
#include "mathlib.h"
#include <client.h>

static int sky_solid_tex = 0;
static int sky_alpha_tex = 0;
static qboolean sky_inited = FALSE;

void R_InitSkyFromRGBA(const byte* rgba, int w, int h)
{
    if (sky_inited) return;
    if (!rgba) return;
    if (w != 256 || h != 128) return;

    byte* solid = (byte*)Mem_Alloc(128 * 128 * 4);
    byte* alpha = (byte*)Mem_Alloc(128 * 128 * 4);

    for (int y = 0; y < 128; y++)
    {
        for (int x = 0; x < 128; x++)
        {
            const byte* srcA = &rgba[(y * 256 + x) * 4];
            const byte* srcS = &rgba[(y * 256 + (x + 128)) * 4];

            byte* dstA = &alpha[(y * 128 + x) * 4];
            byte* dstS = &solid[(y * 128 + x) * 4];

            dstA[0] = srcA[0]; dstA[1] = srcA[1]; dstA[2] = srcA[2]; dstA[3] = srcA[3];
            dstS[0] = srcS[0]; dstS[1] = srcS[1]; dstS[2] = srcS[2]; dstS[3] = 255;
        }
    }

    sky_solid_tex = GL_LoadTexture("sky_solid", solid, 128, 128, 0);
    sky_alpha_tex = GL_LoadTexture("sky_alpha", alpha, 128, 128, TF_ALPHA);

    Mem_Free(solid);
    Mem_Free(alpha);

    sky_inited = TRUE;
}

static void SkyTexCoord(const float* pos, float speed, float* s, float* t)
{
    vec3_t dir;
    dir[0] = pos[0] - r_refdef.vieworg[0];
    dir[1] = pos[1] - r_refdef.vieworg[1];
    dir[2] = pos[2] - r_refdef.vieworg[2];

    dir[2] *= 3.0f;

    float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1.0f) len = 1.0f;

    float scale = (6.0f * 63.0f) / len;
    float skytime = (float)(cl.time * speed);

    *s = (skytime + dir[0] * scale) * (1.0f / 128.0f);
    *t = (skytime + dir[1] * scale) * (1.0f / 128.0f);
}

void R_DrawSkyPoly(msurface_t* surf)
{
    if (!sky_inited || !surf || !surf->polys)
        return;

    glpoly_t* p;
    float* v;

    // задний слой
    glDisable(GL_BLEND);
    GL_Bind(sky_solid_tex);

    for (p = surf->polys; p; p = p->next)
    {
        glBegin(GL_POLYGON);
        for (int i = 0; i < p->numverts; i++)
        {
            v = p->verts[i];

            float s, t;
            SkyTexCoord(v, 8.0f, &s, &t);

            glTexCoord2f(s, t);
            glVertex3fv(v);
        }
        glEnd();
    }

    // передний слой
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GL_Bind(sky_alpha_tex);

    for (p = surf->polys; p; p = p->next)
    {
        glBegin(GL_POLYGON);
        for (int i = 0; i < p->numverts; i++)
        {
            v = p->verts[i];

            float s, t;
            SkyTexCoord(v, 16.0f, &s, &t);

            glTexCoord2f(s, t);
            glVertex3fv(v);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
}
