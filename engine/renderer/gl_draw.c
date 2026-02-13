#include "quakedef.h"
#include "gl_local.h"
#include "console.h"
#include "common.h"
#include <keys.h>
#include <client.h>

extern int GL_LoadFontImage(const char* filename);
int char_texture = 0;
int tex_conback = 0;  // Фон консоли
int tex_logo = 0;     // Логотип Quake (qplaque)
int logo_w, logo_h;
byte host_basepal[768]; // RGB * 256

// === ЗАГРУЗКА ===

// Функция для загрузки LMP (картинок Quake)
int Draw_LoadLump(const char* name)
{
    byte* filedata;
    int filelen;
    int width, height;
    byte* pixels;
    byte* rgba;
    int texnum;
    int i;


    // Загружаем файл
    filedata = COM_LoadFile(name, &filelen);
    if (!filedata)
    {
        Con_Printf("Draw_LoadLump: Could not load %s\n", name);
        return 0;
    }

    // Определяем формат (RAW или LMP с заголовком)
    if (filelen == 64000) // 320x200 (стандартный conback без заголовка)
    {
        width = 320;
        height = 200;
        pixels = filedata; // Данные начинаются сразу
        Con_Printf("Loaded RAW lump %s (%dx%d)\n", name, width, height);
    }
    else
    {
        // Читаем заголовок (Little Endian)
        width = LittleLong(((int*)filedata)[0]);
        height = LittleLong(((int*)filedata)[1]);
        pixels = filedata + 8; // Пропускаем 8 байт заголовка

        // Проверка на адекватность
        if (width <= 0 || width > 4096 || height <= 0 || height > 4096)
        {
            Con_Printf("Draw_LoadLump: Bad dimensions %dx%d in %s\n", width, height, name);
            free(filedata);
            return 0;
        }
        Con_Printf("Loaded LMP %s (%dx%d)\n", name, width, height);
    }

    // Конвертация 8-bit Indexed -> 32-bit RGBA
    // (Пока без палитры: делаем оттенки коричневого)
    rgba = (byte*)malloc(width * height * 4);
    if (!rgba)
    {
        free(filedata);
        return 0;
    }

    for (i = 0; i < width * height; i++)
    {
        byte idx = pixels[i];

        if (idx == 255) {
            rgba[i * 4 + 0] = 0; rgba[i * 4 + 1] = 0; rgba[i * 4 + 2] = 0; rgba[i * 4 + 3] = 0;
        }
        else {
            // Берем цвет из палитры!
            rgba[i * 4 + 0] = host_basepal[idx * 3 + 0]; // R
            rgba[i * 4 + 1] = host_basepal[idx * 3 + 1]; // G
            rgba[i * 4 + 2] = host_basepal[idx * 3 + 2]; // B
            rgba[i * 4 + 3] = 255;
        }
    }

    // Загрузка в OpenGL
    // TF_CLAMP чтобы края не повторялись
    texnum = GL_LoadTexture(name, rgba, width, height, TF_NOMIPMAP | TF_ALPHA | TF_CLAMP);

    // Чистка
    free(rgba);
    // free(filedata);
    // Но обычно free() работает, если COM_LoadFile делает malloc()

    return texnum;
}

void Draw_Pic(int x, int y, int w, int h, int texnum)
{
    GL_Bind(texnum);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(x, y);
    glTexCoord2f(1, 0); glVertex2i(x + w, y);
    glTexCoord2f(1, 1); glVertex2i(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2i(x, y + h);
    glEnd();
}

void Draw_Init(void)
{
    int len;

    // Загрузка палитры
    byte* pal = COM_LoadFile("gfx/palette.lmp", &len);
    if (pal && len >= 768) {
        memcpy(host_basepal, pal, 768);
        Con_Printf("Palette loaded\n");
    }
    else {
        // Генерируем серую палитру, если файла нет
        for (int i = 0; i < 256; i++) {
            host_basepal[i * 3 + 0] = i;
            host_basepal[i * 3 + 1] = i;
            host_basepal[i * 3 + 2] = i;
        }
    }
    if (pal) free(pal); // или COM_FreeFile

    char_texture = GL_LoadFontImage("gfx/conchars.png");
    Con_Printf("FONT TEX ID = %d\n", char_texture);
    tex_conback = Draw_LoadLump("gfx/conback.lmp");
    tex_logo = Draw_LoadLump("gfx/qplaque.lmp");

    if (!char_texture)
    {
        Con_Printf("Error loading font!\n");
    }
}

// === РИСОВАНИЕ ===

void GL_Set2D(void)
{
    glViewport(0, 0, glstate.width, glstate.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, glstate.width, glstate.height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void Draw_Menu(void)
{
    // Пока пусто
}

void Draw_Fill(int x, int y, int w, int h, int r, int g, int b, int a)
{
    glDisable(GL_TEXTURE_2D);
    glColor4ub(r, g, b, a);

    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();

    glColor4f(1, 1, 1, 1);
    glEnable(GL_TEXTURE_2D);
}

void Draw_Character(int x, int y, int num, int scale)
{
    float row, col;
    float size = 0.0625f; // 1/16

    num &= 255;

    if (num == ' ' || !char_texture)
        return;

    row = (num >> 4) * size;
    col = (num & 15) * size;

    GL_Bind(char_texture);

    // Размер на экране (8 * scale)
    int s = 8 * scale;

    glBegin(GL_QUADS);
    glTexCoord2f(col, row);
    glVertex2i(x, y);

    glTexCoord2f(col + size, row);
    glVertex2i(x + s, y);

    glTexCoord2f(col + size, row + size);
    glVertex2i(x + s, y + s);

    glTexCoord2f(col, row + size);
    glVertex2i(x, y + s);
    glEnd();
}

void Draw_String(int x, int y, const char* str, int scale)
{
    while (*str)
    {
        Draw_Character(x, y, *str, scale);
        str++;
        x += 8 * scale;
    }
}

// === ОТРИСОВКА КОНСОЛИ ===

void Con_Draw(void)
{
    // --- Анимация ---
    static double last_time = 0;
    double time = Sys_FloatTime();
    float dt = (float)(time - last_time);
    last_time = time;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.001f) dt = 0.001f;

    static float current_height = 0.0f;
    float max_height = glstate.height / 2.0f;
    float speed_px_sec = 600.0f;
    float target = (key_dest == key_console) ? max_height : 0.0f;

    float step = speed_px_sec * dt;
    if (current_height < target) { current_height += step; if (current_height > target) current_height = target; }
    else if (current_height > target) { current_height -= step; if (current_height < target) current_height = target; }

    if (current_height <= 0.0f) return;
    int height = (int)current_height;

    // ══════════════════════════════════════════
    // ПОЛНАЯ ИЗОЛЯЦИЯ
    // ══════════════════════════════════════════
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    glViewport(0, 0, glstate.width, glstate.height);

    if (glActiveTexture)
    {
        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE2);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ═══ ВОТ ЭТО КЛЮЧЕВОЕ ═══
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // Принудительный сброс кеша текстур -
    // привязываем НАПРЯМУЮ, минуя GL_Bind()
    glBindTexture(GL_TEXTURE_2D, 0);

    glDepthMask(GL_TRUE);
    glColor4f(1, 1, 1, 1);

    // 2D матрицы
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, glstate.width, glstate.height, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // === ФОН ===
    if (tex_conback)
    {
        // НАПРЯМУЮ привязываем, не через GL_Bind!
        glBindTexture(GL_TEXTURE_2D, tex_conback);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(0, 0);
        glTexCoord2f(1, 0); glVertex2i(glstate.width, 0);
        glTexCoord2f(1, 1); glVertex2i(glstate.width, height);
        glTexCoord2f(0, 1); glVertex2i(0, height);
        glEnd();
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
        glColor4f(0.3f, 0.3f, 0.3f, 0.9f);
        glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(glstate.width, 0);
        glVertex2i(glstate.width, height);
        glVertex2i(0, height);
        glEnd();
        glEnable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, 1);
    }

    // === ТЕКСТ ===
    int scale = 2;
    int char_size = 8 * scale;
    int rows = height / char_size;
    int y = height - char_size;

    // НАПРЯМУЮ привязываем шрифт!
    glBindTexture(GL_TEXTURE_2D, char_texture);
    glColor4f(1, 1, 1, 1);

    Draw_Character(8, y, ']', scale);

    char* input = &key_lines[edit_line][1];
    int len = (int)strlen(input);
    for (int i = 0; i < len; i++)
        Draw_Character(16 + i * char_size, y, input[i], scale);

    if (((int)(Sys_FloatTime() * 4)) & 1)
        Draw_Character(16 + len * char_size, y, 11, scale);

    for (int i = 0; i < rows; i++)
    {
        int line_idx = (con.current - i + con.totallines) % con.totallines;
        char* text = &con.text[line_idx * con.linewidth];
        int hist_y = y - ((i + 1) * char_size);
        if (hist_y < -char_size) break;

        if (text[0])
        {
            for (int k = 0; k < con.linewidth; k++)
            {
                if (text[k] > 32)
                    Draw_Character(8 + k * char_size, hist_y, text[k], scale);
            }
        }
    }

    // ══════════════════════════════════════════
    // ВОССТАНОВЛЕНИЕ
    // ══════════════════════════════════════════
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glPopAttrib();
}

void Draw_Background(void)
{
    // Очистка экрана
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Если текстура фона не загружена - выходим (останется черный экран)
    if (!tex_conback)
        return;

    // Включаем 2D режим
    GL_Set2D();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Включаем текстуры
    glEnable(GL_TEXTURE_2D);
    GL_Bind(tex_conback);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // Белый (оригинальный) цвет

    // Рисуем на весь экран
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(0, 0);
    glTexCoord2f(1, 0); glVertex2i(glstate.width, 0);
    glTexCoord2f(1, 1); glVertex2i(glstate.width, glstate.height);
    glTexCoord2f(0, 1); glVertex2i(0, glstate.height);
    glEnd();

    // Можно нарисовать логотип по центру
    if (tex_logo)
    {
        // Оригинальные размеры qplaque
        int w = 32;
        int h = 144;

        // Увеличим в 2 раза, чтобы было видно
        w *= 2; h *= 2;

        int x = 16; // Слева
        int y = 16; // Сверху

        // Или по центру:
        // int x = (glstate.width - w) / 2;
        // int y = (glstate.height - h) / 2;

        glEnable(GL_BLEND);
        Draw_Pic(x, y, w, h, tex_logo);
        glDisable(GL_BLEND);
    }
}

float scr_load_progress = 0.0f;
char scr_load_message[256] = "";

void SCR_UpdateLoading(const char* message, float progress)
{
    // Обновляем состояние
    strcpy(scr_load_message, message);
    scr_load_progress = progress;

    // == РИСУЕМ ПРЯМО СЕЙЧАС (Swap Buffers) ==
    // Это важно, так как игровой цикл стоит!

    glClear(GL_COLOR_BUFFER_BIT);

    // 1. Фон (conback или просто темный)
    GL_Set2D();
    glDisable(GL_DEPTH_TEST);

    if (tex_conback) {
        glEnable(GL_TEXTURE_2D);
        GL_Bind(tex_conback);
        glColor4f(0.5f, 0.5f, 0.5f, 1.0f); // Затемненный фон
        Draw_Pic(0, 0, glstate.width, glstate.height, tex_conback);
    }

    // 2. Надпись "Loading..." по центру
    glEnable(GL_TEXTURE_2D);
    GL_Bind(char_texture);
    glColor4f(1, 1, 1, 1);

    const char* title = "LOADING...";
    Draw_String((glstate.width - strlen(title) * 16) / 2, glstate.height / 2 - 20, title, 2);

    // 3. Текущее сообщение (например "Loading vertices...")
    if (message)
        Draw_String((glstate.width - strlen(message) * 8) / 2, glstate.height / 2 + 10, message, 1);

    // 4. Полоса прогресса (Bar)
    int bar_w = 300;
    int bar_h = 10;
    int bar_x = (glstate.width - bar_w) / 2;
    int bar_y = glstate.height / 2 + 30;

    glDisable(GL_TEXTURE_2D);
    // Рамка
    glColor4f(0.3f, 0.3f, 0.3f, 1);
    Draw_Fill(bar_x - 2, bar_y - 2, bar_w + 4, bar_h + 4, 50, 50, 50, 255);
    // Заполнение
    int fill_w = (int)(bar_w * progress);
    Draw_Fill(bar_x, bar_y, fill_w, bar_h, 200, 100, 0, 255); // Оранжевый

    // ОБМЕН БУФЕРОВ (Системный вызов)
    // Важно вызвать это вручную, так как R_EndFrame не работает здесь
    VID_Swap();
}
