// engine/sound/snd_main.c
// Простая звуковая система

#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "mathlib.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
#include <cvar.h>

// Максимумы
#define MAX_CHANNELS    32
#define MAX_SFX         512
#define MAX_SFX_HASH    256

// Звуковой файл
typedef struct sfx_s {
    char name[MAX_QPATH];
    int length;             // Длина в сэмплах
    int loopstart;          // -1 = не зацикленный
    int rate;               // Частота
    int width;              // 1 или 2 байта
    int channels;           // Моно/стерео
    byte* data;             // PCM данные
    struct sfx_s* hash_next;
} sfx_t;

// Канал воспроизведения
typedef struct channel_s {
    sfx_t* sfx;             // Что играет
    int entnum;             // Сущность-источник
    int entchannel;         // Канал сущности
    vec3_t origin;          // Позиция
    float dist_mult;        // Множитель расстояния
    float master_vol;       // Громкость
    int pos;                // Текущая позиция
    int end;                // Когда закончится
    float leftvol;          // Громкость левого
    float rightvol;         // Громкость правого
} channel_t;

// Состояние звуковой системы
static qboolean snd_initialized;
static channel_t channels[MAX_CHANNELS];
static int num_channels;

static sfx_t known_sfx[MAX_SFX];
static int num_sfx;
static sfx_t* sfx_hash[MAX_SFX_HASH];

static vec3_t listener_origin;
static vec3_t listener_forward;
static vec3_t listener_right;
static vec3_t listener_up;

// Cvars
static cvar_t* s_volume;
static cvar_t* s_musicvolume;
static cvar_t* s_khz;
static cvar_t* s_show;

// ============================================================================
// Инициализация
// ============================================================================

void S_Init(void)
{
    Con_Printf("S_Init\n");

    s_volume = Cvar_Get("volume", "0.7", FCVAR_ARCHIVE);
    s_musicvolume = Cvar_Get("musicvolume", "1.0", FCVAR_ARCHIVE);
    s_khz = Cvar_Get("s_khz", "22", FCVAR_ARCHIVE);
    s_show = Cvar_Get("s_show", "0", 0);

    memset(known_sfx, 0, sizeof(known_sfx));
    memset(sfx_hash, 0, sizeof(sfx_hash));
    memset(channels, 0, sizeof(channels));

    num_sfx = 0;
    num_channels = MAX_CHANNELS;

    // Инициализация аудио-бэкенда
#ifdef _WIN32
    // DirectSound или WaveOut инициализация
#endif

    snd_initialized = TRUE;
    Con_Printf("Sound initialized\n");
}

void S_Shutdown(void)
{
    int i;

    if (!snd_initialized)
        return;

    Con_Printf("S_Shutdown\n");

    S_StopAllSounds();

    // Освобождение звуков
    for (i = 0; i < num_sfx; i++)
    {
        if (known_sfx[i].data)
        {
            free(known_sfx[i].data);
            known_sfx[i].data = NULL;
        }
    }

    snd_initialized = FALSE;
}

// ============================================================================
// Загрузка звуков
// ============================================================================

static unsigned int S_HashName(const char* name)
{
    unsigned int hash = 0;
    while (*name)
    {
        hash = hash * 31 + tolower(*name);
        name++;
    }
    return hash % MAX_SFX_HASH;
}

static sfx_t* S_FindName(const char* name)
{
    unsigned int hash;
    sfx_t* sfx;

    hash = S_HashName(name);

    // Поиск в хеше
    for (sfx = sfx_hash[hash]; sfx; sfx = sfx->hash_next)
    {
        if (!Q_stricmp(sfx->name, name))
            return sfx;
    }

    // Создание нового
    if (num_sfx >= MAX_SFX)
    {
        Con_Printf("S_FindName: out of sfx slots\n");
        return NULL;
    }

    sfx = &known_sfx[num_sfx++];
    memset(sfx, 0, sizeof(*sfx));
    Q_strncpy(sfx->name, name, sizeof(sfx->name));
    sfx->loopstart = -1;

    // Добавление в хеш
    sfx->hash_next = sfx_hash[hash];
    sfx_hash[hash] = sfx;

    return sfx;
}

static qboolean S_LoadWAV(sfx_t* sfx)
{
    byte* data;
    int len;
    char path[MAX_QPATH];

    // Путь к файлу
    snprintf(path, sizeof(path), "sound/%s", sfx->name);

    data = COM_LoadFile(path, &len);
    if (!data)
    {
        Con_Printf("Couldn't load %s\n", path);
        return FALSE;
    }

    // Парсинг WAV (упрощённый)
    // RIFF header
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
    {
        Con_Printf("%s is not a WAV file\n", path);
        free(data);
        return FALSE;
    }

    // Поиск fmt chunk
    byte* ptr = data + 12;
    byte* end = data + len;

    int rate = 22050;
    int width = 2;
    int channels = 1;
    byte* pcm_data = NULL;
    int pcm_len = 0;

    while (ptr < end - 8)
    {
        int chunk_size = *(int*)(ptr + 4);

        if (memcmp(ptr, "fmt ", 4) == 0)
        {
            short format = *(short*)(ptr + 8);
            channels = *(short*)(ptr + 10);
            rate = *(int*)(ptr + 12);
            width = *(short*)(ptr + 22) / 8;
        }
        else if (memcmp(ptr, "data", 4) == 0)
        {
            pcm_data = ptr + 8;
            pcm_len = chunk_size;
        }

        ptr += 8 + chunk_size;
        if (chunk_size & 1) ptr++;  // Padding
    }

    if (!pcm_data)
    {
        Con_Printf("%s has no data chunk\n", path);
        free(data);
        return FALSE;
    }

    // Копирование данных
    sfx->data = (byte*)malloc(pcm_len);
    memcpy(sfx->data, pcm_data, pcm_len);
    sfx->length = pcm_len / (width * channels);
    sfx->rate = rate;
    sfx->width = width;
    sfx->channels = channels;

    free(data);

    Con_DPrintf("Loaded %s: %d samples, %d Hz\n", sfx->name, sfx->length, rate);

    return TRUE;
}

// ============================================================================
// Воспроизведение
// ============================================================================

void S_StartSound(vec3_t origin, int entnum, int entchannel,
    const char* name, float vol, float attn)
{
    sfx_t* sfx;
    channel_t* ch;
    int i;

    if (!snd_initialized)
        return;

    // Поиск/загрузка звука
    sfx = S_FindName(name);
    if (!sfx)
        return;

    if (!sfx->data)
    {
        if (!S_LoadWAV(sfx))
            return;
    }

    // Поиск канала для сущности или свободного
    ch = NULL;

    // Сначала ищем канал этой сущности
    for (i = 0; i < num_channels; i++)
    {
        if (channels[i].entnum == entnum &&
            channels[i].entchannel == entchannel)
        {
            ch = &channels[i];
            break;
        }
    }

    // Ищем свободный
    if (!ch)
    {
        for (i = 0; i < num_channels; i++)
        {
            if (!channels[i].sfx)
            {
                ch = &channels[i];
                break;
            }
        }
    }

    // Ищем самый тихий
    if (!ch)
    {
        float min_vol = 1.0f;
        for (i = 0; i < num_channels; i++)
        {
            if (channels[i].master_vol < min_vol)
            {
                min_vol = channels[i].master_vol;
                ch = &channels[i];
            }
        }
    }

    if (!ch)
        return;

    // Настройка канала
    ch->sfx = sfx;
    ch->entnum = entnum;
    ch->entchannel = entchannel;
    VectorCopy(origin, ch->origin);
    ch->dist_mult = attn / 1000.0f;
    ch->master_vol = vol;
    ch->pos = 0;
    ch->end = sfx->length;
}

void S_StopSound(int entnum, int entchannel)
{
    int i;

    for (i = 0; i < num_channels; i++)
    {
        if (channels[i].entnum == entnum &&
            channels[i].entchannel == entchannel)
        {
            channels[i].sfx = NULL;
            break;
        }
    }
}

void S_StopAllSounds(void)
{
    memset(channels, 0, sizeof(channels));
}

// ============================================================================
// Обновление
// ============================================================================

static void S_SpatializeChannel(channel_t* ch)
{
    vec3_t source_vec;
    float dist, dot;
    float lscale, rscale;

    // Расстояние
    VectorSubtract(ch->origin, listener_origin, source_vec);
    dist = VectorNormalize(source_vec) * ch->dist_mult;

    // Громкость по расстоянию
    float scale = 1.0f - dist;
    if (scale < 0) scale = 0;
    if (scale > 1) scale = 1;

    // Стерео панорамирование
    dot = DotProduct(listener_right, source_vec);

    rscale = 0.5f + dot * 0.5f;
    lscale = 0.5f - dot * 0.5f;

    ch->rightvol = ch->master_vol * scale * rscale * s_volume->value;
    ch->leftvol = ch->master_vol * scale * lscale * s_volume->value;
}

void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
    int i;
    channel_t* ch;

    if (!snd_initialized)
        return;

    VectorCopy(origin, listener_origin);
    VectorCopy(forward, listener_forward);
    VectorCopy(right, listener_right);
    VectorCopy(up, listener_up);

    // Обновление каналов
    for (i = 0, ch = channels; i < num_channels; i++, ch++)
    {
        if (!ch->sfx)
            continue;

        S_SpatializeChannel(ch);
    }

    // Микширование в буфер воспроизведения (реализация зависит от бэкенда)
}
