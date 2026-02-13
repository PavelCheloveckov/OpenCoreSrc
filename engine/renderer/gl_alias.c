// gl_alias.c

#include "quakedef.h"
#include "mdl.h"
#include "gl_local.h"

void R_DrawAliasModel(aliasmodel_t* mod, vec3_t origin, vec3_t angles, int frame, int skin)
{
    int i;
    dtriangle_t* tri;
    trivertx_t* verts;
    float s, t;
    vec3_t v;

    if (!mod) return;

    // Ограничиваем frame
    if (frame < 0 || frame >= mod->numframes)
        frame = 0;

    if (skin < 0 || skin >= mod->numskins)
        skin = 0;

    verts = mod->frames[frame];

    glPushMatrix();

    // Позиция и поворот
    glTranslatef(origin[0], origin[1], origin[2]);
    glRotatef(angles[1], 0, 0, 1);   // yaw
    glRotatef(-angles[0], 0, 1, 0);  // pitch
    glRotatef(angles[2], 1, 0, 0);   // roll

    // Текстура
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, mod->skin_textures[skin]);
    glColor3f(1, 1, 1);

    // Рисуем треугольники
    glBegin(GL_TRIANGLES);

    for (i = 0; i < mod->numtris; i++)
    {
        tri = &mod->triangles[i];

        for (int j = 0; j < 3; j++)
        {
            int vi = tri->vertindex[j];
            trivertx_t* vert = &verts[vi];
            stvert_t* st = &mod->texcoords[vi];

            // Текстурные координаты
            s = (float)st->s / (float)mod->skinwidth;
            t = (float)st->t / (float)mod->skinheight;

            // Если на шве и задняя сторона - смещаем
            if (st->onseam && !tri->facesfront)
                s += 0.5f;

            // Позиция вершины
            v[0] = vert->v[0] * mod->scale[0] + mod->scale_origin[0];
            v[1] = vert->v[1] * mod->scale[1] + mod->scale_origin[1];
            v[2] = vert->v[2] * mod->scale[2] + mod->scale_origin[2];

            glTexCoord2f(s, t);
            glVertex3fv(v);
        }
    }

    glEnd();
    glPopMatrix();
}
