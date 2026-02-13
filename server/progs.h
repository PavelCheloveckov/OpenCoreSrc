#ifndef PROGS_H
#define PROGS_H

#include "quakedef.h"

// ОПРЕДЕЛЕНИЕ ENTVARS_T
struct entvars_s {
    string_t    classname;
    string_t    globalname;

    vec3_t      origin;
    vec3_t      oldorigin;
    vec3_t      velocity;
    vec3_t      basevelocity;
    vec3_t      clbasevelocity;
    vec3_t      movedir;

    vec3_t      angles;
    vec3_t      avelocity;
    vec3_t      punchangle;
    vec3_t      v_angle;

    vec3_t      endpos;
    vec3_t      startpos;
    float       impacttime;
    float       starttime;

    int         fixangle;
    float       idealpitch;
    float       pitch_speed;
    float       ideal_yaw;
    float       yaw_speed;

    int         modelindex;
    string_t    model;

    int         viewmodel;
    int         weaponmodel;

    vec3_t      absmin;
    vec3_t      absmax;
    vec3_t      mins;
    vec3_t      maxs;
    vec3_t      size;

    float       ltime;
    float       nextthink;

    int         movetype;
    int         solid;

    int         skin;
    int         body;
    int         effects;

    float       gravity;
    float       friction;

    int         light_level;

    int         sequence;
    int         gaitsequence;
    float       frame;
    float       animtime;
    float       framerate;
    byte        controller[4];
    byte        blending[2];

    float       scale;

    int         rendermode;
    float       renderamt;
    vec3_t      rendercolor;
    int         renderfx;

    float       health;
    float       frags;
    int         weapons;
    float       takedamage;

    int         deadflag;
    vec3_t      view_ofs;

    int         button;
    int         impulse;

    struct edict_s* chain;
    struct edict_s* dmg_inflictor;
    struct edict_s* enemy;
    struct edict_s* aiment;
    struct edict_s* owner;
    struct edict_s* groundentity;

    int         spawnflags;
    int         flags;

    int         colormap;
    int         team;

    float       max_health;
    float       teleport_time;
    float       armortype;
    float       armorvalue;
    int         waterlevel;
    int         watertype;

    string_t    target;
    string_t    targetname;
    string_t    netname;
    string_t    message;

    float       dmg_take;
    float       dmg_save;
    float       dmg;
    float       dmgtime;

    string_t    noise;
    string_t    noise1;
    string_t    noise2;
    string_t    noise3;

    float       speed;
    float       air_finished;
    float       pain_finished;
    float       radsuit_finished;

    struct edict_s* pContainingEntity;

    int         playerclass;
    float       maxspeed;

    float       fov;
    int         weaponanim;

    int         pushmsec;

    int         bInDuck;
    int         flTimeStepSound;
    int         flSwimTime;
    int         flDuckTime;
    int         iStepLeft;
    float       flFallVelocity;

    int         gamestate;
    int         oldbuttons;
    int         groupinfo;

    int         iuser1, iuser2, iuser3, iuser4;
    float       fuser1, fuser2, fuser3, fuser4;
    vec3_t      vuser1, vuser2, vuser3, vuser4;
    struct edict_s* euser1, * euser2, * euser3, * euser4;
};

// ОПРЕДЕЛЕНИЕ EDICT_S
struct edict_s {
    qboolean    free;
    int         serialnumber;
    float       freetime;
    struct entvars_s v;
    void* pvPrivateData;
};

#endif // PROGS_H
