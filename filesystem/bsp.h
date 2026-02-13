#ifndef BSP_H
#define BSP_H

#include "quakedef.h"

// ============================================================================
// Константы BSP
// ============================================================================
#define QBSP_MAGIC   (('Q') | ('B' << 8) | ('S' << 16) | ('P' << 24))
#define QBSP_VERSION 1

// Порядок лумпов как в компиляторе
enum
{
    LUMP_ENTITIES,
    LUMP_PLANES,
    LUMP_TEXTURES,
    LUMP_VERTICES,
    LUMP_VISIBILITY,
    LUMP_NODES,
    LUMP_TEXINFO,
    LUMP_FACES,
    LUMP_LIGHTING,
    LUMP_LEAFS,
    LUMP_MARKSURFACES,
    LUMP_EDGES,
    LUMP_SURFEDGES,
    LUMP_MODELS,
    LUMP_BRUSHES,
    LUMP_BRUSHSIDES,
    LUMP_COUNT
};

#define HEADER_LUMPS LUMP_COUNT
#define LUMP_LEAVES  LUMP_LEAFS  // алиас для совместимости

// Типы плоскостей
#define PLANE_X             0
#define PLANE_Y             1
#define PLANE_Z             2
#define PLANE_ANYX          3
#define PLANE_ANYY          4
#define PLANE_ANYZ          5

// Флаги поверхности
#define SURF_PLANEBACK      2
#define SURF_DRAWSKY        4
#define SURF_DRAWSPRITE     8
#define SURF_DRAWTURB       16
#define SURF_DRAWTILED      32
#define SURF_DRAWBACKGROUND 64

#define VERTEXSIZE          7

// ============================================================================
// Структуры диска (как в файле)
// ============================================================================

typedef struct lump_s {
    int offset;
    int length;
} lump_t;

typedef struct dheader_s {
    int magic;
    int version;
    lump_t lumps[LUMP_COUNT];
} dheader_t;

typedef struct dmodel_s {
    float mins[3], maxs[3];
    float origin[3];
    int headnode;      // в новом формате int, один headnode
    int firstface, numfaces;
} dmodel_t;

typedef struct dvertex_s {
    float point[3];
} dvertex_t;

typedef struct dplane_s {
    float normal[3];
    float dist;
    int type;
} dplane_t;

typedef struct dnode_s {
    int planenum;
    int children[2];   // int в новом формате
    short mins[3];
    short maxs[3];
    unsigned short firstface;
    unsigned short numfaces;
} dnode_t;

typedef struct dclipnode_s {
    int planenum;
    short children[2];
} dclipnode_t;

typedef struct texinfo_s {
    float vecs[2][4];
    int miptex;
    int flags;
} texinfo_t;

typedef struct dedge_s {
    unsigned short v[2];
} dedge_t;

typedef struct dface_s {
    int planenum;
    short side;
    int firstedge;
    short numedges;
    short texinfo;
    byte styles[4];
    int lightofs;
} dface_t;

typedef struct dleaf_s {
    int contents;
    int visofs;
    short mins[3];
    short maxs[3];
    unsigned short firstmarksurface;
    unsigned short nummarksurfaces;
    // ambient_level нет в новом формате
} dleaf_t;

// ============================================================================
// Структуры памяти (Runtime)
// ============================================================================

// Forward declarations
struct msurface_s;
struct mnode_s;
struct texture_s;

typedef struct glpoly_s {
    struct glpoly_s* next;
    struct glpoly_s* chain;
    int             numverts;
    int             flags;
    float           verts[4][VERTEXSIZE];
} glpoly_t;

typedef struct mvertex_s {
    vec3_t position;
} mvertex_t;

typedef struct medge_s {
    unsigned short v[2];
    unsigned int cachededgeoffset;
} medge_t;

typedef struct texture_s {
    char name[16];
    unsigned width, height;
    int gl_texturenum;
    struct msurface_s* texturechain;
    int anim_total;
    int anim_min, anim_max;
    struct texture_s* anim_next;
    struct texture_s* alternate_anims;
    unsigned offsets[4];
} texture_t;

typedef struct mtexinfo_s {
    float vecs[2][4];
    float mipadjust;
    struct texture_s* texture;
    int flags;
} mtexinfo_t;

typedef struct msurface_s {
    int visframe;
    mplane_t* plane;
    int flags;
    int firstedge;
    int numedges;
    int texturemins[2];
    int extents[2];
    int light_s, light_t;
    glpoly_t* polys;
    struct msurface_s* texturechain;
    mtexinfo_t* texinfo;
    int dlightframe;
    int dlightbits;
    unsigned lightmaptexturenum;
    byte styles[4];
    int cached_light[4];
    int lightofs;
    byte* samples;
} msurface_t;

typedef struct mnode_s {
    int contents;
    int visframe;
    short mins[3];
    short maxs[3];
    struct mnode_s* parent;
    mplane_t* plane;
    struct mnode_s* children[2];
    unsigned short firstsurface;
    unsigned short numsurfaces;
} mnode_t;

typedef struct mleaf_s {
    int contents;
    int visframe;
    short mins[3];
    short maxs[3];
    struct mnode_s* parent;
    byte* compressed_vis;
    struct msurface_s** firstmarksurface;
    int nummarksurfaces;
    int key;
    byte ambient_sound_level[4];
} mleaf_t;

typedef struct hull_s {
    dclipnode_t* clipnodes;
    mplane_t* planes;
    int firstclipnode;
    int lastclipnode;
    vec3_t clip_mins;
    vec3_t clip_maxs;
} hull_t;

typedef struct model_s {
    char name[MAX_QPATH];
    qboolean needload;
    int type;
    int numframes;
    int flags;

    vec3_t mins, maxs;
    float radius;

    int firstmodelsurface, nummodelsurfaces;

    int numsubmodels;
    dmodel_t* submodels;

    int numplanes;
    mplane_t* planes;

    int numleafs;
    mleaf_t* leafs;

    int numvertexes;
    mvertex_t* vertexes;

    int numedges;
    medge_t* edges;

    int numnodes;
    mnode_t* nodes;

    int numtexinfo;
    mtexinfo_t* texinfo;

    int numsurfaces;
    msurface_t* surfaces;

    int numsurfedges;
    int* surfedges;

    int numclipnodes;
    dclipnode_t* clipnodes;

    int nummarksurfaces;
    msurface_t** marksurfaces;

    hull_t hulls[4];

    int numtextures;
    struct texture_s** textures;

    byte* visdata;
    byte* lightdata;
    char* entities;

} model_t;

// Функции
model_t* Mod_LoadBrushModel(const char* name);
void Mod_FreeModel(model_t* mod);
mleaf_t* Mod_PointInLeaf(vec3_t p, model_t* model);
byte* Mod_LeafPVS(mleaf_t* leaf, model_t* model);

#endif // BSP_H
