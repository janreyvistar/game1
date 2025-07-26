//------------------------------------------------------------
// shooter.c – compact top‑down shooter (Raylib, C99)
//------------------------------------------------------------
#include <raylib.h>
#include <raymath.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//------------------------------ constants ------------------
#define SCR_W              800
#define SCR_H              600
#define BULLET_POOL        100
#define ENEMY_POOL         10000
#define SPAWN_POINTS       8
#define BULLET_RADIUS      3.0f
#define PLAYER_SIZE        20
#ifndef DEG2RAD                    // raylib already defines it
#define DEG2RAD            (PI/180.0f)
#endif
#define CLAMP(x,lo,hi)     ((x)<(lo)?(lo):((x)>(hi)?(hi):(x)))

//------------------------------ helpers --------------------
static inline float  frand(float a, float b){ return a + ((float)rand()/RAND_MAX)*(b-a); }
static inline Vector2 v2_adds(Vector2 v,float s,Vector2 d){ return (Vector2){v.x+d.x*s, v.y+d.y*s}; }
static inline Vector2 v2_clamp(Vector2 v,Vector2 lo,Vector2 hi){
    return (Vector2){ CLAMP(v.x,lo.x,hi.x), CLAMP(v.y,lo.y,hi.y) };
}

//------------------------------ bullets --------------------
typedef struct{
    bool     active;
    float    damage, speed, life, t;
    Vector2  pos, dir;
} Bullet;

static void bullets_init(Bullet b[BULLET_POOL]){
    for(int i=0;i<BULLET_POOL;i++) b[i]=(Bullet){0};
}
static void bullet_spawn(Bullet b[BULLET_POOL],Vector2 p,Vector2 d,
                         float dmg,float spd,float life){
    for(int i=0;i<BULLET_POOL;i++) if(!b[i].active){
        b[i]=(Bullet){true,dmg,spd,life,0,p,d}; break;
    }
}
static void bullet_update(Bullet* b,float dt){
    b->pos = v2_adds(b->pos,b->speed*dt,b->dir);
    b->t  += dt;
    if(b->t>b->life) b->active=false;
}
static void bullet_draw(const Bullet* b){ DrawCircleV(b->pos,BULLET_RADIUS,BLACK); }

//------------------------------ enums & power‑ups ----------
typedef enum{PU_HEAL,PU_FIRE,PU_RELOAD,PU_SPEED,PU_DMG} PUType;
typedef enum{COMMON,UNCOMMON,RARE} PURarity;

// forward for struct use in PowerUp ↓
struct Player;

typedef struct{
    bool     active;
    Vector2  pos;
    int      size;
    Color    colour;
    PUType   type;
    PURarity rar;
    float h_fac,f_fac,r_fac,s_fac,d_fac;
} PowerUp;

//------------------------------ weapon ---------------------
typedef struct{
    float   dmg, spread, fireRate, reload, bulletLife, bulletSpd;
    int     maxAmmo, ammo;
    float   tFire, tReload;
    bool    reloading;
    Bullet  pool[BULLET_POOL];
} Weapon;

static void weapon_init(Weapon* w){
    *w = (Weapon){
        .dmg=150,.spread=5*DEG2RAD,.fireRate=0.5f,.reload=3,
        .bulletLife=0.4f,.bulletSpd=1000,.maxAmmo=100,.ammo=100
    };
    bullets_init(w->pool);
}
static Vector2 weapon_spread(const Weapon* w,Vector2 d){
    return Vector2Rotate(d,frand(-w->spread,w->spread));
}
static void weapon_shoot(Weapon* w,Vector2 muzzle){
    if(!w->ammo||w->reloading) return;
    w->ammo--; w->tFire=0;
    Vector2 dir = weapon_spread(w,
                    Vector2Normalize(Vector2Subtract(GetMousePosition(),muzzle)));
    bullet_spawn(w->pool,muzzle,dir,w->dmg,w->bulletSpd,w->bulletLife);
}
static void weapon_update(Weapon* w,float dt){
    w->tFire   += dt;
    if(!w->ammo) w->tReload += dt;

    if(!w->ammo && w->tReload>w->reload){
        w->ammo=w->maxAmmo; w->tReload=0; w->reloading=false;
    }
    w->reloading = (w->ammo==0);

    for(int i=0;i<BULLET_POOL;i++)
        if(w->pool[i].active) bullet_update(&w->pool[i],dt);
}
static void weapon_draw(const Weapon* w){
    for(int i=0;i<BULLET_POOL;i++)
        if(w->pool[i].active) bullet_draw(&w->pool[i]);
}

//------------------------------ player ---------------------
typedef struct Player{
    Vector2  pos;
    Rectangle col;
    float    speed;
    int      hp,maxHp;
    Weapon   gun;
} Player;

static void player_init(Player* p){
    *p=(Player){.pos={SCR_W/2,SCR_H/2},.speed=200,.hp=100,.maxHp=100,
                .col={0,0,PLAYER_SIZE,PLAYER_SIZE}};
    weapon_init(&p->gun);
}
static void player_update(Player* p,float dt){
    Vector2 dir={0};
    if(IsKeyDown(KEY_W)) dir.y-=1; if(IsKeyDown(KEY_S)) dir.y+=1;
    if(IsKeyDown(KEY_A)) dir.x-=1; if(IsKeyDown(KEY_D)) dir.x+=1;
    dir=Vector2Normalize(dir);
    p->pos=v2_adds(p->pos,p->speed*dt,dir);
    p->pos=v2_clamp(p->pos,(Vector2){0,0},(Vector2){SCR_W,SCR_H});
    p->col.x=p->pos.x; p->col.y=p->pos.y;

    // fire (kept in player logic)
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && p->gun.tFire>p->gun.fireRate)
        weapon_shoot(&p->gun,p->pos);

    weapon_update(&p->gun,dt);
}
static void player_draw(const Player* p){
    DrawRectangleV((Vector2){p->pos.x-PLAYER_SIZE/2,p->pos.y-PLAYER_SIZE/2},
                   (Vector2){PLAYER_SIZE,PLAYER_SIZE},BLACK);
    weapon_draw(&p->gun);
}

//------------------------------ power‑up logic --------------
static void powerup_spawn(PowerUp* p){
    p->active=true;
    int type=rand()%100;
    if(type<50) p->rar=COMMON;
    else if(type>50&&type<87.5) p->rar=UNCOMMON;
    else p->rar=RARE;
    p->type = rand()%5;
    p->pos  =(Vector2){GetRandomValue(0,SCR_W),GetRandomValue(0,SCR_H)};
    switch(p->rar){
        case COMMON:   p->size=10; p->h_fac=1.1f; p->f_fac=0.6f; p->r_fac=0.9f; p->s_fac=1.1f; p->d_fac=1.1f; break;
        case UNCOMMON: p->size=15; p->h_fac=1.3f; p->f_fac=0.4f; p->r_fac=0.8f; p->s_fac=1.3f; p->d_fac=1.3f; break;
        case RARE:     p->size=20; p->h_fac=1.5f; p->f_fac=0.1f; p->r_fac=0.5f; p->s_fac=1.5f; p->d_fac=1.5f; break;
    }
    p->colour = (Color){255,255,255,255};
    switch(p->type){
        case PU_HEAL:   p->colour=RED;    p->f_fac=p->r_fac=p->s_fac=p->d_fac=1; break;
        case PU_FIRE:   p->colour=YELLOW; p->h_fac=p->r_fac=p->s_fac=p->d_fac=1; break;
        case PU_RELOAD: p->colour=GREEN;  p->h_fac=p->f_fac=p->s_fac=p->d_fac=1; break;
        case PU_SPEED:  p->colour=BLUE;   p->h_fac=p->f_fac=p->r_fac=p->d_fac=1; break;
        case PU_DMG:    p->colour=PINK;   p->h_fac=p->f_fac=p->r_fac=p->s_fac=1; break;
    }
}


//------------------------------ enemies --------------------
typedef struct{
    bool     active, selfHit;
    Vector2  pos, dir;
    float    spd, hp;
    Rectangle col;
} Enemy;

typedef struct{
    Enemy    e[ENEMY_POOL];
    Vector2  spawn[SPAWN_POINTS];
    float    spawnT, spawnRate;
    int      alive, wave, maxPerWave;
    float    maxHp, maxSpd, dmg;
    int      total;
    //----------------------------------
    float    waveT, waveDelay;
    bool     pending;
} EnemyMgr;

static void em_init(EnemyMgr* m){
    *m=(EnemyMgr){.spawnRate=2,.maxPerWave=4,.maxHp=200,.maxSpd=100,.dmg=10,.waveDelay=5};
    for(int i=0;i<ENEMY_POOL;i++) m->e[i].active=false;
    m->spawn[0]=(Vector2){0,0};          m->spawn[1]=(Vector2){SCR_W,0};
    m->spawn[2]=(Vector2){0,SCR_H};      m->spawn[3]=(Vector2){SCR_W,SCR_H};
    m->spawn[4]=(Vector2){SCR_W/2,0};    m->spawn[5]=(Vector2){SCR_W/2,SCR_H};
    m->spawn[6]=(Vector2){0,SCR_H/2};    m->spawn[7]=(Vector2){SCR_W,SCR_H/2};
}
static void em_wave_next(EnemyMgr* m){
    m->wave++; m->pending=false; m->waveT=0;
    m->spawnRate *= expf(-0.03f*m->wave);
    m->maxHp     *= expf(0.01f*m->wave);
    m->maxSpd    *= expf(0.005f*m->wave);
    m->maxPerWave= (int)(m->maxPerWave*expf(0.05f*m->wave));
    m->alive=0; m->total=0;
    for(int i=0;i<ENEMY_POOL;i++) m->e[i].active=false;
}
static void em_wave_logic(EnemyMgr* m,float dt){
    if(m->alive==0 && !m->pending && m->total>=m->maxPerWave){ m->pending=true; m->waveT=0; }
    if(m->pending){ m->waveT+=dt; if(m->waveT>=m->waveDelay) em_wave_next(m); }
}
static void enemy_spawn(EnemyMgr* m){
    if(m->alive>=m->maxPerWave || m->total>=m->maxPerWave) return;
    for(int i=0;i<ENEMY_POOL;i++) if(!m->e[i].active){
        m->e[i]=(Enemy){true,false,m->spawn[rand()%SPAWN_POINTS],{0},m->maxSpd,m->maxHp,{0,0,10,10}};
        m->alive++; m->total++; break;
    }
}
static void enemy_move(Enemy* e,const Player* p,float dt){
    if(!e->selfHit) e->dir=Vector2Normalize(Vector2Subtract(p->pos,e->pos));
    e->pos=v2_adds(e->pos,e->spd*dt,e->dir);
    e->col.x=e->pos.x; e->col.y=e->pos.y;
}
static void enemy_avoid(Enemy* a,Enemy* b){
    if(CheckCollisionCircles(a->pos,10,b->pos,10)){
        a->selfHit=b->selfHit=true;
        a->dir=Vector2Normalize(Vector2Subtract(b->pos,a->pos));
        a->pos=v2_adds(a->pos,-a->spd*GetFrameTime(),a->dir);
        a->col.x=a->pos.x; a->col.y=a->pos.y;
    }else{ a->selfHit=b->selfHit=false; }
}
static void enemy_attack(EnemyMgr* m,int i,Player* p){
    Enemy* e=&m->e[i];
    if(CheckCollisionRecs(p->col,e->col)){
        p->hp-=m->dmg; e->active=false; m->alive--;
    }
}
static void em_update(EnemyMgr* m,Player* p,float dt){
    m->spawnT+=dt; if(m->spawnT>m->spawnRate){ m->spawnT=0; enemy_spawn(m); }
    for(int i=0;i<ENEMY_POOL;i++){
        Enemy* e=&m->e[i]; if(!e->active) continue;
        enemy_attack(m,i,p);
        enemy_move(e,p,dt);
        for(int j=i+1;j<ENEMY_POOL;j++) if(m->e[j].active) enemy_avoid(e,&m->e[j]);

        // bullets
        for(int b=0;b<BULLET_POOL;b++){
            Bullet* bul=&p->gun.pool[b]; if(!bul->active) continue;
            if(CheckCollisionCircleRec(bul->pos,BULLET_RADIUS,e->col)){
                e->hp-=p->gun.dmg; bul->active=false;
            }
        }
        if(e->hp<=0){ e->active=false; m->alive--; }
    }
}
static void em_draw(const EnemyMgr* m){
    for(int i=0;i<ENEMY_POOL;i++) if(m->e[i].active) DrawRectangleRec(m->e[i].col,GREEN);
}
static void draw_enemy_hp(const EnemyMgr* m,int i){
    const Enemy* e=&m->e[i];
    char buf[8]; sprintf(buf,"%d",(int)e->hp);
    Vector2 fs=MeasureTextEx(GetFontDefault(),buf,5,0);
    DrawText(buf,e->pos.x-fs.x/2,e->pos.y-fs.y-10,5,BLACK);
}

//------------------------------ power‑up logic --------------
static void powerup_apply(PowerUp* p,Player* pl){
    if(!p->active) return;
    if(CheckCollisionCircleRec(p->pos,p->size,pl->col)){
        pl->maxHp      =(int)(pl->maxHp*p->h_fac);
        pl->hp         = pl->maxHp;
        pl->speed     *= p->s_fac;
        pl->gun.fireRate*= p->f_fac;
        pl->gun.reload  *= p->r_fac;
        pl->gun.dmg     *= p->d_fac;
        p->active=false;
    }
}
static void powerup_draw(const PowerUp* p){
    if(!p->active) return;
    DrawCircle(p->pos.x,p->pos.y,p->size,p->colour);
    const char* t[]={"HEAL","FIRE","RELOAD","SPEED","DMG"};
    const char* r[]={"COMMON","UNCOMMON","RARE"};
    char txt[32]; sprintf(txt,"%s (%s)",t[p->type],r[p->rar]);
    Vector2 fs=MeasureTextEx(GetFontDefault(),txt,5,0);
    DrawText(txt,p->pos.x-p->size-fs.x,p->pos.y-10-p->size,5,p->colour);
}

//------------------------------ game state -----------------
typedef enum{START,PLAY,END} GameState;

//------------------------------ main -----------------------
int main(void){
    InitWindow(SCR_W,SCR_H,"Shooter");
    SetTargetFPS(60);
    srand(time(NULL));

    GameState state=START;
    float startT=0,startDelay=3; bool startPressed=false;

    Player pl; player_init(&pl);
    EnemyMgr em; em_init(&em);
    PowerUp  pu={0}; bool puSpawned=false;

    while(!WindowShouldClose()){
        float dt=GetFrameTime();

        switch(state){
        case START:{
            BeginDrawing(); ClearBackground(RAYWHITE);
            Vector2 fs=MeasureTextEx(GetFontDefault(),"Shooter",80,0);
            DrawText("Shooter",(SCR_W-fs.x)/2,10+fs.y,80,BLACK);

            if(IsKeyPressed(KEY_ENTER)) startPressed=true;
            if(startPressed){
                startT+=dt;
                int c=(int)ceil(startDelay-startT);
                fs=MeasureTextEx(GetFontDefault(),TextFormat("%d",c),50,0);
                DrawText(TextFormat("%d",c),(SCR_W-fs.x)/2,300+fs.y,50,BLACK);
            }else{
                fs=MeasureTextEx(GetFontDefault(),"Press Enter to start",20,0);
                DrawText("Press Enter to start",(SCR_W-fs.x)/2,300+fs.y,20,BLACK);
            }
            EndDrawing();

            if(startT>=startDelay){
                player_init(&pl); em_init(&em); puSpawned=false;
                state=PLAY;
            }
        } break;

        case PLAY:{
            // update
            powerup_apply(&pu,&pl);
            player_update(&pl,dt);
            em_update(&em,&pl,dt);
            em_wave_logic(&em,dt);

            // spawn power‑up during wave break
            if(em.pending && !puSpawned){ powerup_spawn(&pu); puSpawned=true; }
            if(!em.pending){ puSpawned=false; pu.active=false; }

            // draw
            BeginDrawing(); ClearBackground(RAYWHITE);
            player_draw(&pl); em_draw(&em); powerup_draw(&pu);
            for(int i=0;i<ENEMY_POOL;i++) if(em.e[i].active) draw_enemy_hp(&em,i);

            DrawText(TextFormat("Wave: %d",em.wave),10,10,20,BLACK);
            DrawText(TextFormat("Enemies: %d",em.alive),10,40,20,BLACK);
            DrawText(TextFormat("Max Wave Enemies: %d",em.maxPerWave),10,70,20,DARKGRAY);
            DrawText(TextFormat("Total kills: %d",em.total-em.alive),10,100,20,DARKGRAY);
            DrawText(TextFormat("Enemy Spawn Time: %.2f",em.spawnRate),10,130,20,DARKGRAY);
            if(em.pending){
                DrawText(TextFormat("Next wave in: %.2f",em.waveDelay-em.waveT),
                         SCR_W/2 - MeasureText(TextFormat("Next wave in: %.2f",em.waveDelay-em.waveT),20)/2,
                         10,20,DARKGRAY);
            }

            DrawText(TextFormat("Damage: %.0f",pl.gun.dmg),600,10,20,BLACK);
            DrawText(TextFormat("FireRate: %.2f",pl.gun.fireRate),600,40,20,BLACK);
            DrawText(TextFormat("Reload: %.2f",pl.gun.reload),600,70,20,BLACK);
            DrawText(TextFormat("Speed: %.0f",pl.speed),600,100,20,BLACK);
            DrawText(TextFormat("HP: %d",pl.hp),600,530,20,BLACK);
            DrawText(TextFormat("%d/%d",pl.gun.ammo,pl.gun.maxAmmo),10,530,20,DARKGRAY);
            if(pl.gun.reloading) DrawText("Reloading",20,570,20,DARKPURPLE);
            EndDrawing();

            if(pl.hp<=0 || IsKeyPressed(KEY_Q)) state=END;
        } break;

        case END:{
            BeginDrawing(); ClearBackground(RAYWHITE);
            Vector2 fs=MeasureTextEx(GetFontDefault(),"Game Over",50,0);
            DrawText("Game Over",SCR_W/2-fs.x/2,SCR_H/2-fs.y/2,50,BLACK);
            fs=MeasureTextEx(GetFontDefault(),"Press Enter to play again",20,0);
            DrawText("Press Enter to play again",SCR_W/2-fs.x/2,SCR_H/2+fs.y,20,BLACK);
            EndDrawing();
            if(IsKeyPressed(KEY_ENTER)){ state=START; startT=0; startPressed=true; }
        } break;
        }
    }
    CloseWindow();
    return 0;
}
