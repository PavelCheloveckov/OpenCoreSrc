// engine/client/cl_pred.c
#include "quakedef.h"
#include "client.h"

// ============================================================================
// Предсказание движения игрока
// ============================================================================

void CL_PredictMove(void)
{
    // Простое предсказание: применяем скорость

    /*if (cls.state != ca_active)
        return;

    // Копируем серверную позицию как базу
    // VectorCopy(cl.vieworg, cl.predicted_origin);
    VectorCopy(cl.velocity, cl.predicted_velocity);

    // Применяем последнюю команду
    float frametime = cls.frametime;

    // Простое движение (без коллизий - это делает сервер)
    VectorMA(cl.predicted_origin, frametime, cl.predicted_velocity, cl.predicted_origin);

    // Гравитация (если в воздухе)
    if (!cl.onground)
    {
        cl.predicted_velocity[2] -= 800 * frametime;  // sv_gravity
    }*/
}

// ============================================================================
// Установка позиции от сервера
// ============================================================================

void CL_SetPredictionError(vec3_t error)
{
    VectorCopy(error, cl.prediction_error);
    cl.prediction_time = cls.realtime;
}
