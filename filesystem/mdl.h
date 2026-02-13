// mdl.h

#ifndef MDL_H
#define MDL_H

#include "quakedef.h"

// Заголовок MDL файла (Quake 1)
#define IDPOLYHEADER    (('O'<<24)+('P'<<16)+('D'<<8)+'I')  // "IDPO"
#define ALIAS_VERSION   6

typedef enum {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_RUN,
    ANIM_ATTACK,
    ANIM_PAIN,
    ANIM_DEATH,
    ANIM_COUNT
} anim_type_t;

typedef struct {
    int start;
    int end;
    float framerate;
    qboolean loop;
} anim_sequence_t;

typedef struct
{
    int         ident;          // "IDPO"
    int         version;        // 6
    vec3_t      scale;          // масштаб
    vec3_t      scale_origin;   // смещение
    float       boundingradius;
    vec3_t      eyeposition;    // позиция глаз
    int         numskins;
    int         skinwidth;
    int         skinheight;
    int         numverts;
    int         numtris;
    int         numframes;
    int         synctype;       // 0 = sync, 1 = rand
    int         flags;
    float       size;
} mdl_header_t;

// Вершина (сжатая)
typedef struct
{
    byte    v[3];           // позиция (0-255, масштабируется)
    byte    lightnormalindex;
} trivertx_t;

// Треугольник
typedef struct
{
    int     facesfront;     // 0 = back, 1 = front
    int     vertindex[3];
} dtriangle_t;

// Текстурные координаты
typedef struct
{
    int     onseam;         // на шве?
    int     s, t;           // координаты
} stvert_t;

// Один кадр анимации
typedef struct
{
    trivertx_t  bboxmin, bboxmax;   // bounding box
    char        name[16];           // имя кадра
    // за ним идут numverts * trivertx_t
} daliasframe_t;

// Загруженная модель
typedef struct aliasmodel_s
{
    char        name[64];

    int         numframes;
    int         numverts;
    int         numtris;

    int         skinwidth;
    int         skinheight;
    int         numskins;
    GLuint* skin_textures;      // GL текстуры скинов

    vec3_t      mins;
    vec3_t      maxs;

    vec3_t*     frame_mins;   // [numframes]
    vec3_t*     frame_maxs;   // [numframes]

    vec3_t      scale;
    vec3_t      scale_origin;

    dtriangle_t* triangles;
    stvert_t* texcoords;

    // Кадры анимации
    trivertx_t** frames;            // [numframes][numverts]
    char** frame_names;
    anim_sequence_t anims[ANIM_COUNT];

} aliasmodel_t;

// Функции
aliasmodel_t* Mod_LoadAliasModel(const char* name);
void Mod_FreeAliasModel(aliasmodel_t* mod);
void Mod_FindAnimations(aliasmodel_t* mod);

#endif
