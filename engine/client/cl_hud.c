// client/cl_hud.c
#include "quakedef.h"
#include "client.h"
#include "console.h"
#include "gl_local.h"
#include "server.h"

extern int char_texture;

static int fps_count = 0;
static float fps_next_update = 0;
static int fps_value = 0;

void HUD_CalcFPS(void)
{
    fps_count++;

    if (cls.realtime >= fps_next_update)
    {
        fps_value = fps_count;
        fps_count = 0;
        fps_next_update = cls.realtime + 1.0f;
    }
}

static void HUD_DrawChar(int x, int y, int num, float scale)
{
    float row, col;
    float size = 0.0625f;

    num &= 255;
    if (num == ' ' || !char_texture)
        return;

    row = (num >> 4) * size;
    col = (num & 15) * size;

    glBindTexture(GL_TEXTURE_2D, char_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int s = (int)(8 * scale);

    glBegin(GL_QUADS);
    glTexCoord2f(col, row);               glVertex2i(x, y);
    glTexCoord2f(col + size, row);         glVertex2i(x + s, y);
    glTexCoord2f(col + size, row + size);  glVertex2i(x + s, y + s);
    glTexCoord2f(col, row + size);         glVertex2i(x, y + s);
    glEnd();
}

static void HUD_DrawString(int x, int y, const char* str, float scale, int spacing)
{
    while (*str)
    {
        HUD_DrawChar(x, y, *str, scale);
        str++;
        x += spacing;
    }
}

void HUD_Draw(void)
{
    char str[128];
    int y = 8;
    float scale = 1.5f;
    int right = glstate.width;
    int padding = 8;
    int char_w = (int)(7 * scale);  // интервал между символами
    int line_h = (int)(12 * scale);

    HUD_CalcFPS();

    GL_Set2D();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, char_texture);
    glColor4f(1, 1, 1, 1);

    // FPS
    snprintf(str, sizeof(str), "FPS: %d", fps_value);
    HUD_DrawString(right - (int)strlen(str) * char_w - padding, y, str, scale, char_w);

    // Позиция
    y += line_h;
    snprintf(str, sizeof(str), "pos: %.1f %.1f %.1f",
        cl.vieworg[0], cl.vieworg[1], cl.vieworg[2]);
    HUD_DrawString(right - (int)strlen(str) * char_w - padding, y, str, scale, char_w);

    if (sv.active && sv.edicts)
    {
        edict_t* player = EDICT_NUM(1);
        float speed = sqrtf(
            player->v.velocity[0] * player->v.velocity[0] +
            player->v.velocity[1] * player->v.velocity[1]);

        y += line_h;
        snprintf(str, sizeof(str), "vel: %.0f", speed);
        HUD_DrawString(right - (int)strlen(str) * char_w - padding, y, str, scale, char_w);

        y += line_h;
        if (player->v.flags & FL_ONGROUND)
        {
            glColor4f(0, 1, 0, 1);
            snprintf(str, sizeof(str), "ground: yes");
        }
        else
        {
            glColor4f(1, 0.3f, 0.3f, 1);
            snprintf(str, sizeof(str), "ground: no");
        }
        HUD_DrawString(right - (int)strlen(str) * char_w - padding, y, str, scale, char_w);
        glColor4f(1, 1, 1, 1);

        y += line_h;
        snprintf(str, sizeof(str), "ang: %.1f %.1f",
            cl.viewangles[0], cl.viewangles[1]);
        HUD_DrawString(right - (int)strlen(str) * char_w - padding, y, str, scale, char_w);
    }

    // === Прицел ===
    int cx = glstate.width / 2;
    int cy = glstate.height / 2;
    int size = 12;
    int thickness = 2;

    glDisable(GL_TEXTURE_2D);

    glColor4f(0, 0, 0, 0.6f);
    glBegin(GL_QUADS);
    glVertex2i(cx - size + 1, cy - thickness / 2 + 1);
    glVertex2i(cx + size + 1, cy - thickness / 2 + 1);
    glVertex2i(cx + size + 1, cy + thickness / 2 + 1);
    glVertex2i(cx - size + 1, cy + thickness / 2 + 1);
    glVertex2i(cx - thickness / 2 + 1, cy - size + 1);
    glVertex2i(cx + thickness / 2 + 1, cy - size + 1);
    glVertex2i(cx + thickness / 2 + 1, cy + size + 1);
    glVertex2i(cx - thickness / 2 + 1, cy + size + 1);
    glEnd();

    glColor4f(1, 1, 1, 0.9f);
    glBegin(GL_QUADS);
    glVertex2i(cx - size, cy - thickness / 2);
    glVertex2i(cx + size, cy - thickness / 2);
    glVertex2i(cx + size, cy + thickness / 2);
    glVertex2i(cx - size, cy + thickness / 2);
    glVertex2i(cx - thickness / 2, cy - size);
    glVertex2i(cx + thickness / 2, cy - size);
    glVertex2i(cx + thickness / 2, cy + size);
    glVertex2i(cx - thickness / 2, cy + size);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
}
