// mathlib/mathlib.h
#ifndef MATHLIB_H
#define MATHLIB_H

#include <math.h>
#include "quakedef.h"

#define M_PI            3.14159265358979323846
#define M_PI_F          ((float)(M_PI))

#define DEG2RAD(a)      ((a) * (M_PI_F / 180.0f))
#define RAD2DEG(a)      ((a) * (180.0f / M_PI_F))

#define DotProduct(x, y)        ((x)[0]*(y)[0] + (x)[1]*(y)[1] + (x)[2]*(y)[2])
#define VectorSubtract(a, b, c) ((c)[0]=(a)[0]-(b)[0], (c)[1]=(a)[1]-(b)[1], (c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a, b, c)      ((c)[0]=(a)[0]+(b)[0], (c)[1]=(a)[1]+(b)[1], (c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a, b)        ((b)[0]=(a)[0], (b)[1]=(a)[1], (b)[2]=(a)[2])
#define VectorClear(a)          ((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a, b)      ((b)[0]=-(a)[0], (b)[1]=-(a)[1], (b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)   ((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define VectorScale(in, scale, out) ((out)[0]=(in)[0]*(scale), (out)[1]=(in)[1]*(scale), (out)[2]=(in)[2]*(scale))
#define VectorMA(v, s, b, o)    ((o)[0]=(v)[0]+(b)[0]*(s), (o)[1]=(v)[1]+(b)[1]*(s), (o)[2]=(v)[2]+(b)[2]*(s))
#define VectorLength(a)         (sqrtf(DotProduct(a, a)))
#define VectorCompare(a, b)     (((a)[0]==(b)[0]) && ((a)[1]==(b)[1]) && ((a)[2]==(b)[2]))
#define VectorAverage(a, b, o)  ((o)[0]=((a)[0]+(b)[0])*0.5f, (o)[1]=((a)[1]+(b)[1])*0.5f, (o)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorLerp(a, lerp, b, c) ((c)[0]=(a)[0]+(lerp)*((b)[0]-(a)[0]), (c)[1]=(a)[1]+(lerp)*((b)[1]-(a)[1]), (c)[2]=(a)[2]+(lerp)*((b)[2]-(a)[2]))

#define BOX_ON_PLANE_SIDE(emins, emaxs, p) \
    (((p)->type < 3) ? \
        (((p)->dist <= (emins)[(p)->type]) ? 1 : \
        (((p)->dist >= (emaxs)[(p)->type]) ? 2 : 3)) \
    : BoxOnPlaneSide((emins), (emaxs), (p)))

#define bound(min, num, max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define Q_min(a, b) (((a) < (b)) ? (a) : (b))
#define Q_max(a, b) (((a) > (b)) ? (a) : (b))
#define Q_rint(x)   ((x) < 0 ? ((int)((x)-0.5f)) : ((int)((x)+0.5f)))

extern vec3_t vec3_origin;

// Функции
void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);
vec_t VectorNormalize(vec3_t v);
void VectorNormalizeFast(vec3_t v);
void VectorInverse(vec3_t v);
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const mplane_t* plane);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void VectorAngles(const vec3_t forward, vec3_t angles);
void AngleVectorsTranspose(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void AngleMatrix(const vec3_t angles, float (*matrix)[4]);
void AngleIMatrix(const vec3_t angles, float (*matrix)[4]);
void VectorRotate(const vec3_t in1, const float in2[3][4], vec3_t out);
void VectorIRotate(const vec3_t in1, const float in2[3][4], vec3_t out);
void VectorTransform(const vec3_t in1, const float in2[3][4], vec3_t out);
void ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);
float anglemod(float a);
float ApproachAngle(float target, float value, float speed);
float AngleDiff(float destAngle, float srcAngle);
float AngleNormalize(float angle);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
int Q_log2(int val);

// Random
void SeedRandomNumberGenerator(void);
float RandomFloat(float flLow, float flHigh);
int RandomLong(int lLow, int lHigh);

#endif // MATHLIB_H
