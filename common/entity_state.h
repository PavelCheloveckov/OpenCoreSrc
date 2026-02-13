// common/entity_state.h
#ifndef ENTITY_STATE_H
#define ENTITY_STATE_H

#include "quakedef.h"

// Entity State - сетевое состояние сущности
typedef struct entity_state_s {
    int entityType;
    int number;

    vec3_t origin;
    vec3_t angles;

    int modelindex;
    int sequence;
    float frame;
    int colormap;
    short skin;
    short solid;

    int effects;
    float scale;
    byte eflags;

    int rendermode;
    int renderamt;
    color24 rendercolor;
    int renderfx;

    int movetype;
    float animtime;
    float framerate;
    int body;
    byte controller[4];
    byte blending[4];

    vec3_t velocity;
    vec3_t mins;
    vec3_t maxs;

    int aiment;
    int owner;

    float friction;
    float gravity;

    int team;
    int playerclass;
    int health;
    int spectator;
    int weaponmodel;
    int gaitsequence;
    vec3_t basevelocity;
    int usehull;
    int oldbuttons;
    int onground;
    int iStepLeft;
    float flFallVelocity;
    float fov;
    int weaponanim;
    vec3_t startpos;
    vec3_t endpos;
    float impacttime;
    float starttime;

    int iuser1;
    int iuser2;
    int iuser3;
    int iuser4;
    float fuser1;
    float fuser2;
    float fuser3;
    float fuser4;
    vec3_t vuser1;
    vec3_t vuser2;
    vec3_t vuser3;
    vec3_t vuser4;
} entity_state_t;

#endif // ENTITY_STATE_H
