// server/sv_move.c
#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "cvar.h"
#include "mathlib.h"
#include "server.h"
#include "progs.h"
#include <bsp.h>

vec3_t  trace_start, trace_end;
vec3_t  trace_mins, trace_maxs;
vec3_t  trace_extents;

trace_t trace_all; // Результат
float   trace_fraction; // 1.0 = прошли весь путь, 0.5 = врезались на полпути
int     trace_contents;
extern  model_t* cl_worldmodel;

static qboolean SV_TestEntityPosition(edict_t* ent)
{
    trace_t tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent);
    return (tr.startsolid || tr.allsolid);
}

static void SV_Unstick(edict_t* ent)
{
    int i;
    for (i = 0; i < 128; i++)
    {
        if (!SV_TestEntityPosition(ent))
            return;
        ent->v.origin[2] += 1.0f;
    }
}

static void SV_DropToFloorEdict(edict_t* ent)
{
    vec3_t end;
    trace_t tr;

    VectorCopy(ent->v.origin, end);
    end[2] -= 256.0f;

    tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
    VectorCopy(tr.endpos, ent->v.origin);

    if (tr.fraction < 1.0f)
    {
        ent->v.origin[2] += 1.0f;
        ent->v.flags |= FL_ONGROUND;
        ent->v.groundentity = tr.ent;
    }
}

static int SV_HullPointContents(hull_t* hull, int num, vec3_t p)
{
    while (num >= 0)
    {
        dclipnode_t* node = &hull->clipnodes[num];
        mplane_t* plane = &hull->planes[node->planenum];

        float d;
        if (plane->type < 3)
            d = p[plane->type] - plane->dist;
        else
            d = DotProduct(p, plane->normal) - plane->dist;

        num = (d < 0) ? node->children[1] : node->children[0];
    }
    return num;
}

// Проверка столкновения линии с деревом клипнодов
static qboolean SV_RecursiveHullCheck(hull_t* hull, int num,
    float p1f, float p2f,
    vec3_t p1, vec3_t p2,
    trace_t* tr)
{
    if (num < 0)
    {
        if (num != CONTENTS_SOLID)
        {
            tr->allsolid = FALSE;
            return TRUE;
        }
        tr->startsolid = TRUE;
        return TRUE;
    }

    dclipnode_t* node = &hull->clipnodes[num];
    mplane_t* plane = &hull->planes[node->planenum];

    float t1 = DotProduct(p1, plane->normal) - plane->dist;
    float t2 = DotProduct(p2, plane->normal) - plane->dist;

    if (t1 >= 0 && t2 >= 0)
        return SV_RecursiveHullCheck(hull, node->children[0], p1f, p2f, p1, p2, tr);
    if (t1 < 0 && t2 < 0)
        return SV_RecursiveHullCheck(hull, node->children[1], p1f, p2f, p1, p2, tr);

    #define DIST_EPSILON 0.03125f

    int side = (t1 < 0);

    float frac;
    if (!side)
        frac = (t1 - DIST_EPSILON) / (t1 - t2);
    else
        frac = (t1 + DIST_EPSILON) / (t1 - t2);

    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;

    float midf = p1f + (p2f - p1f) * frac;

    vec3_t mid;
    VectorLerp(p1, frac, p2, mid);

    if (!SV_RecursiveHullCheck(hull, node->children[side], p1f, midf, p1, mid, tr))
        return FALSE;

    if (SV_HullPointContents(hull, node->children[side ^ 1], mid) != CONTENTS_SOLID)
        return SV_RecursiveHullCheck(hull, node->children[side ^ 1], midf, p2f, mid, p2, tr);

    if (tr->allsolid)
        return FALSE;

    tr->fraction = midf;
    VectorCopy(mid, tr->endpos);

    VectorCopy(plane->normal, tr->plane.normal);
    tr->plane.dist = plane->dist;
    if (side)
    {
        VectorNegate(tr->plane.normal, tr->plane.normal);
        tr->plane.dist = -tr->plane.dist;
    }

    return FALSE;
}

static trace_t SV_TraceLine(vec3_t start, vec3_t end)
{
    trace_t tr;
    hull_t* hull;

    memset(&tr, 0, sizeof(tr));
    tr.fraction = 1.0f;
    tr.allsolid = TRUE;
    tr.startsolid = FALSE;
    tr.ent = EDICT_NUM(0);

    hull = &sv.worldmodel->hulls[0];
    SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.0f, 1.0f, start, end, &tr);
    VectorLerp(start, tr.fraction, end, tr.endpos);

    return tr;
}

// Эта функция должна проверять столкновения
trace_t SV_Move(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t* passedict)
{
    trace_t best;
    vec3_t s, e;
    int i;

    memset(&best, 0, sizeof(best));
    best.fraction = 1.0f;
    best.allsolid = FALSE;
    best.startsolid = FALSE;
    best.ent = EDICT_NUM(0);
    VectorCopy(end, best.endpos);

    if (!sv.worldmodel)
        return best;

    // Если точечная трассировка (mins=maxs=0)
    if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0 &&
        maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0)
    {
        best = SV_TraceLine(start, end);
        return best;
    }

    // Точки для проверки bbox - 8 углов + центр
    vec3_t offsets[9] = {
        {mins[0], mins[1], mins[2]},  // нижний угол 1
        {maxs[0], mins[1], mins[2]},  // нижний угол 2
        {mins[0], maxs[1], mins[2]},  // нижний угол 3
        {maxs[0], maxs[1], mins[2]},  // нижний угол 4
        {mins[0], mins[1], maxs[2]},  // верхний угол 1
        {maxs[0], mins[1], maxs[2]},  // верхний угол 2
        {mins[0], maxs[1], maxs[2]},  // верхний угол 3
        {maxs[0], maxs[1], maxs[2]},  // верхний угол 4
        {0, 0, 0}                      // центр
    };

    best.fraction = 1.0f;
    best.allsolid = FALSE;

    for (i = 0; i < 9; i++)
    {
        VectorAdd(start, offsets[i], s);
        VectorAdd(end, offsets[i], e);

        trace_t tr = SV_TraceLine(s, e);

        if (tr.startsolid)
            best.startsolid = TRUE;

        if (tr.allsolid)
        {
            best.allsolid = TRUE;
            best.startsolid = TRUE;
        }

        if (tr.fraction < best.fraction)
        {
            best.fraction = tr.fraction;
            best.plane = tr.plane;
            best.startsolid = tr.startsolid;
        }
    }

    // Вычисляем endpos для origin (не для точки проверки)
    VectorLerp(start, best.fraction, end, best.endpos);

    return best;
}

trace_t SV_PushEntity(edict_t* ent, vec3_t push)
{
    trace_t trace;
    vec3_t start;
    vec3_t end;

    VectorCopy(ent->v.origin, start);
    VectorAdd(start, push, end);

    // Пробуем переместить сущность
    // MOVE_NORMAL означает обычное движение с проверкой столкновений
    trace = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

    // Если переместились, обновляем позицию
    if (trace.fraction > 0)
    {
        VectorCopy(trace.endpos, ent->v.origin);
        SV_LinkEdict(ent, TRUE);
    }

    return trace;
}

// Проверка содержимого точки (вода, лава, воздух)
int SV_PointContents(vec3_t p)
{
    mnode_t* node;
    mplane_t* plane;
    float d;
    model_t* model;

    // Используем cl_worldmodel (загруженная карта)
    model = cl_worldmodel;

    if (!model || !model->nodes)
    {
        // Fallback на серверную модель
        model = sv.worldmodel;
    }

    if (!model || !model->nodes)
        return CONTENTS_EMPTY;

    node = model->nodes;

    while (node->contents >= 0)  // пока не дошли до листа
    {
        plane = node->plane;

        if (!plane)
            return CONTENTS_EMPTY;

        d = DotProduct(p, plane->normal) - plane->dist;

        if (d > 0)
            node = node->children[0];
        else
            node = node->children[1];

        if (!node)
            return CONTENTS_EMPTY;
    }

    return node->contents;
}

void SV_LinkEdict(edict_t* ent, qboolean touch_triggers)
{
    // Здесь код, который помещает сущность в дерево коллизий
}

void SV_UnlinkEdict(edict_t* ent)
{
    // Удаление из дерева коллизий
}
