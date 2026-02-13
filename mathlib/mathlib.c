// mathlib/mathlib.c
#include "quakedef.h"
#include "bsp.h"
#include <mathlib.h>

vec3_t vec3_origin = { 0,0,0 };

void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
    cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
    cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
    cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

vec_t VectorNormalize(vec3_t v)
{
    float length, ilength;

    length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (length)
    {
        ilength = 1.0f / length;
        v[0] *= ilength;
        v[1] *= ilength;
        v[2] *= ilength;
    }

    return length;
}

void VectorNormalizeFast(vec3_t v)
{
    float ilength = 1.0f / sqrtf(DotProduct(v, v));
    v[0] *= ilength;
    v[1] *= ilength;
    v[2] *= ilength;
}

void VectorInverse(vec3_t v)
{
    v[0] = -v[0];
    v[1] = -v[1];
    v[2] = -v[2];
}

int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const mplane_t* p)
{
    float dist1, dist2;
    int sides;

    switch (p->signbits)
    {
    case 0:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        break;
    case 1:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        break;
    case 2:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        break;
    case 3:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        break;
    case 4:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        break;
    case 5:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        break;
    case 6:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        break;
    case 7:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        break;
    default:
        dist1 = dist2 = 0;
        break;
    }

    sides = 0;
    if (dist1 >= p->dist)
        sides = 1;
    if (dist2 < p->dist)
        sides |= 2;

    return sides;
}

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
    float angle;
    float sr, sp, sy, cr, cp, cy;

    angle = DEG2RAD(angles[1]);  // yaw
    sy = sinf(angle);
    cy = cosf(angle);

    angle = DEG2RAD(angles[0]);  // pitch
    sp = sinf(angle);
    cp = cosf(angle);

    angle = DEG2RAD(angles[2]);  // roll
    sr = sinf(angle);
    cr = cosf(angle);

    if (forward)
    {
        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;
    }

    if (right)
    {
        right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
        right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
        right[2] = -1 * sr * cp;
    }

    if (up)
    {
        up[0] = (cr * sp * cy + -sr * -sy);
        up[1] = (cr * sp * sy + -sr * cy);
        up[2] = cr * cp;
    }
}

void VectorAngles(const vec3_t forward, vec3_t angles)
{
    float tmp, yaw, pitch;

    if (forward[1] == 0 && forward[0] == 0)
    {
        yaw = 0;
        if (forward[2] > 0)
            pitch = 90;
        else
            pitch = 270;
    }
    else
    {
        yaw = (atan2f(forward[1], forward[0]) * 180.0f / M_PI_F);
        if (yaw < 0)
            yaw += 360;

        tmp = sqrtf(forward[0] * forward[0] + forward[1] * forward[1]);
        pitch = (atan2f(forward[2], tmp) * 180.0f / M_PI_F);
        if (pitch < 0)
            pitch += 360;
    }

    angles[0] = pitch;
    angles[1] = yaw;
    angles[2] = 0;
}

void AngleMatrix(const vec3_t angles, float (*matrix)[4])
{
    float angle;
    float sr, sp, sy, cr, cp, cy;

    angle = DEG2RAD(angles[1]);
    sy = sinf(angle);
    cy = cosf(angle);
    angle = DEG2RAD(angles[0]);
    sp = sinf(angle);
    cp = cosf(angle);
    angle = DEG2RAD(angles[2]);
    sr = sinf(angle);
    cr = cosf(angle);

    matrix[0][0] = cp * cy;
    matrix[1][0] = cp * sy;
    matrix[2][0] = -sp;
    matrix[0][1] = sr * sp * cy + cr * -sy;
    matrix[1][1] = sr * sp * sy + cr * cy;
    matrix[2][1] = sr * cp;
    matrix[0][2] = (cr * sp * cy + -sr * -sy);
    matrix[1][2] = (cr * sp * sy + -sr * cy);
    matrix[2][2] = cr * cp;
    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
}

void VectorTransform(const vec3_t in1, const float in2[3][4], vec3_t out)
{
    out[0] = DotProduct(in1, in2[0]) + in2[0][3];
    out[1] = DotProduct(in1, in2[1]) + in2[1][3];
    out[2] = DotProduct(in1, in2[2]) + in2[2][3];
}

void ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
    out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
    out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
    out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
    out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];
    out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
    out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
    out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
    out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];
    out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
    out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
    out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
    out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

float anglemod(float a)
{
    a = (360.0f / 65536) * ((int)(a * (65536 / 360.0f)) & 65535);
    return a;
}

float AngleNormalize(float angle)
{
    angle = fmodf(angle, 360.0f);
    if (angle > 180)
        angle -= 360;
    if (angle < -180)
        angle += 360;
    return angle;
}

float AngleDiff(float destAngle, float srcAngle)
{
    float delta;
    delta = destAngle - srcAngle;
    if (destAngle > srcAngle)
    {
        if (delta >= 180)
            delta -= 360;
    }
    else
    {
        if (delta <= -180)
            delta += 360;
    }
    return delta;
}

float ApproachAngle(float target, float value, float speed)
{
    float delta = target - value;
    if (speed < 0)
        speed = -speed;
    if (delta < -180)
        delta += 360;
    else if (delta > 180)
        delta -= 360;
    if (delta > speed)
        value += speed;
    else if (delta < -speed)
        value -= speed;
    else
        value = target;
    return value;
}

int Q_log2(int val)
{
    int answer = 0;
    while ((val >>= 1) != 0)
        answer++;
    return answer;
}

void SeedRandomNumberGenerator(void)
{
    srand((unsigned int)time(NULL));
}

float RandomFloat(float flLow, float flHigh)
{
    float fl = (float)rand() / (float)RAND_MAX;
    return (fl * (flHigh - flLow)) + flLow;
}

int RandomLong(int lLow, int lHigh)
{
    unsigned int maxAcceptable;
    unsigned int x = lHigh - lLow + 1;
    unsigned int n;

    if (x <= 0 || RAND_MAX < x - 1)
        return lLow;

    maxAcceptable = RAND_MAX - ((RAND_MAX + 1) % x);
    do {
        n = rand();
    } while (n > maxAcceptable);

    return lLow + (n % x);
}

#undef VectorCopy
#undef VectorAdd
#undef VectorSubtract
#undef VectorScale
#undef VectorSet
#undef DotProduct
#undef VectorMA

void VectorCopy(const vec3_t in, vec3_t out)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

void VectorAdd(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
    out[0] = veca[0] + vecb[0];
    out[1] = veca[1] + vecb[1];
    out[2] = veca[2] + vecb[2];
}

void VectorSubtract(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
    out[0] = veca[0] - vecb[0];
    out[1] = veca[1] - vecb[1];
    out[2] = veca[2] - vecb[2];
}

void VectorScale(const vec3_t in, vec_t scale, vec3_t out)
{
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
}

void VectorSet(vec3_t v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

vec_t DotProduct(const vec3_t v1, const vec3_t v2)
{
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void VectorMA(const vec3_t veca, float scale, const vec3_t vecb, vec3_t out)
{
    out[0] = veca[0] + scale * vecb[0];
    out[1] = veca[1] + scale * vecb[1];
    out[2] = veca[2] + scale * vecb[2];
}
