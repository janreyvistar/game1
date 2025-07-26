#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include<math.h>
//--------------------------- constants ----------------------
#define SCR_W                 800
#define SCR_H                 600
#define BULLET_POOL           100
#define ENEMY_POOL            10000
#define SPAWN_POINTS          8
#define BULLET_RADIUS         3.0f
#define PLAYER_SIZE           20
//#define DEG2RAD               (PI / 180.0f)
#define CLAMP(x, min, max)    ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
enum Game{
  START,
  PLAYING,
  END  
};
Sound shooting_sound;
Sound hit_sound;
Sound powerup_sound;
Sound death_sound;
Sound count_down_sound;
Vector2 clamp_v2(Vector2 v, Vector2 min, Vector2 max) {
    return (Vector2){ CLAMP(v.x, min.x, max.x), CLAMP(v.y, min.y, max.y) };
}
static inline Vector2 v2_scale_add(Vector2 v, float s, Vector2 add) {
    return (Vector2){ v.x + add.x * s, v.y + add.y * s };
}

static inline float randf(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

//--------------------------- bullets ------------------------
typedef struct {
    bool   active;
    float  damage;
    float  lifespan, lifeTimer;
    float  speed;
    Vector2 pos, dir;
} Bullet;

static void bullet_pool_init(Bullet pool[BULLET_POOL]) {
    for (int i = 0; i < BULLET_POOL; ++i)
        pool[i] = (Bullet){ .active = false, .damage = 50, .lifespan = 0.4f, .speed = 1000 };
}

static void bullet_spawn(Bullet pool[BULLET_POOL], Vector2 pos, Vector2 dir) {
    for (int i = 0; i < BULLET_POOL; ++i) {
        Bullet *b = &pool[i];
        if (!b->active) {
            *b = (Bullet){ .active = true, .damage = 50, .lifespan = 0.4f,
                           .speed = 1000, .pos = pos, .dir = dir };
            break;
        }
    }
}

static void bullet_update(Bullet *b, float dt) {
    b->pos = v2_scale_add(b->pos, b->speed * dt, b->dir);
    b->lifeTimer += dt;
    if (b->lifeTimer > b->lifespan) b->active = false;
}

static void bullet_draw(const Bullet *b) {
    DrawCircleV(b->pos, BULLET_RADIUS, BLACK);
}

//--------------------------- weapon -------------------------
typedef struct {
    float  spread;       // radians
    float  fireRate;     // seconds/bullet
    float  reloadTime;   // seconds/mag
    int    max_rounds;

    float  fireTimer;
    float  reloadTimer;
    int    ammo;
    bool reloading;
    float damage;
    Bullet bullets[BULLET_POOL];
} Weapon;

static void weapon_init(Weapon *w) {
    *w = (Weapon){
        .spread      = 5.0f * DEG2RAD,
        .fireRate    = 0.5f,
        .reloadTime  = 3.0f,
        .max_rounds  = 100,
        .ammo        = 100,
        .damage      = 150
    };
    bullet_pool_init(w->bullets);
}

static Vector2 weapon_apply_spread(const Weapon *w, Vector2 dir) {
    return Vector2Rotate(dir, randf(-w->spread, w->spread));
}

static void weapon_update(Weapon *w, Vector2 muzzle, float dt) {
    bool want_fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    // timers
    w->fireTimer   += dt;
    if (!w->ammo)  w->reloadTimer += dt;

    // reload finished?
    if (!w->ammo && w->reloadTimer > w->reloadTime) {
        w->ammo        = w->max_rounds;
        w->reloadTimer = 0;
        w->reloading=0;
    }
    if(w->ammo==0 && w->reloadTimer<=w->reloadTime){
        w->reloading=1;
    }
    // try to shoot
    if (want_fire && w->ammo && w->fireTimer > w->fireRate) {
        Vector2 mouse = GetMousePosition();
        Vector2 dir   = weapon_apply_spread(w, Vector2Normalize(Vector2Subtract(mouse, muzzle)));
        bullet_spawn(w->bullets, muzzle, dir);
        PlaySound(shooting_sound);
        w->ammo--;
        w->fireTimer = 0;
    }

    // update bullets
    for (int i = 0; i < BULLET_POOL; ++i)
        if (w->bullets[i].active) bullet_update(&w->bullets[i], dt);
}

static void weapon_draw(const Weapon *w) {
    for (int i = 0; i < BULLET_POOL; ++i)
        if (w->bullets[i].active) bullet_draw(&w->bullets[i]);
}
//PowerUps



typedef enum {
    HEAL,
    FIRE_RATE,
    RELOAD_TIME,
    SPEED,
    DAMAGE
}PowerUpType;

typedef enum{
    COMMON,
    UNCOMMON,
    RARE
}Rarity;

typedef struct {
    Vector2 pos;
    PowerUpType type;
    Rarity rarity;
    float health_factor;
    float fireRate_factor;
    float reloadTime_factor;
    float speed_factor;
    float damage_factor;
    Color color;
    int size;
    int active;
} PowerUp;
static void set_powerup(PowerUp* powerup){
    powerup->active = 1;
    int rarity = rand()%100;
    if(rarity<50) powerup->rarity=COMMON;
    else if(rarity>50&&rarity<80) powerup->rarity=UNCOMMON;
    else powerup->rarity=RARE;
    powerup->type = rand()%5;
    powerup->pos = (Vector2){GetRandomValue(0,GetScreenWidth()),GetRandomValue(0,GetScreenHeight())};
    switch(powerup->rarity){
        case COMMON:
            powerup->health_factor = 1.1f;
            powerup->fireRate_factor = 0.6f;
            powerup->reloadTime_factor = 0.9f;
            powerup->speed_factor = 1.1f;
            powerup->damage_factor = 1.1f;
            powerup->size = 10;
            break;
        case UNCOMMON:
            powerup->health_factor = 1.3;
            powerup->fireRate_factor = 0.4f;
            powerup->reloadTime_factor = 0.8f;
            powerup->speed_factor = 1.3f;
            powerup->damage_factor = 1.3f;
            powerup->size  = 15;
            break;
        case RARE:
            powerup->health_factor = 1.50f;
            powerup->fireRate_factor = 0.1f;
            powerup->reloadTime_factor = 0.5f;
            powerup->speed_factor = 1.5f;
            powerup->damage_factor = 1.50;
            powerup->size = 20;
            break;
        default:
            break;
    }
    switch(powerup->type){
        case HEAL:
            powerup->fireRate_factor = 1;
            powerup->reloadTime_factor = 1;
            powerup->speed_factor = 1;
            powerup->damage_factor = 1;
            powerup->color = RED;
            break;
        case FIRE_RATE:
            powerup->health_factor = 1;
            powerup->reloadTime_factor = 1;
            powerup->speed_factor = 1;
            powerup->damage_factor = 1;
            powerup->color = YELLOW;
            break;
        case RELOAD_TIME:
            powerup->health_factor = 1;
            powerup->fireRate_factor = 1;
            powerup->speed_factor = 1;
            powerup->damage_factor = 1;
            powerup->color = GREEN;
            break;
        case SPEED:
            powerup->health_factor = 1;
            powerup->fireRate_factor = 1;
            powerup->reloadTime_factor = 1;
            powerup->damage_factor = 1;
            powerup->color = BLUE;
            break;
        case DAMAGE:
            powerup->health_factor = 1;
            powerup->fireRate_factor = 1;
            powerup->reloadTime_factor = 1;
            powerup->speed_factor = 1;
            powerup->color = PINK;
            break;
        default:
            break;
    }

}

//--------------------------- player -------------------------
typedef struct {
    Vector2 pos, vel;
    Rectangle collider;
    float   speed;
    int   health;
    int max_health;
    Weapon  gun;
} Player;

static void player_init(Player *p) {
    *p = (Player){ .pos = {SCR_W/2.0f, SCR_H/2.0f},
                   .speed = 200.0f, .health = 100.0f,.max_health=100.0f,
                    .collider=(Rectangle){0,0,PLAYER_SIZE,PLAYER_SIZE} };
    weapon_init(&p->gun);
}

static void player_update(Player *p, float dt) {
    p->collider.x = p->pos.x; p->collider.y = p->pos.y;
    Vector2 dir = { 0 };
    if (IsKeyDown(KEY_W)) dir.y -= 1;
    if (IsKeyDown(KEY_S)) dir.y += 1;
    if (IsKeyDown(KEY_A)) dir.x -= 1;
    if (IsKeyDown(KEY_D)) dir.x += 1;

    dir = Vector2Normalize(dir);
    p->pos = v2_scale_add(p->pos, p->speed * dt, dir);

    weapon_update(&p->gun, p->pos, dt);
}

static void player_draw(const Player *p) {
    DrawRectangleV(
        (Vector2){ p->pos.x - PLAYER_SIZE/2, p->pos.y - PLAYER_SIZE/2 },
        (Vector2){ PLAYER_SIZE, PLAYER_SIZE },
        BLACK);
    weapon_draw(&p->gun);
}
static void player_limit_movement(Player *p) {
    p->pos = clamp_v2(p->pos,(Vector2){0,0},(Vector2){SCR_W,SCR_H});
}
//--------------------------- enemies ------------------------
typedef struct{
    Vector2 position;
    float radius;
}Circle;

typedef struct {
    bool     active;
    Vector2  pos, vel;
    Vector2  dir;
    float    speed;
    float    health;
    Rectangle collider;
    Circle    areaCollider;
    bool     self_colliding;
   
} Enemy;

typedef struct {
    Enemy    e[ENEMY_POOL];
    Vector2  spawner[SPAWN_POINTS];
    float    spawnTimer, spawnRate;
    int      alive;
    int      wave;
    int      max_per_wave;
    float    max_health;
    float    max_speed;
    
    float total_enemies;
    float waveTimer;
    float waveDelay;
    bool  wavePending;
    float    damage;

} EnemyManager;

static void enemy_manager_init(EnemyManager *em) {
    *em = (EnemyManager){
        .spawnRate     = 2.0f,
        .max_per_wave  = 4,
        .max_health    = 200.0f,
        .max_speed     = 100.0f,
        .total_enemies = 0.0f,
        .waveDelay     = 5.0f,
        .damage        = 10.0f
    };

    for (int i = 0; i < ENEMY_POOL; ++i)
        em->e[i].active = false;

    // 4 corners + mid‑edges
    em->spawner[0] = (Vector2){0,0};
    em->spawner[1] = (Vector2){SCR_W,0};
    em->spawner[2] = (Vector2){0,SCR_H};
    em->spawner[3] = (Vector2){SCR_W,SCR_H};
    em->spawner[4] = (Vector2){SCR_W/2,0};
    em->spawner[5] = (Vector2){SCR_W/2,SCR_H};
    em->spawner[6] = (Vector2){0,SCR_H/2};
    em->spawner[7] = (Vector2){SCR_W,SCR_H/2};
}

static void enemy_wave_next(EnemyManager *em) {
    em->wave++;
    em->waveTimer    = 0;
    em->wavePending  = false;
    em->spawnRate   *= expf(-0.04f * em->wave);
    em->max_health  *= expf(0.01f * em->wave);
    em->max_speed   *= expf(0.005f * em->wave);
    em->max_per_wave= (int)(em->max_per_wave * expf(0.05f * em->wave));
    em->total_enemies =0;
    em->alive = 0;
    for (int i = 0; i < ENEMY_POOL; ++i)
        em->e[i].active = false;
}
static void enemy_wave_update(EnemyManager *em, float dt) {
    if (em->alive == 0 && em->wavePending == false && em->total_enemies>=em->max_per_wave) {
        em->wavePending = true;
        em->waveTimer   = 0;
    }

    if (em->wavePending) {
        em->waveTimer += GetFrameTime();
        if (em->waveTimer >= em->waveDelay) {
            enemy_wave_next(em);
        }
    }
}



static void enemy_spawn(EnemyManager *em) {
    if (em->alive >= em->max_per_wave) return;
    if (em->total_enemies>=em->max_per_wave)return;
    for (int i = 0; i < ENEMY_POOL; ++i) {
        Enemy *e = &em->e[i];
        if (!e->active) {
            *e = (Enemy){
                .active  = true,
                .pos     = em->spawner[rand() % SPAWN_POINTS],
                .speed   = em->max_speed,
                .health  = em->max_health,
                .collider= {0,0,10,10}
            };
            em->alive++;
            em->total_enemies++;
            break;
        }
    }
}

static void enemy_update(Enemy *e, const Player *p, float dt) {
    // Vector2 dir = (Vector2){ 0,0 };
    if(!e->self_colliding){
        e->dir= Vector2Normalize(Vector2Subtract(p->pos, e->pos));
    }else{
        
    }
    e->pos      = v2_scale_add(e->pos, e->speed * dt, e->dir);
    e->collider.x = e->pos.x; e->collider.y = e->pos.y;
}
static void enemy_moveaway(Enemy *e1,Enemy *e2){
    e1->dir= Vector2Normalize(Vector2Subtract(e2->pos, e1->pos));
    e1->pos      = v2_scale_add(e1->pos, -e1->speed * GetFrameTime(), e1->dir);
    e1->collider.x = e1->pos.x; e1->collider.y = e1->pos.y;
}
static void enemy_selfcollide(Enemy *e1,Enemy *e2){
    if(CheckCollisionCircles(e1->pos,10,e2->pos,10)){
        e1->self_colliding=true;
        e2->self_colliding=true;
        enemy_moveaway(e1,e2);
    }else{
        e1->self_colliding=false;
        e2->self_colliding=false;
    }
}
static void attack(EnemyManager *e,int index,Player *p){
    if(!e->e[index].active)return;
    if(CheckCollisionRecs(p->collider,e->e[index].collider)){
        p->health-=e->damage;
        e->e[index].active=false;
        e->alive--;
        PlaySound(hit_sound);
    }
}


static void enemy_manager_update(EnemyManager *em, const Player *p, float dt) {
    // spawn logic
    em->spawnTimer += dt;
    if (em->spawnTimer > em->spawnRate) {
        em->spawnTimer = 0;
        enemy_spawn(em);
    }

    for (int i = 0; i < ENEMY_POOL; ++i) {
        Enemy *e = &em->e[i];
        if (!e->active) continue;
        attack(em,i,p);
        enemy_update(e, p, dt);

        // bullet collision
        for (int b = 0; b < BULLET_POOL; ++b) {
            Bullet *bul = &p->gun.bullets[b];
            if (bul->active &&
                CheckCollisionCircleRec(bul->pos, BULLET_RADIUS, e->collider)) {
                e->health -= p->gun.damage;
                PlaySound(hit_sound);
                bul->active = false;
            }
        }

        if (e->health <= 0) {
            e->active = false;
            em->alive--;
        }
    }
}

static void enemy_draw(const EnemyManager *em) {
    for (int i = 0; i < ENEMY_POOL; ++i)
        if (em->e[i].active)
            DrawRectangleRec(em->e[i].collider, GREEN);
}
static void draw_enemy_health(const EnemyManager *em,int index){
    Vector2 fsize=MeasureTextEx(GetFontDefault(), TextFormat("%d", (int)em->e[index].health), 5, 0);
    DrawText(TextFormat("%d", (int)em->e[index].health), em->e[index].pos.x-fsize.x*0.5f, em->e[index].pos.y-fsize.y-10, 5, BLACK);
}
float start_timer=0;
float start_time=3.0f;
int start_pressed=0;
int powerup_active=0;

static void pickup_powerup(PowerUp* powerup, Player* player){
    if(powerup->active){
         if(CheckCollisionCircleRec(powerup->pos, powerup->size, player->collider)){
            player->max_health*=(powerup->health_factor);
            player->speed*=(powerup->speed_factor);
            player->gun.fireRate*=(powerup->fireRate_factor);
            player->gun.damage*=(powerup->damage_factor);
            player->gun.reloadTime*=(powerup->reloadTime_factor);
            player->health=player->max_health;
            powerup->active=false;
            PlaySound(powerup_sound);
         }
    }
}
void draw_powerup(const PowerUp* powerup){
    char* type=" ";
    char* rarity=" ";
    Color color;
    switch(powerup->type){
        case HEAL:
            type="HEAL";
            break;
        case FIRE_RATE:
            type="FIRE_RATE";
            break;
        case RELOAD_TIME:
            type="RELOAD_TIME";
            break;
        case SPEED:
            type="SPEED";
            break;
        case DAMAGE:
            type="DAMAGE";
            break;
    }
    switch(powerup->rarity){
        case COMMON:
            rarity="COMMON";
            color=GRAY;
            break;
        case UNCOMMON:
            rarity="UNCOMMON";
            color=GREEN;
            break;
        case RARE:
            rarity="RARE";
            color=BLUE;
            break;
    }
    if(powerup->active){
        Vector2 fsize=MeasureTextEx(GetFontDefault(), TextFormat("%s (%s)",type,rarity), 5, 0);
        DrawText(TextFormat("%s (%s)",type,rarity),powerup->pos.x-powerup->size-fsize.x,powerup->pos.y-10-powerup->size,5,color);
    }
}
int gameover=0;

//--------------------------- game loop ----------------------
int main(void) {
    InitWindow(SCR_W, SCR_H, "Shooter");
    SetTargetFPS(60);
    InitAudioDevice();
    shooting_sound=LoadSound("../sound_effect/Shoot49.wav");
    hit_sound=LoadSound("../sound_effect/Hit24.wav");
    powerup_sound=LoadSound("../sound_effect/PowerUp.wav");
    death_sound=LoadSound("../sound_effect/Random2.wav");
    count_down_sound=LoadSound("../sound_effect/Pickup6.wav");
    srand((unsigned)time(NULL));
    enum Game game = START;
    Player       player;      player_init(&player);
    EnemyManager enemies;     enemy_manager_init(&enemies);
    PowerUp     powerup;
    float count_time=1;
    float count_timer=0;
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        switch (game)
        {
        case START:
            
            BeginDrawing();
            ClearBackground(RAYWHITE);
            int fw, fh;
            Vector2 fsize = MeasureTextEx(GetFontDefault(), "Shooter", 80, 0);
            DrawText("Shooter", (SCR_W-fsize.x)/2, 10+fsize.y, 80, BLACK); 
            EndDrawing();
            if(IsKeyPressed(KEY_ENTER)){
                start_pressed=1;
            }
            if(start_pressed){
                start_timer+=dt;
                count_timer+=dt;
                if(count_timer>=count_time){
                    PlaySound(count_down_sound);
                    //count_time+=1;
                    count_timer=0;
                }
                fsize=MeasureTextEx(GetFontDefault(), TextFormat("%d", (int)(start_time-start_timer)), 50, 0);
                DrawText(TextFormat("%d", (int)ceil((start_time-start_timer))), (SCR_W-fsize.x)/2, 300+fsize.y, 50, BLACK);
                
            }else{
                fsize=MeasureTextEx(GetFontDefault(), "Press enter to start", 20, 0);
                DrawText("Press enter to start", (SCR_W-fsize.x)/2, 300+fsize.y, 20, BLACK);
            }
            
            if(start_timer>=start_time){
                enemy_manager_init(&enemies);
                player_init(&player);
                start_pressed=0;
                game=PLAYING;
            }
            
            break;
        case PLAYING:
                //--- update
            pickup_powerup(&powerup,&player);
            player_update(&player, dt);
            enemy_manager_update(&enemies, &player, dt);
            enemy_wave_update(&enemies, dt);
            for(int i=0;i<ENEMY_POOL;i++){
                if(!enemies.e[i].active)continue;
                for(int j=i+1;j<ENEMY_POOL;j++){
                    if(!enemies.e[i].active||!enemies.e[j].active)continue;
                    enemy_selfcollide(&enemies.e[i],&enemies.e[j]);
                }
                draw_enemy_health(&enemies,i);
            }
            
            //--- draw
            BeginDrawing();
            ClearBackground(RAYWHITE);

            player_draw(&player);
            enemy_draw(&enemies);

            DrawText(TextFormat("Wave: %d", enemies.wave), 10, 10, 20, BLACK);
            DrawText(TextFormat("Enemies: %d", enemies.alive), 10, 40, 20, BLACK);
            DrawText(TextFormat("Max Wave Enemies: %d", enemies.max_per_wave), 
                    10, 70, 20, DARKGRAY);
            DrawText(TextFormat("total_kills: %d", (int)enemies.total_enemies-enemies.alive), 
                    10, 100, 20, DARKGRAY);
            DrawText(TextFormat("Enemy Spawn Time:%.2f",enemies.spawnRate),10,130,20,DARKGRAY);
            if(enemies.wavePending){
                if(powerup_active==0){
                    set_powerup(&powerup);
                    powerup_active=1;
                }
                
                DrawText(TextFormat("Next wave in: %.2f", enemies.wavePending ? enemies.waveDelay - enemies.waveTimer : 0), 
                    GetScreenWidth()/2 - MeasureText(TextFormat("Next wave in: %.2f", enemies.wavePending ? enemies.waveDelay - enemies.waveTimer : 0), 20)/2, 
                    10, 20, DARKGRAY);
            }else{
                powerup_active=0;
                powerup.active=0;
            }
            DrawText(TextFormat("Damage: %f", player.gun.damage), 600, 10, 20, BLACK);
            DrawText(TextFormat("Fire Rate: %.2f", player.gun.fireRate), 600, 40, 20, BLACK);
            DrawText(TextFormat("Reload Time: %.2f", player.gun.reloadTime), 600, 70, 20, BLACK);
            // DrawText(TextFormat("Bullet Speed: %.2f", player.gun.bulletSpeed), 600, 100, 20, BLACK);
            DrawText(TextFormat("Speed: %.2f", player.speed), 600, 100, 20, BLACK);
            DrawText(TextFormat("Health: %d", player.health), 600, 530, 20, BLACK);
            
            DrawText(TextFormat("%d/%d",player.gun.ammo,player.gun.max_rounds),10,530,20,DARKGRAY);
            if(player.gun.reloading==1){
                DrawText("Reloading",20,570,20,DARKPURPLE);
            }
            if(powerup.active){
                DrawCircle(powerup.pos.x,powerup.pos.y,powerup.size,powerup.color);
            }
            EndDrawing();
            if(player.health<=0){
                game=END;
            }
            if(IsKeyPressed(KEY_Q)){
                game=END;
            }
            if(powerup.active){
                draw_powerup(&powerup);
            }
            player_limit_movement(&player);
            break;
        case END:
            BeginDrawing();
            if(!gameover){
                PlaySound(death_sound);
                gameover=1;
            }
            
            ClearBackground(RAYWHITE);
            fsize=MeasureTextEx(GetFontDefault(), "Game Over", 50, 0);
            DrawText("Game Over", SCR_W/2-fsize.x/2, SCR_H/2-fsize.y/2, 50, BLACK);
            fsize=MeasureTextEx(GetFontDefault(), "Press Enter to play again", 50, 0);
            DrawText("Press Enter to play again", SCR_W/2-fsize.x/2, SCR_H/2+fsize.y/2, 50, BLACK);
            if(IsKeyPressed(KEY_ENTER)){
                start_timer=0;
                start_pressed=1;
                gameover=0;
                game=START;
                
            }
            EndDrawing();
            break;
        default:
            break;
        }
        
    }

    CloseWindow();
    return 0;
}
