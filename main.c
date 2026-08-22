#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#define CHUNK_SIZE 8
#define RENDER_DISTANCE 2 // Optimized render distance for mobile GPU efficiency
#define NUM_CHUNKS ((RENDER_DISTANCE * 2 + 1) * (RENDER_DISTANCE * 2 + 1))
#define BLOCK_SIZE 2.0f
#define MAX_ZOMBIES 60
#define MAX_COINS 80
#define MAX_ARROWS 50
#define MAX_BUILDINGS 60
#define MAX_CHESTS 3
#define TOTAL_WEAPONS 50
#define SAVE_FILE "hunter_android_save.dat"

typedef enum {
    STATE_MAIN_MENU = 0,
    STATE_GAMEPLAY,
    STATE_SETTINGS,
    STATE_PAUSED
} GameState;

typedef enum {
    RANK_E = 0,
    RANK_D,
    RANK_C,
    RANK_B,
    RANK_A,
    RANK_S,
    RANK_MONARCH
} HunterRank;

typedef enum {
    STYLE_BOW_RECURVE = 0,
    STYLE_CROSSBOW,
    STYLE_COMPOUND,
    STYLE_DAGGER_BOW,
    STYLE_DRAGON_BONE,
    STYLE_VOID_MONARCH,
    STYLE_CELESTIAL_RULER
} WeaponStyle;

typedef struct {
    int id;
    const char *name;
    HunterRank rank;
    WeaponStyle style;
    float damage;
    float fireRate;
    int pierce;
    float aoeRadius;
    Color primaryColor;
    Color secondaryColor;
    Color projectileColor;
    int unlockCost;
    bool unlocked;
} WeaponDef;

typedef enum {
    BIOME_PLAINS, BIOME_FOREST, BIOME_DESERT, BIOME_MOUNTAIN,
    BIOME_SWAMP, BIOME_VOLCANO, BIOME_TUNDRA, BIOME_GRAVEYARD,
    BIOME_MUSHROOM, BIOME_CAVERN
} BiomeType;

typedef enum {
    BUILD_NONE = 0,
    BUILD_WALL,        // 1: Palisade Wall (5g)
    BUILD_TOWER,       // 2: Watchtower (20g)
    BUILD_CAMPFIRE,    // 3: Healing Campfire (15g)
    BUILD_SPIKE_TRAP,  // 4: Spike Trap (10g)
    BUILD_GOLD_VAULT,  // 5: Coin Generator (30g)
    BUILD_TESLA_COIL,  // 6: Chain Lightning (40g)
    BUILD_FROST_TOTEM, // 7: Slow Aura (25g)
    BUILD_MORTAR,      // 8: Bomb Launcher (50g)
    BUILD_SHRINE,      // 9: Damage Boost Shrine (45g)
    BUILD_SPRINGBOARD  // 0: Knockback Trap (35g)
} BuildingType;

typedef struct {
    int height;
    BiomeType biome;
    bool hasVegetation;
    int vegSeed;
} Voxel;

typedef struct {
    int chunkX;
    int chunkZ;
    bool loaded;
    Voxel blocks[CHUNK_SIZE][CHUNK_SIZE];
} Chunk;

typedef struct {
    Vector3 position;
    BiomeType biomeType;
    float speed;
    float rotation;
    float health;
    float maxHealth;
    float walkTimer;
    bool isMoving;
    bool isBoss;
    float scale;
    bool active;
    float deathTimer;
    float slowTimer;
} Zombie3D;

typedef struct {
    Vector3 position;
    int value;
    bool active;
} Coin;

typedef struct {
    Vector3 position;
    Vector3 direction;
    float speed;
    float lifeTime;
    float damage;
    float aoeRadius;
    int pierceCount;
    int weaponId;
    Color color;
    bool active;
} Arrow;

typedef struct {
    Vector3 position;
    BuildingType type;
    int tier;
    float actionCooldown;
    float animTimer;
    bool active;
} Building;

typedef struct {
    Vector3 position;
    bool opened;
    bool active;
} TreasureChest;

typedef struct {
    Vector2 center;
    Vector2 knob;
    float radius;
    bool active;
    int touchId;
} VirtualJoystick;

typedef struct {
    int currentWave;
    int highestWave;
    int lifetimeKills;
    int totalGold;
    int goldCoins;
    int hunterLevel;
    int equippedWeaponId;
    bool unlockedWeapons[TOTAL_WEAPONS];
    bool hasSavedGame;
} GameSaveData;

// --- PERSISTENCE ---
void SaveGameData(GameSaveData data) {
    FILE *f = fopen(SAVE_FILE, "wb");
    if (f) {
        fwrite(&data, sizeof(GameSaveData), 1, f);
        fclose(f);
    }
}

GameSaveData LoadGameData(void) {
    GameSaveData data;
    data.currentWave = 1;
    data.highestWave = 1;
    data.lifetimeKills = 0;
    data.totalGold = 0;
    data.goldCoins = 60;
    data.hunterLevel = 1;
    data.equippedWeaponId = 0;
    data.hasSavedGame = false;
    for (int i = 0; i < TOTAL_WEAPONS; i++) {
        data.unlockedWeapons[i] = (i == 0);
    }

    FILE *f = fopen(SAVE_FILE, "rb");
    if (f) {
        fread(&data, sizeof(GameSaveData), 1, f);
        fclose(f);
    }
    return data;
}

// --- 50 WEAPONS ARSENAL ---
static WeaponDef weaponCatalog[TOTAL_WEAPONS] = {
    // E-Rank (0-7)
    { 0, "Novice Oak Bow",           RANK_E, STYLE_BOW_RECURVE, 25.0f,  0.42f, 1, 0.0f, (Color){ 130, 85, 45, 255 },  (Color){ 90, 55, 25, 255 },  (Color){ 180, 180, 180, 255 }, 0,     true  },
    { 1, "Rusted Iron Crossbow",     RANK_E, STYLE_CROSSBOW,    32.0f,  0.48f, 1, 0.0f, (Color){ 110, 105, 100, 255 }, (Color){ 80, 50, 30, 255 },   (Color){ 140, 140, 140, 255 }, 20,    false },
    { 2, "Recruit's Shortbow",       RANK_E, STYLE_BOW_RECURVE, 28.0f,  0.38f, 1, 0.0f, (Color){ 160, 110, 60, 255 },  (Color){ 70, 40, 20, 255 },  (Color){ 190, 160, 130, 255 }, 35,    false },
    { 3, "Goblin Scout Dart",        RANK_E, STYLE_COMPOUND,    22.0f,  0.30f, 1, 0.0f, (Color){ 80, 120, 60, 255 },   (Color){ 50, 80, 40, 255 },   (Color){ 110, 170, 90,  255 }, 50,    false },
    { 4, "Worn Bone Bow",            RANK_E, STYLE_DRAGON_BONE, 35.0f,  0.50f, 1, 0.0f, (Color){ 210, 205, 190, 255 }, (Color){ 140, 130, 110, 255 }, (Color){ 220, 220, 200, 255 }, 65,    false },
    { 5, "Crude Flint Bow",          RANK_E, STYLE_BOW_RECURVE, 30.0f,  0.40f, 1, 0.0f, (Color){ 90, 95, 105, 255 },   (Color){ 60, 65, 75, 255 },   (Color){ 100, 105, 115, 255 }, 80,    false },
    { 6, "Apprentice Wand Bow",      RANK_E, STYLE_BOW_RECURVE, 34.0f,  0.42f, 1, 0.0f, (Color){ 100, 160, 200, 255 }, (Color){ 60, 100, 150, 255 }, (Color){ 130, 200, 220, 255 }, 100,   false },
    { 7, "Wolf Tooth Ballista",      RANK_E, STYLE_CROSSBOW,    40.0f,  0.55f, 1, 0.0f, (Color){ 170, 150, 130, 255 }, (Color){ 110, 90, 70, 255 },   (Color){ 200, 190, 170, 255 }, 120,   false },

    // D-Rank (8-15)
    { 8, "Steel Compound Bow",       RANK_D, STYLE_COMPOUND,    45.0f,  0.38f, 1, 0.0f, (Color){ 70, 140, 210, 255 },  (Color){ 40, 70, 110, 255 },  (Color){ 80,  170, 255, 255 }, 150,   false },
    { 9, "Viper Fang Longbow",       RANK_D, STYLE_BOW_RECURVE, 42.0f,  0.34f, 2, 0.0f, (Color){ 40, 180, 80, 255 },   (Color){ 20, 90, 40, 255 },   (Color){ 50,  220, 100, 255 }, 180,   false },
    { 10, "Cursed Miner's Bolt",     RANK_D, STYLE_CROSSBOW,    52.0f,  0.45f, 1, 0.0f, (Color){ 120, 70, 160, 255 },  (Color){ 60, 30, 90, 255 },   (Color){ 140, 100, 180, 255 }, 220,   false },
    { 11, "Twin-String Hunter",      RANK_D, STYLE_COMPOUND,    38.0f,  0.28f, 1, 0.0f, (Color){ 220, 140, 40, 255 },  (Color){ 140, 70, 20, 255 },  (Color){ 255, 180, 60,  255 }, 260,   false },
    { 12, "Razor Iron Piercer",      RANK_D, STYLE_BOW_RECURVE, 55.0f,  0.44f, 2, 0.0f, (Color){ 160, 180, 200, 255 }, (Color){ 100, 115, 130, 255 }, (Color){ 190, 210, 230, 255 }, 300,   false },
    { 13, "Swamp Stalker Bow",       RANK_D, STYLE_BOW_RECURVE, 48.0f,  0.36f, 1, 0.0f, (Color){ 70, 120, 50, 255 },   (Color){ 40, 70, 30, 255 },   (Color){ 90,  150, 70,  255 }, 340,   false },
    { 14, "Obsidian Shard Thrower",  RANK_D, STYLE_CROSSBOW,    58.0f,  0.48f, 1, 1.2f, (Color){ 45, 40, 50, 255 },    (Color){ 25, 20, 30, 255 },   (Color){ 80,  70,  90,  255 }, 380,   false },
    { 15, "Dungeon Raider Crossbow", RANK_D, STYLE_CROSSBOW,    50.0f,  0.35f, 2, 0.0f, (Color){ 180, 120, 70, 255 },  (Color){ 90, 55, 30, 255 },   (Color){ 210, 150, 100, 255 }, 420,   false },

    // C-Rank (16-23)
    { 16, "Frostbite Longbow",       RANK_C, STYLE_BOW_RECURVE, 70.0f,  0.36f, 2, 0.0f, (Color){ 0, 190, 240, 255 },   (Color){ 0, 100, 160, 255 },  (Color){ 0,   210, 255, 255 }, 500,   false },
    { 17, "Hellhound Ember Bow",     RANK_C, STYLE_DRAGON_BONE, 78.0f,  0.40f, 1, 2.2f, (Color){ 230, 60, 20, 255 },   (Color){ 130, 20, 10, 255 },  (Color){ 255, 90,  30,  255 }, 560,   false },
    { 18, "Shadow Assassin Bow",     RANK_C, STYLE_DAGGER_BOW,  68.0f,  0.28f, 2, 0.0f, (Color){ 70, 30, 110, 255 },   (Color){ 30, 10, 60, 255 },   (Color){ 90,  40,  140, 255 }, 620,   false },
    { 19, "Glacier Spike Launcher",  RANK_C, STYLE_CROSSBOW,    85.0f,  0.45f, 2, 0.0f, (Color){ 140, 210, 240, 255 }, (Color){ 60, 120, 160, 255 }, (Color){ 170, 240, 255, 255 }, 680,   false },
    { 20, "Thunderstrike Recurve",   RANK_C, STYLE_BOW_RECURVE, 74.0f,  0.32f, 3, 0.0f, (Color){ 240, 210, 40, 255 },  (Color){ 140, 110, 10, 255 }, (Color){ 255, 230, 60,  255 }, 750,   false },
    { 21, "Venomous Hydra String",   RANK_C, STYLE_COMPOUND,    65.0f,  0.26f, 2, 1.4f, (Color){ 30, 210, 90, 255 },   (Color){ 10, 110, 40, 255 },  (Color){ 40,  240, 120, 255 }, 820,   false },
    { 22, "Magma Core Crossbow",     RANK_C, STYLE_CROSSBOW,    88.0f,  0.46f, 1, 2.5f, (Color){ 200, 50, 10, 255 },   (Color){ 100, 20, 5, 255 },   (Color){ 230, 70,  20,  255 }, 900,   false },
    { 23, "Knight-Captain Greatbow", RANK_C, STYLE_BOW_RECURVE, 80.0f,  0.35f, 2, 0.0f, (Color){ 200, 170, 90, 255 },  (Color){ 120, 90, 40, 255 },  (Color){ 220, 190, 120, 255 }, 980,   false },

    // B-Rank (24-31)
    { 24, "Bloodlust Piercer",       RANK_B, STYLE_DAGGER_BOW,  110.0f, 0.32f, 3, 0.0f, (Color){ 190, 20, 40, 255 },   (Color){ 90, 5, 15, 255 },    (Color){ 220, 30,  50,  255 }, 1100,  false },
    { 25, "Gale Wind Swiftbow",      RANK_B, STYLE_BOW_RECURVE, 95.0f,  0.22f, 2, 0.0f, (Color){ 90, 220, 150, 255 },  (Color){ 40, 130, 80, 255 },  (Color){ 120, 255, 180, 255 }, 1250,  false },
    { 26, "Titan Bone Ballista",     RANK_B, STYLE_DRAGON_BONE, 140.0f, 0.48f, 3, 2.0f, (Color){ 220, 210, 190, 255 }, (Color){ 140, 130, 110, 255 }, (Color){ 240, 230, 210, 255 }, 1400,  false },
    { 27, "Abyssal Shadow Bow",      RANK_B, STYLE_VOID_MONARCH,115.0f, 0.28f, 3, 0.0f, (Color){ 40, 15, 65, 255 },    (Color){ 15, 5, 30, 255 },    (Color){ 70,  30,  110, 255 }, 1550,  false },
    { 28, "Volcanic Dragon Tail",    RANK_B, STYLE_DRAGON_BONE, 130.0f, 0.38f, 2, 3.2f, (Color){ 230, 90, 0, 255 },    (Color){ 130, 40, 0, 255 },   (Color){ 255, 110, 0,   255 }, 1700,  false },
    { 29, "Phantom Spectre Bow",     RANK_B, STYLE_COMPOUND,    105.0f, 0.25f, 4, 0.0f, (Color){ 150, 200, 240, 255 }, (Color){ 70, 110, 150, 255 }, (Color){ 180, 230, 255, 255 }, 1900,  false },
    { 30, "Celestial Arc Recurve",   RANK_B, STYLE_BOW_RECURVE, 120.0f, 0.30f, 3, 1.8f, (Color){ 230, 190, 50, 255 },  (Color){ 140, 100, 20, 255 }, (Color){ 255, 215, 80,  255 }, 2100,  false },
    { 31, "Necromancer Shooter",     RANK_B, STYLE_CROSSBOW,    125.0f, 0.33f, 3, 2.2f, (Color){ 110, 210, 130, 255 }, (Color){ 50, 110, 60, 255 },   (Color){ 140, 240, 160, 255 }, 2300,  false },

    // A-Rank (32-38)
    { 32, "Baruka's Dagger Bow",     RANK_A, STYLE_DAGGER_BOW,  175.0f, 0.25f, 4, 0.0f, (Color){ 0, 200, 180, 255 },   (Color){ 0, 90, 80, 255 },    (Color){ 0,   230, 200, 255 }, 2600,  false },
    { 33, "High Orc Greatbow",       RANK_A, STYLE_DRAGON_BONE, 210.0f, 0.35f, 3, 3.5f, (Color){ 170, 30, 15, 255 },   (Color){ 90, 10, 5, 255 },    (Color){ 190, 40,  20,  255 }, 3000,  false },
    { 34, "Kaisel's Thunder Flare",  RANK_A, STYLE_DRAGON_BONE, 190.0f, 0.26f, 4, 2.8f, (Color){ 90, 130, 240, 255 },  (Color){ 40, 60, 140, 255 },  (Color){ 120, 160, 255, 255 }, 3400,  false },
    { 35, "Tusk's Hymn of Fire",     RANK_A, STYLE_VOID_MONARCH,230.0f, 0.40f, 2, 4.5f, (Color){ 230, 40, 5, 255 },    (Color){ 120, 10, 0, 255 },   (Color){ 255, 50,  10,  255 }, 3900,  false },
    { 36, "Red Gate Blizzard Core",  RANK_A, STYLE_BOW_RECURVE, 185.0f, 0.28f, 5, 2.0f, (Color){ 180, 225, 245, 255 }, (Color){ 90, 140, 170, 255 }, (Color){ 210, 245, 255, 255 }, 4400,  false },
    { 37, "Demon King's Twin Bow",   RANK_A, STYLE_DAGGER_BOW,  200.0f, 0.24f, 4, 3.0f, (Color){ 140, 30, 210, 255 },  (Color){ 70, 10, 110, 255 },  (Color){ 160, 40,  240, 255 }, 5000,  false },
    { 38, "Shadow Commander Cross",  RANK_A, STYLE_CROSSBOW,    220.0f, 0.30f, 4, 3.2f, (Color){ 55, 15, 110, 255 },   (Color){ 25, 5, 60, 255 },    (Color){ 70,  20,  140, 255 }, 5600,  false },

    // S-Rank (39-44)
    { 39, "Kamish's Wrath Bow",      RANK_S, STYLE_DRAGON_BONE, 320.0f, 0.22f, 6, 4.0f, (Color){ 230, 30, 30, 255 },   (Color){ 120, 10, 10, 255 },  (Color){ 255, 40,  40,  255 }, 6500,  false },
    { 40, "Beru's Predatory Stinger",RANK_S, STYLE_DAGGER_BOW,  290.0f, 0.16f, 5, 2.5f, (Color){ 120, 20, 160, 255 },  (Color){ 50, 5, 80, 255 },    (Color){ 140, 30,  190, 255 }, 7500,  false },
    { 41, "Demon King Baran's Light",RANK_S, STYLE_COMPOUND,    340.0f, 0.24f, 6, 4.5f, (Color){ 80, 180, 230, 255 },  (Color){ 30, 90, 140, 255 },  (Color){ 100, 210, 255, 255 }, 8600,  false },
    { 42, "Igris's Blood Longbow",   RANK_S, STYLE_BOW_RECURVE, 310.0f, 0.19f, 5, 3.0f, (Color){ 180, 5, 20, 255 },    (Color){ 90, 0, 5, 255 },     (Color){ 210, 10,  30,  255 }, 9800,  false },
    { 43, "Bellion's Void Greatbow", RANK_S, STYLE_VOID_MONARCH,370.0f, 0.26f, 6, 5.0f, (Color){ 35, 10, 60, 255 },    (Color){ 15, 0, 30, 255 },    (Color){ 60,  20,  100, 255 }, 11200, false },
    { 44, "Architect Runic Ballista",RANK_S, STYLE_CROSSBOW,    350.0f, 0.22f, 6, 4.8f, (Color){ 230, 190, 0, 255 },   (Color){ 120, 95, 0, 255 },   (Color){ 255, 215, 0,   255 }, 12800, false },

    // Monarch & National Level (45-49)
    { 45, "Monarch of Shadows Void", RANK_MONARCH, STYLE_VOID_MONARCH,   520.0f, 0.15f, 8,  5.5f, (Color){ 90, 20, 200, 255 }, (Color){ 30, 5, 80, 255 }, (Color){ 110, 30,  230, 255 }, 15000, false },
    { 46, "Dragon Monarch Breath",   RANK_MONARCH, STYLE_DRAGON_BONE,    580.0f, 0.18f, 7,  6.5f, (Color){ 230, 30, 0, 255 },  (Color){ 110, 10, 0, 255 }, (Color){ 255, 40,  0,   255 }, 18000, false },
    { 47, "Frost Monarch Zero Bow",  RANK_MONARCH, STYLE_BOW_RECURVE,    490.0f, 0.14f, 9,  5.0f, (Color){ 160, 220, 245, 255 }, (Color){ 80, 140, 180, 255 }, (Color){ 180, 240, 255, 255 }, 22000, false },
    { 48, "Beast Monarch Fang Bow",  RANK_MONARCH, STYLE_DAGGER_BOW,     550.0f, 0.16f, 8,  6.0f, (Color){ 200, 120, 15, 255 }, (Color){ 100, 55, 5, 255 }, (Color){ 230, 140, 20,  255 }, 26000, false },
    { 49, "National: Ruler Authority",RANK_MONARCH, STYLE_CELESTIAL_RULER,750.0f, 0.12f, 12, 7.5f, (Color){ 255, 215, 80, 255 }, (Color){ 160, 120, 20, 255 }, (Color){ 255, 235, 120, 255 }, 35000, false }
};

const char* GetRankLabel(HunterRank rank, Color *outColor) {
    switch (rank) {
        case RANK_E: if (outColor) *outColor = (Color){ 170, 170, 170, 255 }; return "E-RANK";
        case RANK_D: if (outColor) *outColor = (Color){ 70,  180, 240, 255 }; return "D-RANK";
        case RANK_C: if (outColor) *outColor = (Color){ 80,  220, 100, 255 }; return "C-RANK";
        case RANK_B: if (outColor) *outColor = (Color){ 200, 130, 255, 255 }; return "B-RANK";
        case RANK_A: if (outColor) *outColor = (Color){ 255, 140, 30,  255 }; return "A-RANK";
        case RANK_S: if (outColor) *outColor = (Color){ 240, 40,  50,  255 }; return "S-RANK";
        case RANK_MONARCH: if (outColor) *outColor = (Color){ 255, 215, 0, 255 }; return "MONARCH";
        default: return "E-RANK";
    }
}

// Procedural Terrain Noise
static int Hash2D_Chunk(int x, int z, int seed) {
    int n = x * 374761393 + z * 668265263 + seed;
    n = (n ^ (n >> 13)) * 1274126177;
    return n ^ (n >> 16);
}

static float PseudoNoise_Chunk(int x, int z, int seed) {
    return (float)(Hash2D_Chunk(x, z, seed) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float SmoothNoise_Chunk(float x, float z, int seed) {
    int ix = (int)floorf(x);
    int iz = (int)floorf(z);
    float fx = x - ix;
    float fz = z - iz;

    float sx = fx * fx * (3.0f - 2.0f * fx);
    float sz = fz * fz * (3.0f - 2.0f * fz);

    float n00 = PseudoNoise_Chunk(ix, iz, seed);
    float n10 = PseudoNoise_Chunk(ix + 1, iz, seed);
    float n01 = PseudoNoise_Chunk(ix, iz + 1, seed);
    float n11 = PseudoNoise_Chunk(ix + 1, iz + 1, seed);

    return Lerp(Lerp(n00, n10, sx), Lerp(n01, n11, sx), sz);
}

int GetElevationAt(int worldBlockX, int worldBlockZ, int seed, BiomeType *outBiome) {
    float elevation = SmoothNoise_Chunk(worldBlockX * 0.025f, worldBlockZ * 0.025f, seed);
    float temp      = SmoothNoise_Chunk(worldBlockX * 0.012f, worldBlockZ * 0.012f, seed + 100);
    float moist     = SmoothNoise_Chunk(worldBlockX * 0.012f, worldBlockZ * 0.012f, seed + 200);

    BiomeType b = BIOME_PLAINS;
    int height = 1;

    if (elevation > 0.70f) {
        b = BIOME_MOUNTAIN;
        height = (int)((elevation - 0.70f) * 18.0f) + 3;
    } else if (elevation < 0.22f) {
        b = BIOME_CAVERN;
        height = 0;
    } else {
        height = (int)(elevation * 2.8f) + 1;
        if (temp > 0.68f) b = (moist > 0.50f) ? BIOME_VOLCANO : BIOME_DESERT;
        else if (temp < 0.32f) b = (moist > 0.50f) ? BIOME_TUNDRA : BIOME_GRAVEYARD;
        else {
            if (moist > 0.68f) b = BIOME_SWAMP;
            else if (moist > 0.52f) b = BIOME_MUSHROOM;
            else if (moist > 0.35f) b = BIOME_FOREST;
            else b = BIOME_PLAINS;
        }
    }

    if (outBiome) *outBiome = b;
    return height;
}

void GenerateChunk(Chunk *chunk, int cx, int cz, int seed) {
    chunk->chunkX = cx;
    chunk->chunkZ = cz;
    chunk->loaded = true;

    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            int wx = cx * CHUNK_SIZE + lx;
            int wz = cz * CHUNK_SIZE + lz;
            BiomeType b;
            chunk->blocks[lz][lx].height = GetElevationAt(wx, wz, seed, &b);
            chunk->blocks[lz][lx].biome = b;

            if (abs(wx) <= 3 && abs(wz) <= 3) {
                chunk->blocks[lz][lx].hasVegetation = false;
                continue;
            }

            float vegChance = PseudoNoise_Chunk(wx, wz, seed + 333);
            chunk->blocks[lz][lx].vegSeed = Hash2D_Chunk(wx, wz, seed + 999);
            float threshold = (b == BIOME_FOREST) ? 0.88f : (b == BIOME_PLAINS) ? 0.92f : 0.95f;
            chunk->blocks[lz][lx].hasVegetation = (vegChance > threshold);
        }
    }
}

void UpdateChunkManager(Chunk loadedChunks[], int centerChunkX, int centerChunkZ, int seed) {
    bool needed[NUM_CHUNKS] = { 0 };

    for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; dz++) {
        for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; dx++) {
            int targetCx = centerChunkX + dx;
            int targetCz = centerChunkZ + dz;

            bool isLoaded = false;
            for (int i = 0; i < NUM_CHUNKS; i++) {
                if (loadedChunks[i].loaded && loadedChunks[i].chunkX == targetCx && loadedChunks[i].chunkZ == targetCz) {
                    needed[i] = true;
                    isLoaded = true;
                    break;
                }
            }

            if (!isLoaded) {
                for (int i = 0; i < NUM_CHUNKS; i++) {
                    if (!needed[i]) {
                        GenerateChunk(&loadedChunks[i], targetCx, targetCz, seed);
                        needed[i] = true;
                        break;
                    }
                }
            }
        }
    }
}

float GetWorldHeight(float worldX, float worldZ, int seed) {
    int bx = (int)floorf(worldX / BLOCK_SIZE);
    int bz = (int)floorf(worldZ / BLOCK_SIZE);
    return GetElevationAt(bx, bz, seed, NULL) * 1.0f;
}

// 3D Weapon Model Engine
void DrawWeapon3D(WeaponDef *wep, float attackProgress, float globalTime) {
    float drawBack = sinf(attackProgress * PI) * 0.35f;
    float recoil = sinf(attackProgress * PI) * 0.25f;

    switch (wep->style) {
        case STYLE_BOW_RECURVE:
            DrawCube((Vector3){ 0.0f, 0.0f, 0.22f - recoil }, 0.08f, 1.4f, 0.08f, wep->primaryColor);
            DrawCube((Vector3){ 0.0f, 0.65f, 0.12f - drawBack }, 0.07f, 0.25f, 0.15f, wep->secondaryColor);
            DrawCube((Vector3){ 0.0f, -0.65f, 0.12f - drawBack }, 0.07f, 0.25f, 0.15f, wep->secondaryColor);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.02f - drawBack }, 0.02f, 1.3f, 0.02f, WHITE);
            break;

        case STYLE_CROSSBOW:
            DrawCube((Vector3){ 0.0f, 0.0f, 0.35f - recoil }, 0.12f, 0.12f, 0.9f, wep->secondaryColor);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.65f - recoil }, 1.1f, 0.08f, 0.12f, wep->primaryColor);
            DrawCube((Vector3){ 0.0f, 0.08f, 0.35f - recoil }, 0.06f, 0.06f, 0.7f, wep->projectileColor);
            break;

        case STYLE_COMPOUND:
            DrawCube((Vector3){ 0.0f, 0.0f, 0.25f - recoil }, 0.09f, 1.2f, 0.09f, wep->primaryColor);
            DrawCylinder((Vector3){ 0.0f, 0.6f, 0.15f }, 0.12f, 0.12f, 0.04f, 8, wep->secondaryColor);
            DrawCylinder((Vector3){ 0.0f, -0.6f, 0.15f }, 0.12f, 0.12f, 0.04f, 8, wep->secondaryColor);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.05f - drawBack }, 0.02f, 1.2f, 0.02f, (Color){ 255, 230, 100, 255 });
            break;

        case STYLE_DAGGER_BOW:
            DrawCube((Vector3){ 0.0f, 0.45f, 0.2f - drawBack }, 0.06f, 0.7f, 0.22f, wep->primaryColor);
            DrawCube((Vector3){ 0.0f, -0.45f, 0.2f - drawBack }, 0.06f, 0.7f, 0.22f, wep->primaryColor);
            DrawSphere((Vector3){ 0.0f, 0.0f, 0.2f }, 0.14f, wep->secondaryColor);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.04f - drawBack }, 0.02f, 1.1f, 0.02f, wep->projectileColor);
            break;

        case STYLE_DRAGON_BONE:
            DrawCube((Vector3){ 0.0f, 0.0f, 0.28f - recoil }, 0.14f, 1.6f, 0.14f, wep->primaryColor);
            for (float dy = -0.5f; dy <= 0.5f; dy += 0.25f) {
                DrawCube((Vector3){ 0.0f, dy, 0.38f - recoil }, 0.08f, 0.08f, 0.2f, wep->secondaryColor);
            }
            DrawCube((Vector3){ 0.0f, 0.0f, 0.06f - drawBack }, 0.03f, 1.5f, 0.03f, (Color){ 255, 80, 20, 255 });
            break;

        case STYLE_VOID_MONARCH:
            {
                float floatY = sinf(globalTime * 6.0f) * 0.1f;
                DrawSphere((Vector3){ 0.0f, floatY, 0.35f }, 0.24f, wep->secondaryColor);
                DrawCube((Vector3){ 0.0f, 0.6f + floatY, 0.25f - drawBack }, 0.1f, 0.8f, 0.16f, wep->primaryColor);
                DrawCube((Vector3){ 0.0f, -0.6f + floatY, 0.25f - drawBack }, 0.1f, 0.8f, 0.16f, wep->primaryColor);
                DrawSphereWires((Vector3){ 0.0f, floatY, 0.35f }, 0.35f, 6, 6, wep->projectileColor);
            }
            break;

        case STYLE_CELESTIAL_RULER:
            {
                float spin = globalTime * 45.0f;
                DrawCube((Vector3){ 0.0f, 0.0f, 0.35f - recoil }, 0.15f, 1.8f, 0.15f, wep->primaryColor);
                DrawCube((Vector3){ 0.0f, 0.75f, 0.18f - drawBack }, 0.45f, 0.45f, 0.12f, wep->secondaryColor);
                DrawCube((Vector3){ 0.0f, -0.75f, 0.18f - drawBack }, 0.45f, 0.45f, 0.12f, wep->secondaryColor);

                rlPushMatrix();
                    rlTranslatef(0.0f, 0.0f, 0.35f);
                    rlRotatef(spin, 0.0f, 0.0f, 1.0f);
                    DrawCircle3D((Vector3){ 0, 0, 0 }, 0.6f, (Vector3){ 0, 0, 1 }, 0.0f, GOLD);
                rlPopMatrix();
            }
            break;
    }
}

void DrawBiomeVegetation(Vector3 basePos, BiomeType biome, int seed) {
    float baseY = basePos.y;
    switch (biome) {
        case BIOME_PLAINS:
            DrawCube((Vector3){ basePos.x, baseY + 1.25f, basePos.z }, 0.45f, 2.5f, 0.45f, (Color){ 110, 65, 30, 255 });
            DrawCube((Vector3){ basePos.x, baseY + 3.0f, basePos.z }, 2.4f, 1.8f, 2.4f, (Color){ 45, 140, 40, 255 });
            DrawCube((Vector3){ basePos.x, baseY + 4.1f, basePos.z }, 1.6f, 1.2f, 1.6f, (Color){ 60, 165, 50, 255 });
            break;
        case BIOME_FOREST:
            DrawCube((Vector3){ basePos.x, baseY + 1.75f, basePos.z }, 0.5f, 3.5f, 0.5f, (Color){ 85, 50, 25, 255 });
            DrawCube((Vector3){ basePos.x, baseY + 3.8f, basePos.z }, 2.8f, 2.2f, 2.8f, (Color){ 30, 105, 35, 255 });
            DrawCube((Vector3){ basePos.x, baseY + 5.0f, basePos.z }, 1.8f, 1.4f, 1.8f, (Color){ 40, 130, 45, 255 });
            break;
        case BIOME_DESERT:
            DrawCube((Vector3){ basePos.x, baseY + 1.6f, basePos.z }, 0.5f, 3.2f, 0.5f, (Color){ 70, 135, 55, 255 });
            DrawCube((Vector3){ basePos.x + 0.55f, baseY + 1.8f, basePos.z }, 0.7f, 0.35f, 0.35f, (Color){ 70, 135, 55, 255 });
            break;
        case BIOME_MOUNTAIN:
            DrawCube((Vector3){ basePos.x, baseY + 0.8f, basePos.z }, 0.4f, 1.6f, 0.4f, (Color){ 90, 60, 35, 255 });
            DrawCylinder((Vector3){ basePos.x, baseY + 2.0f, basePos.z }, 0.0f, 1.6f, 1.8f, 6, (Color){ 40, 85, 60, 255 });
            break;
        default:
            DrawCube((Vector3){ basePos.x, baseY + 0.8f, basePos.z }, 0.6f, 1.6f, 0.6f, (Color){ 80, 80, 90, 255 });
            break;
    }
}

void DrawHunter(Vector3 pos, float rotAngle, float walkTime, bool isMoving, HunterRank rank, WeaponDef *wep, float attackProgress, float globalTime) {
    float legSwing = isMoving ? sinf(walkTime * 10.0f) * 0.4f : 0.0f;
    Color tunicColor = (rank == RANK_MONARCH) ? (Color){ 25, 15, 40, 255 } :
                       (rank >= RANK_A) ? (Color){ 30, 35, 55, 255 } : (Color){ 35, 120, 50, 255 };

    rlPushMatrix();
        rlTranslatef(pos.x, pos.y, pos.z);
        rlRotatef(rotAngle, 0.0f, 1.0f, 0.0f);

        DrawCube((Vector3){ -0.2f, 0.45f, legSwing * 0.3f }, 0.22f, 0.9f, 0.22f, (Color){ 35, 40, 45, 255 });
        DrawCube((Vector3){ 0.2f, 0.45f, -legSwing * 0.3f }, 0.22f, 0.9f, 0.22f, (Color){ 35, 40, 45, 255 });

        DrawCube((Vector3){ 0.0f, 1.35f, 0.0f }, 0.7f, 0.9f, 0.4f, tunicColor);
        DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 0.72f, 0.15f, 0.42f, (Color){ 20, 20, 25, 255 });

        DrawCube((Vector3){ 0.0f, 1.45f, -0.25f }, 0.22f, 0.7f, 0.18f, (Color){ 25, 25, 30, 255 });
        DrawCube((Vector3){ 0.0f, 1.85f, -0.25f }, 0.15f, 0.2f, 0.12f, wep->projectileColor);

        DrawCube((Vector3){ 0.0f, 2.1f, 0.0f }, 0.46f, 0.46f, 0.46f, (Color){ 245, 200, 160, 255 });
        DrawCube((Vector3){ 0.0f, 2.38f, -0.04f }, 0.5f, 0.16f, 0.52f, (Color){ 25, 25, 35, 255 });

        DrawCube((Vector3){ -0.12f, 2.12f, 0.24f }, 0.09f, 0.07f, 0.02f, WHITE);
        DrawCube((Vector3){ 0.12f, 2.12f, 0.24f }, 0.09f, 0.07f, 0.02f, WHITE);
        DrawCube((Vector3){ -0.12f, 2.12f, 0.25f }, 0.05f, 0.05f, 0.02f, (Color){ 0, 200, 255, 255 });
        DrawCube((Vector3){ 0.12f, 2.12f, 0.25f }, 0.05f, 0.05f, 0.02f, (Color){ 0, 200, 255, 255 });

        rlPushMatrix();
            rlTranslatef(-0.48f, 1.35f, 0.15f);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.18f, 0.75f, 0.18f, (Color){ 245, 200, 160, 255 });
            DrawWeapon3D(wep, attackProgress, globalTime);
        rlPopMatrix();

        rlPushMatrix();
            float pullZ = -0.2f * sinf(attackProgress * PI);
            rlTranslatef(0.48f, 1.35f, pullZ - legSwing * 0.3f);
            DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 0.18f, 0.75f, 0.18f, (Color){ 245, 200, 160, 255 });
        rlPopMatrix();

    rlPopMatrix();
}

void DrawThemedZombie(Zombie3D *z) {
    float legSwing = z->isMoving ? sinf(z->walkTimer * 10.0f) * 0.4f : 0.0f;
    Color skinColor = (Color){ 90, 140, 80, 255 };
    Color shirtColor = (Color){ 140, 30, 30, 255 };
    Color eyeColor = RED;

    if (z->isBoss) {
        skinColor = (Color){ 20, 20, 25, 255 };
        shirtColor = (Color){ 180, 20, 40, 255 };
        eyeColor = (Color){ 255, 215, 0, 255 };
    }

    rlPushMatrix();
        rlTranslatef(z->position.x, z->position.y, z->position.z);
        rlRotatef(z->rotation, 0.0f, 1.0f, 0.0f);
        rlScalef(z->scale, z->scale, z->scale);

        if (z->deathTimer > 0.0f) {
            float fallAngle = z->deathTimer * 90.0f;
            if (fallAngle > 90.0f) fallAngle = 90.0f;
            rlTranslatef(0.0f, 0.2f * (1.0f - z->deathTimer), -z->deathTimer * 0.8f);
            rlRotatef(fallAngle, -1.0f, 0.0f, 0.0f);
        }

        DrawCube((Vector3){ -0.2f, 0.45f, legSwing * 0.3f }, 0.22f, 0.9f, 0.22f, (Color){ 40, 40, 45, 255 });
        DrawCube((Vector3){ 0.2f, 0.45f, -legSwing * 0.3f }, 0.22f, 0.9f, 0.22f, (Color){ 40, 40, 45, 255 });
        DrawCube((Vector3){ 0.0f, 1.35f, 0.0f }, 0.7f, 0.9f, 0.4f, shirtColor);
        DrawCube((Vector3){ 0.0f, 2.1f, 0.0f }, 0.45f, 0.45f, 0.45f, skinColor);
        DrawCube((Vector3){ -0.12f, 2.12f, 0.23f }, 0.07f, 0.07f, 0.02f, eyeColor);
        DrawCube((Vector3){ 0.12f, 2.12f, 0.23f }, 0.07f, 0.07f, 0.02f, eyeColor);
        DrawCube((Vector3){ -0.48f, 1.45f, 0.3f }, 0.18f, 0.18f, 0.7f, skinColor);
        DrawCube((Vector3){ 0.48f, 1.45f, 0.3f }, 0.18f, 0.18f, 0.7f, skinColor);

        if (z->isBoss) DrawCylinder((Vector3){ 0.0f, 2.45f, 0.0f }, 0.3f, 0.35f, 0.25f, 6, GOLD);
    rlPopMatrix();
}

void DrawChest(TreasureChest *chest) {
    Vector3 cp = chest->position;
    if (!chest->opened) {
        DrawCube((Vector3){ cp.x, cp.y + 0.4f, cp.z }, 1.2f, 0.8f, 1.0f, (Color){ 180, 110, 40, 255 });
        DrawCubeWires((Vector3){ cp.x, cp.y + 0.4f, cp.z }, 1.2f, 0.8f, 1.0f, GOLD);
        DrawCube((Vector3){ cp.x, cp.y + 0.4f, cp.z + 0.52f }, 0.25f, 0.25f, 0.08f, GOLD);
    } else {
        DrawCube((Vector3){ cp.x, cp.y + 0.2f, cp.z }, 1.2f, 0.4f, 1.0f, (Color){ 140, 85, 30, 255 });
        DrawCube((Vector3){ cp.x, cp.y + 0.7f, cp.z - 0.4f }, 1.2f, 0.2f, 0.6f, (Color){ 180, 110, 40, 255 });
    }
}

void DrawSpecialArrow(Arrow *a) {
    Vector3 tip = Vector3Add(a->position, Vector3Scale(a->direction, 1.1f));
    DrawCylinderEx(a->position, tip, 0.08f, 0.08f, 8, a->color);
    DrawSphere(tip, 0.18f, a->color);
}

void DrawDetailedBuilding(Building *b, float globalTime) {
    Vector3 p = b->position;
    Color tierAccent = (b->tier == 3) ? (Color){ 0, 220, 255, 255 } : (b->tier == 2 ? (Color){ 180, 185, 195, 255 } : (Color){ 100, 60, 25, 255 });

    switch (b->type) {
        case BUILD_WALL:
            {
                float shakeX = (b->animTimer > 0.0f) ? sinf(b->animTimer * 40.0f) * 0.15f : 0.0f;
                DrawCube((Vector3){ p.x + shakeX, p.y + 1.0f, p.z }, BLOCK_SIZE, 2.0f * (0.8f + b->tier * 0.2f), BLOCK_SIZE, tierAccent);
                DrawCubeWires((Vector3){ p.x + shakeX, p.y + 1.0f, p.z }, BLOCK_SIZE, 2.0f * (0.8f + b->tier * 0.2f), BLOCK_SIZE, BLACK);
            }
            break;
        case BUILD_TOWER:
            {
                float recoil = (b->animTimer > 0.0f) ? (b->animTimer * 0.4f) : 0.0f;
                DrawCube((Vector3){ p.x, p.y + 2.0f, p.z }, 1.2f, 4.0f + b->tier * 0.5f, 1.2f, tierAccent);
                DrawCube((Vector3){ p.x, p.y + 4.2f + b->tier * 0.5f, p.z }, 2.4f, 0.4f, 2.4f, (Color){ 50, 40, 40, 255 });
                DrawSphere((Vector3){ p.x, p.y + 4.7f + b->tier * 0.5f, p.z - recoil }, 0.4f, GOLD);
            }
            break;
        case BUILD_CAMPFIRE:
            {
                float flameBob1 = sinf(globalTime * 8.0f) * 0.12f;
                DrawCylinder((Vector3){ p.x, p.y + 0.15f, p.z }, 0.8f, 0.8f, 0.3f, 8, (Color){ 70, 45, 25, 255 });
                DrawSphere((Vector3){ p.x, p.y + 0.55f + flameBob1, p.z }, 0.5f + b->tier * 0.1f, ORANGE);
            }
            break;
        case BUILD_SPIKE_TRAP:
            {
                float spikeHeight = (b->animTimer > 0.0f) ? (0.6f + b->animTimer * 0.8f) : 0.25f;
                DrawCube((Vector3){ p.x, p.y + 0.1f, p.z }, BLOCK_SIZE, 0.2f, BLOCK_SIZE, tierAccent);
                DrawCylinder((Vector3){ p.x, p.y + 0.1f, p.z }, 0.0f, 0.35f, spikeHeight + b->tier * 0.15f, 6, (Color){ 220, 220, 230, 255 });
            }
            break;
        case BUILD_GOLD_VAULT:
            {
                float pop = (b->animTimer > 0.0f) ? (sinf(b->animTimer * 12.0f) * 0.4f) : 0.0f;
                DrawCube((Vector3){ p.x, p.y + 1.2f, p.z }, 1.8f, 2.4f, 1.8f, (Color){ 215, 175, 55, 255 });
                DrawSphere((Vector3){ p.x, p.y + 2.7f + pop, p.z }, 0.45f * b->tier, GOLD);
            }
            break;
        case BUILD_TESLA_COIL:
            {
                DrawCylinder((Vector3){ p.x, p.y + 1.5f, p.z }, 0.35f, 0.6f, 3.0f, 8, tierAccent);
                DrawSphere((Vector3){ p.x, p.y + 3.2f, p.z }, 0.6f + b->tier * 0.1f, (Color){ 0, 220, 255, 255 });
            }
            break;
        case BUILD_FROST_TOTEM:
            {
                DrawCube((Vector3){ p.x, p.y + 1.5f, p.z }, 0.8f, 3.0f, 0.8f, (Color){ 120, 190, 230, 255 });
                DrawSphere((Vector3){ p.x, p.y + 3.2f, p.z }, 0.55f + b->tier * 0.1f, (Color){ 200, 240, 255, 255 });
            }
            break;
        case BUILD_MORTAR:
            {
                DrawCylinder((Vector3){ p.x, p.y + 0.4f, p.z }, 0.9f, 1.1f, 0.8f, 8, tierAccent);
                DrawCylinder((Vector3){ p.x, p.y + 1.3f, p.z }, 0.4f, 0.4f, 1.6f, 8, (Color){ 25, 30, 35, 255 });
            }
            break;
        case BUILD_SHRINE:
            {
                float orbFloat = sinf(globalTime * 3.0f) * 0.25f;
                DrawCube((Vector3){ p.x, p.y + 0.3f, p.z }, 1.8f, 0.6f, 1.8f, tierAccent);
                DrawSphere((Vector3){ p.x, p.y + 2.4f + orbFloat, p.z }, 0.55f + b->tier * 0.1f, (Color){ 230, 100, 255, 255 });
            }
            break;
        case BUILD_SPRINGBOARD:
            {
                DrawCube((Vector3){ p.x, p.y + 0.15f, p.z }, BLOCK_SIZE, 0.3f, BLOCK_SIZE, tierAccent);
                DrawCylinder((Vector3){ p.x, p.y + 0.45f, p.z }, 0.6f, 0.6f, 0.4f, 8, (Color){ 220, 60, 40, 255 });
            }
            break;
        default: break;
    }
}

void SpawnHorde(Zombie3D zombies[], int wave, Vector3 playerPos, int seed) {
    bool isBossWave = (wave % 5 == 0);
    int hordeCount = isBossWave ? 1 : (6 + (wave * 3));
    if (hordeCount > MAX_ZOMBIES) hordeCount = MAX_ZOMBIES;

    float baseSpeed = 3.5f + (wave * 0.2f);
    float baseHealth = 30.0f + (wave * 14.0f);

    int spawned = 0;
    for (int i = 0; i < MAX_ZOMBIES && spawned < hordeCount; i++) {
        if (!zombies[i].active) {
            float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
            float spawnDist = isBossWave ? 30.0f : (float)GetRandomValue(25, 35);
            float sx = playerPos.x + cosf(angle) * spawnDist;
            float sz = playerPos.z + sinf(angle) * spawnDist;

            BiomeType spawnBiome;
            zombies[i].position = (Vector3){ sx, GetWorldHeight(sx, sz, seed), sz };
            GetElevationAt((int)floorf(sx / BLOCK_SIZE), (int)floorf(sz / BLOCK_SIZE), seed, &spawnBiome);
            zombies[i].biomeType = spawnBiome;

            if (isBossWave) {
                zombies[i].isBoss = true;
                zombies[i].scale = 2.4f;
                zombies[i].speed = baseSpeed * 0.75f;
                zombies[i].health = baseHealth * 8.0f;
            } else {
                zombies[i].isBoss = false;
                zombies[i].scale = 1.0f;
                zombies[i].speed = (spawnBiome == BIOME_DESERT) ? (baseSpeed * 1.3f) : baseSpeed;
                zombies[i].health = (spawnBiome == BIOME_VOLCANO) ? (baseHealth * 1.5f) : baseHealth;
            }

            zombies[i].maxHealth = zombies[i].health;
            zombies[i].rotation = 0.0f;
            zombies[i].walkTimer = 0.0f;
            zombies[i].deathTimer = 0.0f;
            zombies[i].slowTimer = 0.0f;
            zombies[i].isMoving = true;
            zombies[i].active = true;
            spawned++;
        }
    }
}

Vector3 ResolveBuildingCollisions(Vector3 pos, float entityRadius, Building buildings[]) {
    Vector3 resolved = pos;
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (!buildings[i].active) continue;
        if (buildings[i].type != BUILD_WALL && buildings[i].type != BUILD_TOWER && buildings[i].type != BUILD_GOLD_VAULT) continue;

        float wallX = buildings[i].position.x;
        float wallZ = buildings[i].position.z;
        float halfSize = BLOCK_SIZE / 2.0f;

        float closestX = fmaxf(wallX - halfSize, fminf(resolved.x, wallX + halfSize));
        float closestZ = fmaxf(wallZ - halfSize, fminf(resolved.z, wallZ + halfSize));
        float dx = resolved.x - closestX;
        float dz = resolved.z - closestZ;
        float dist = sqrtf(dx * dx + dz * dz);

        if (dist < entityRadius && dist > 0.0001f) {
            float overlap = entityRadius - dist;
            resolved.x += (dx / dist) * overlap;
            resolved.z += (dz / dist) * overlap;
            if (buildings[i].type == BUILD_WALL) buildings[i].animTimer = 0.35f;
        }
    }
    return resolved;
}

bool GetGroundIntersection(Ray ray, float planeY, Vector3 *outIntersection) {
    if (fabsf(ray.direction.y) > 0.0001f) {
        float t = (planeY - ray.position.y) / ray.direction.y;
        if (t >= 0.0f) {
            *outIntersection = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            return true;
        }
    }
    return false;
}

int main(void) {
    // Android automatically runs in full display resolution
    InitWindow(1280, 720, "Solo Leveling 3D Hunter - Mobile Edition");
    SetTargetFPS(60);

    GameState currentState = STATE_MAIN_MENU;
    GameSaveData saveData = LoadGameData();
    int activeSeed = 998822;

    for (int i = 0; i < TOTAL_WEAPONS; i++) {
        if (saveData.unlockedWeapons[i]) weaponCatalog[i].unlocked = true;
    }

    int equippedWeapon = saveData.equippedWeaponId;
    if (equippedWeapon < 0 || equippedWeapon >= TOTAL_WEAPONS) equippedWeapon = 0;

    Chunk chunks[NUM_CHUNKS] = { 0 };
    UpdateChunkManager(chunks, 0, 0, activeSeed);

    Vector3 playerPos = { 0.0f, 0.0f, 0.0f };
    playerPos.y = GetWorldHeight(playerPos.x, playerPos.z, activeSeed);
    float playerSpeed = 8.5f;
    float playerRot = 0.0f;
    float playerWalkTimer = 0.0f;
    bool playerMoving = false;
    float playerHealth = 100.0f;
    float playerMaxHealth = 100.0f;
    int goldCoins = saveData.goldCoins;

    Vector3 buildCameraOffset = { 0.0f, 0.0f, 0.0f };
    float shootTimer = 0.0f;
    float attackAnimProgress = 0.0f;
    bool isBuildMode = false;
    bool isInventoryOpen = false;
    int inventoryPage = 0;
    BuildingType selectedBuild = BUILD_WALL;

    Zombie3D zombies[MAX_ZOMBIES] = { 0 };
    Coin coins[MAX_COINS] = { 0 };
    Arrow arrows[MAX_ARROWS] = { 0 };
    Building buildings[MAX_BUILDINGS] = { 0 };
    TreasureChest chests[MAX_CHESTS] = { 0 };

    for (int i = 0; i < MAX_CHESTS; i++) {
        float cx = (float)GetRandomValue(-80, 80);
        float cz = (float)GetRandomValue(-80, 80);
        chests[i].position = (Vector3){ cx, GetWorldHeight(cx, cz, activeSeed), cz };
        chests[i].opened = false;
        chests[i].active = true;
    }

    int currentHorde = 1;
    float hordeTimer = 25.0f;
    int sessionKills = 0;
    char notification[64] = "Solo Leveling Hunter Activated";
    float notifTimer = 4.0f;
    float globalTime = 0.0f;

    Camera3D camera = { 0 };
    camera.target = playerPos;
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 52.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Mobile Virtual Touch Joystick
    VirtualJoystick joystick = {
        .center = { 150.0f, 570.0f },
        .knob = { 150.0f, 570.0f },
        .radius = 75.0f,
        .active = false,
        .touchId = -1
    };

    Vector3 touchSnapTarget = { 0 };
    bool hasTouchSnap = false;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        globalTime += dt;
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        // Responsive Mobile Dynamic Touch Layout
        joystick.center = (Vector2){ (float)screenW * 0.13f, (float)screenH * 0.78f };
        Rectangle btnMobileBuild   = { (float)screenW - 140.0f, (float)screenH - 140.0f, 115.0f, 115.0f };
        Rectangle btnMobileArsenal = { (float)screenW - 270.0f, (float)screenH - 120.0f, 100.0f, 95.0f };
        Rectangle btnMobileChest   = { (float)screenW - 390.0f, (float)screenH - 120.0f, 100.0f, 95.0f };
        Rectangle btnMobilePause   = { (float)screenW - 90.0f, 25.0f, 65.0f, 65.0f };

        // ------------------ TOUCH MULTI-TOUCH PROCESSING ------------------
        int touchCount = GetTouchPointCount();
        Vector2 moveVector = { 0.0f, 0.0f };

        bool touchBuildPressed = false;
        bool touchArsenalPressed = false;
        bool touchChestPressed = false;
        bool touchPausePressed = false;

        for (int t = 0; t < touchCount; t++) {
            Vector2 tPos = GetTouchPosition(t);

            if (CheckCollisionPointRec(tPos, btnMobileBuild) && IsGestureDetected(GESTURE_TAP)) touchBuildPressed = true;
            if (CheckCollisionPointRec(tPos, btnMobileArsenal) && IsGestureDetected(GESTURE_TAP)) touchArsenalPressed = true;
            if (CheckCollisionPointRec(tPos, btnMobileChest) && IsGestureDetected(GESTURE_TAP)) touchChestPressed = true;
            if (CheckCollisionPointRec(tPos, btnMobilePause) && IsGestureDetected(GESTURE_TAP)) touchPausePressed = true;

            // Touch Analog Joystick (Bottom-Left Quad)
            if (tPos.x < (float)screenW * 0.45f && tPos.y > (float)screenH * 0.45f) {
                Vector2 diff = Vector2Subtract(tPos, joystick.center);
                float dist = Vector2Length(diff);
                if (dist > joystick.radius) diff = Vector2Scale(Vector2Normalize(diff), joystick.radius);
                joystick.knob = Vector2Add(joystick.center, diff);
                moveVector = Vector2Scale(diff, 1.0f / joystick.radius);
                joystick.active = true;
            } 
            // Ground Tap Raycasting in Build Mode
            else if (isBuildMode && tPos.y < (float)screenH - 180.0f) {
                Ray touchRay = GetMouseRay(tPos, camera);
                Vector3 groundHit = { 0 };
                if (GetGroundIntersection(touchRay, GetWorldHeight(camera.target.x, camera.target.z, activeSeed), &groundHit)) {
                    touchSnapTarget.x = floorf(groundHit.x / BLOCK_SIZE + 0.5f) * BLOCK_SIZE;
                    touchSnapTarget.z = floorf(groundHit.z / BLOCK_SIZE + 0.5f) * BLOCK_SIZE;
                    touchSnapTarget.y = GetWorldHeight(touchSnapTarget.x, touchSnapTarget.z, activeSeed);
                    hasTouchSnap = true;
                }
            }
        }

        if (touchCount == 0 && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            joystick.knob = joystick.center;
            joystick.active = false;
        }

        // ------------------ STATE PROCESSING ------------------
        if (currentState == STATE_MAIN_MENU) {
            float camRadius = 25.0f;
            camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };
            camera.position = (Vector3){ cosf(globalTime * 0.2f) * camRadius, 18.0f, sinf(globalTime * 0.2f) * camRadius };
            UpdateChunkManager(chunks, 0, 0, activeSeed);
        }
        else if (currentState == STATE_GAMEPLAY) {
            if (touchPausePressed) currentState = STATE_PAUSED;
            if (touchArsenalPressed) {
                isInventoryOpen = !isInventoryOpen;
                if (isInventoryOpen) isBuildMode = false;
            }
            if (touchBuildPressed && !isInventoryOpen) {
                isBuildMode = !isBuildMode;
                if (isBuildMode) {
                    selectedBuild = BUILD_WALL;
                    buildCameraOffset = (Vector3){ 0.0f, 0.0f, 0.0f };
                }
            }

            if (notifTimer > 0.0f) notifTimer -= dt;

            // Decay building timers
            for (int i = 0; i < MAX_BUILDINGS; i++) {
                if (buildings[i].active && buildings[i].animTimer > 0.0f) {
                    buildings[i].animTimer -= dt;
                    if (buildings[i].animTimer < 0.0f) buildings[i].animTimer = 0.0f;
                }
            }

            // Camera Setup
            if (isBuildMode) {
                Vector3 buildTarget = Vector3Add(playerPos, buildCameraOffset);
                camera.target = buildTarget;
                camera.position = (Vector3){ buildTarget.x + 16.0f, buildTarget.y + 24.0f, buildTarget.z + 16.0f };
            } else {
                camera.target = playerPos;
                camera.position = (Vector3){ playerPos.x + 14.0f, playerPos.y + 18.0f, playerPos.z + 14.0f };
            }

            // Combat & Mechanics
            if (!isBuildMode && !isInventoryOpen) {
                if (playerHealth > 0) {
                    hordeTimer -= dt;
                    if (hordeTimer <= 0.0f) {
                        currentHorde++;
                        hordeTimer = 25.0f;
                        SpawnHorde(zombies, currentHorde, playerPos, activeSeed);
                        if (currentHorde > saveData.highestWave) {
                            saveData.highestWave = currentHorde;
                            saveData.hunterLevel = 1 + (saveData.highestWave / 2);
                        }
                        saveData.currentWave = currentHorde;
                        saveData.goldCoins = goldCoins;
                        saveData.hasSavedGame = true;
                        SaveGameData(saveData);
                    }
                }

                // Player Movement via Touch Joystick
                playerMoving = false;
                if (playerHealth > 0 && Vector2Length(moveVector) > 0.12f) {
                    Vector3 moveDir = { moveVector.x, 0.0f, moveVector.y };
                    moveDir = Vector3Normalize(moveDir);
                    Vector3 target = Vector3Add(playerPos, Vector3Scale(moveDir, playerSpeed * dt));
                    float nextTerrainY = GetWorldHeight(target.x, target.z, activeSeed);

                    if (nextTerrainY - playerPos.y <= 1.2f) {
                        playerPos.x = target.x;
                        playerPos.z = target.z;
                        playerPos.y = nextTerrainY;
                        playerPos = ResolveBuildingCollisions(playerPos, 0.45f, buildings);
                        playerRot = atan2f(moveDir.x, moveDir.z) * RAD2DEG;
                        playerWalkTimer += dt;
                        playerMoving = true;
                    }
                }

                // Chest Open
                if (touchChestPressed && playerHealth > 0) {
                    for (int i = 0; i < MAX_CHESTS; i++) {
                        if (chests[i].active && !chests[i].opened) {
                            if (Vector3Distance(playerPos, chests[i].position) < 4.0f) {
                                chests[i].opened = true;
                                goldCoins += 200;
                                playerMaxHealth += 50.0f;
                                playerHealth = playerMaxHealth;
                                TextCopy(notification, "CHEST LOOTED: +200g & HP Boost!");
                                notifTimer = 3.5f;
                                break;
                            }
                        }
                    }
                }

                // Automatic Hunter Attack Loop
                WeaponDef activeWep = weaponCatalog[equippedWeapon];
                shootTimer -= dt;
                if (shootTimer < 0.0f) shootTimer = 0.0f;
                attackAnimProgress = (activeWep.fireRate > 0.0f) ? (1.0f - (shootTimer / activeWep.fireRate)) : 0.0f;

                if (shootTimer <= 0.0f && playerHealth > 0) {
                    shootTimer = activeWep.fireRate;
                    int closest = -1;
                    float minDist = 26.0f;
                    for (int i = 0; i < MAX_ZOMBIES; i++) {
                        if (zombies[i].active && zombies[i].health > 0) {
                            float d = Vector3Distance(playerPos, zombies[i].position);
                            if (d < minDist) { minDist = d; closest = i; }
                        }
                    }
                    if (closest != -1) {
                        for (int a = 0; a < MAX_ARROWS; a++) {
                            if (!arrows[a].active) {
                                arrows[a].position = (Vector3){ playerPos.x, playerPos.y + 1.4f, playerPos.z };
                                arrows[a].direction = Vector3Normalize(Vector3Subtract(zombies[closest].position, arrows[a].position));
                                arrows[a].speed = 32.0f;
                                arrows[a].damage = activeWep.damage;
                                arrows[a].aoeRadius = activeWep.aoeRadius;
                                arrows[a].pierceCount = activeWep.pierce;
                                arrows[a].weaponId = equippedWeapon;
                                arrows[a].color = activeWep.projectileColor;
                                arrows[a].lifeTime = 2.0f;
                                arrows[a].active = true;
                                break;
                            }
                        }
                    }
                }

                // Active Defenses Execution
                for (int i = 0; i < MAX_BUILDINGS; i++) {
                    if (!buildings[i].active) continue;
                    Vector3 bp = buildings[i].position;

                    switch (buildings[i].type) {
                        case BUILD_CAMPFIRE:
                            if (Vector3Distance(playerPos, bp) < (4.0f + buildings[i].tier) && playerHealth > 0) {
                                playerHealth += (10.0f + buildings[i].tier * 5.0f) * dt;
                                if (playerHealth > playerMaxHealth) playerHealth = playerMaxHealth;
                                buildings[i].animTimer = 0.5f;
                            }
                            break;
                        case BUILD_TOWER:
                            buildings[i].actionCooldown -= dt;
                            if (buildings[i].actionCooldown <= 0.0f) {
                                buildings[i].actionCooldown = 0.7f / (float)buildings[i].tier;
                                for (int z = 0; z < MAX_ZOMBIES; z++) {
                                    if (zombies[z].active && zombies[z].health > 0 && Vector3Distance(bp, zombies[z].position) < 18.0f) {
                                        for (int a = 0; a < MAX_ARROWS; a++) {
                                            if (!arrows[a].active) {
                                                arrows[a].position = (Vector3){ bp.x, bp.y + 4.2f, bp.z };
                                                arrows[a].direction = Vector3Normalize(Vector3Subtract(zombies[z].position, arrows[a].position));
                                                arrows[a].speed = 26.0f;
                                                arrows[a].damage = 35.0f * (float)buildings[i].tier;
                                                arrows[a].aoeRadius = 0.0f;
                                                arrows[a].pierceCount = 1;
                                                arrows[a].color = (Color){ 0, 220, 255, 255 };
                                                arrows[a].lifeTime = 1.5f;
                                                arrows[a].active = true;
                                                buildings[i].animTimer = 0.25f;
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                            break;
                        case BUILD_SPIKE_TRAP:
                            for (int z = 0; z < MAX_ZOMBIES; z++) {
                                if (zombies[z].active && zombies[z].health > 0 && Vector3Distance(bp, zombies[z].position) < 1.4f) {
                                    zombies[z].health -= (30.0f + buildings[i].tier * 15.0f) * dt;
                                    buildings[i].animTimer = 0.35f;
                                }
                            }
                            break;
                        case BUILD_GOLD_VAULT:
                            buildings[i].actionCooldown -= dt;
                            if (buildings[i].actionCooldown <= 0.0f) {
                                buildings[i].actionCooldown = 6.0f / (float)buildings[i].tier;
                                for (int c = 0; c < MAX_COINS; c++) {
                                    if (!coins[c].active) {
                                        coins[c].position = (Vector3){ bp.x + 1.0f, bp.y + 0.4f, bp.z };
                                        coins[c].value = 5 * buildings[i].tier;
                                        coins[c].active = true;
                                        buildings[i].animTimer = 0.4f;
                                        break;
                                    }
                                }
                            }
                            break;
                        default: break;
                    }
                }

                // Projectiles
                for (int a = 0; a < MAX_ARROWS; a++) {
                    if (arrows[a].active) {
                        arrows[a].position = Vector3Add(arrows[a].position, Vector3Scale(arrows[a].direction, arrows[a].speed * dt));
                        arrows[a].lifeTime -= dt;

                        for (int z = 0; z < MAX_ZOMBIES; z++) {
                            if (zombies[z].active && zombies[z].health > 0) {
                                if (Vector3Distance(arrows[a].position, zombies[z].position) < 1.4f) {
                                    zombies[z].health -= arrows[a].damage;
                                    arrows[a].pierceCount--;
                                    if (arrows[a].pierceCount <= 0) {
                                        arrows[a].active = false;
                                        break;
                                    }
                                }
                            }
                        }
                        if (arrows[a].lifeTime <= 0.0f) arrows[a].active = false;
                    }
                }

                // Zombies
                for (int i = 0; i < MAX_ZOMBIES; i++) {
                    if (!zombies[i].active) continue;

                    if (zombies[i].health <= 0) {
                        zombies[i].deathTimer += dt * 3.0f;
                        if (zombies[i].deathTimer >= 1.0f) {
                            zombies[i].active = false;
                            sessionKills++;
                            saveData.lifetimeKills++;

                            int dropCount = zombies[i].isBoss ? 10 : 1;
                            for (int c = 0; c < MAX_COINS && dropCount > 0; c++) {
                                if (!coins[c].active) {
                                    coins[c].position = (Vector3){ zombies[i].position.x, zombies[i].position.y + 0.3f, zombies[i].position.z };
                                    coins[c].value = 1;
                                    coins[c].active = true;
                                    dropCount--;
                                }
                            }
                        }
                    } else if (playerHealth > 0) {
                        Vector3 dir = Vector3Normalize(Vector3Subtract(playerPos, zombies[i].position));
                        dir.y = 0.0f;
                        zombies[i].position.x += dir.x * zombies[i].speed * dt;
                        zombies[i].position.z += dir.z * zombies[i].speed * dt;
                        zombies[i].position.y = GetWorldHeight(zombies[i].position.x, zombies[i].position.z, activeSeed);
                        zombies[i].position = ResolveBuildingCollisions(zombies[i].position, 0.45f * zombies[i].scale, buildings);
                        zombies[i].rotation = atan2f(dir.x, dir.z) * RAD2DEG;
                        zombies[i].walkTimer += dt;

                        if (Vector3Distance(zombies[i].position, playerPos) < (1.4f * zombies[i].scale)) {
                            playerHealth -= (zombies[i].isBoss ? 45.0f : 20.0f) * dt;
                            if (playerHealth <= 0) playerHealth = 0;
                        }
                    }
                }

                // Coin Vacuum
                for (int c = 0; c < MAX_COINS; c++) {
                    if (!coins[c].active) continue;
                    float dist = Vector3Distance(coins[c].position, playerPos);
                    if (dist < 8.0f) {
                        Vector3 pull = Vector3Normalize(Vector3Subtract(playerPos, coins[c].position));
                        coins[c].position = Vector3Add(coins[c].position, Vector3Scale(pull, 14.0f * dt));
                    }
                    if (dist < 1.4f) {
                        coins[c].active = false;
                        goldCoins += coins[c].value;
                        saveData.totalGold += coins[c].value;
                    }
                }
            }

            int currentChunkX = (int)floorf(playerPos.x / (CHUNK_SIZE * BLOCK_SIZE));
            int currentChunkZ = (int)floorf(playerPos.z / (CHUNK_SIZE * BLOCK_SIZE));
            UpdateChunkManager(chunks, currentChunkX, currentChunkZ, activeSeed);
        }

        // ------------------ 3D DRAWING ------------------
        BeginDrawing();
            ClearBackground((Color){ 140, 180, 210, 255 });

            BeginMode3D(camera);

                for (int c = 0; c < NUM_CHUNKS; c++) {
                    if (!chunks[c].loaded) continue;
                    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
                            Voxel v = chunks[c].blocks[lz][lx];
                            float wx = (chunks[c].chunkX * CHUNK_SIZE + lx + 0.5f) * BLOCK_SIZE;
                            float wz = (chunks[c].chunkZ * CHUNK_SIZE + lz + 0.5f) * BLOCK_SIZE;
                            float blockHeight = (v.height + 1) * 1.0f;

                            Color topColor, sideColor;
                            switch (v.biome) {
                                case BIOME_VOLCANO:  topColor = (Color){ 35, 30, 35, 255 }; sideColor = (Color){ 180, 45, 10, 255 }; break;
                                case BIOME_DESERT:   topColor = (Color){ 225, 205, 135, 255 }; sideColor = (Color){ 195, 170, 105, 255 }; break;
                                case BIOME_FOREST:   topColor = (Color){ 40, 95, 40, 255 }; sideColor = (Color){ 90, 60, 35, 255 }; break;
                                default:             topColor = (Color){ 65, 135, 45, 255 }; sideColor = (Color){ 105, 70, 40, 255 }; break;
                            }

                            Vector3 bPos = { wx, blockHeight / 2.0f - 0.5f, wz };
                            DrawCube(bPos, BLOCK_SIZE, blockHeight, BLOCK_SIZE, sideColor);
                            DrawCube((Vector3){ wx, blockHeight - 0.5f, wz }, BLOCK_SIZE, 0.1f, BLOCK_SIZE, topColor);
                            DrawCubeWires(bPos, BLOCK_SIZE, blockHeight, BLOCK_SIZE, (Color){ 20, 30, 40, 75 });

                            if (v.hasVegetation) {
                                Vector3 vegBase = { wx, blockHeight - 0.5f, wz };
                                DrawBiomeVegetation(vegBase, v.biome, v.vegSeed);
                            }
                        }
                    }
                }

                if (currentState == STATE_GAMEPLAY) {
                    for (int i = 0; i < MAX_CHESTS; i++) if (chests[i].active) DrawChest(&chests[i]);
                    for (int i = 0; i < MAX_BUILDINGS; i++) if (buildings[i].active) DrawDetailedBuilding(&buildings[i], globalTime);

                    if (isBuildMode && hasTouchSnap) {
                        DrawCube((Vector3){ touchSnapTarget.x, touchSnapTarget.y + 1.0f, touchSnapTarget.z }, BLOCK_SIZE, 2.0f, BLOCK_SIZE, (Color){ 0, 220, 255, 140 });
                        DrawCubeWires((Vector3){ touchSnapTarget.x, touchSnapTarget.y + 1.0f, touchSnapTarget.z }, BLOCK_SIZE, 2.0f, BLOCK_SIZE, WHITE);
                    }

                    for (int c = 0; c < MAX_COINS; c++) if (coins[c].active) DrawCylinder(coins[c].position, 0.25f, 0.25f, 0.1f, 8, GOLD);
                    for (int a = 0; a < MAX_ARROWS; a++) if (arrows[a].active) DrawSpecialArrow(&arrows[a]);
                    for (int i = 0; i < MAX_ZOMBIES; i++) if (zombies[i].active) DrawThemedZombie(&zombies[i]);
                    if (playerHealth > 0) DrawHunter(playerPos, playerRot, playerWalkTimer, playerMoving, weaponCatalog[equippedWeapon].rank, &weaponCatalog[equippedWeapon], attackAnimProgress, globalTime);
                }

            EndMode3D();

            // ------------------ 2D TOUCH UI & MENUS ------------------

            // MAIN MENU
            if (currentState == STATE_MAIN_MENU) {
                DrawRectangle(0, 0, screenW, screenH, (Color){ 10, 14, 24, 190 });

                DrawText("SOLO LEVELING", screenW / 2 - MeasureText("SOLO LEVELING", 44) / 2, 70, 44, (Color){ 0, 210, 255, 255 });
                DrawText("3D HUNTER SURVIVAL (ANDROID)", screenW / 2 - MeasureText("3D HUNTER SURVIVAL (ANDROID)", 18) / 2, 120, 18, GOLD);

                Rectangle btnNewGame = { (float)screenW / 2 - 160, 220, 320, 60 };
                Rectangle btnContinue = { (float)screenW / 2 - 160, 300, 320, 60 };
                Rectangle btnSettings = { (float)screenW / 2 - 160, 380, 320, 60 };

                DrawRectangleRounded(btnNewGame, 0.3f, 6, (Color){ 20, 30, 50, 255 });
                DrawRectangleRoundedLines(btnNewGame, 0.3f, 6, 2, (Color){ 0, 210, 255, 255 });
                DrawText("NEW GAME", (int)btnNewGame.x + 105, (int)btnNewGame.y + 19, 20, WHITE);
                if (CheckCollisionPointRec(GetMousePosition(), btnNewGame) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentHorde = 1;
                    playerHealth = 100.0f;
                    playerMaxHealth = 100.0f;
                    goldCoins = 60;
                    sessionKills = 0;
                    equippedWeapon = 0;
                    hordeTimer = 25.0f;
                    for (int i = 0; i < MAX_ZOMBIES; i++) zombies[i].active = false;
                    for (int i = 0; i < MAX_BUILDINGS; i++) buildings[i].active = false;
                    SpawnHorde(zombies, currentHorde, playerPos, activeSeed);
                    currentState = STATE_GAMEPLAY;
                }

                bool canContinue = saveData.hasSavedGame;
                DrawRectangleRounded(btnContinue, 0.3f, 6, canContinue ? (Color){ 20, 30, 50, 255 } : (Color){ 30, 30, 35, 180 });
                DrawRectangleRoundedLines(btnContinue, 0.3f, 6, 2, canContinue ? GOLD : DARKGRAY);
                DrawText("CONTINUE", (int)btnContinue.x + 110, (int)btnContinue.y + 19, 20, canContinue ? GOLD : GRAY);
                if (canContinue && CheckCollisionPointRec(GetMousePosition(), btnContinue) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentHorde = saveData.currentWave;
                    goldCoins = saveData.goldCoins;
                    equippedWeapon = saveData.equippedWeaponId;
                    for (int i = 0; i < MAX_ZOMBIES; i++) zombies[i].active = false;
                    SpawnHorde(zombies, currentHorde, playerPos, activeSeed);
                    currentState = STATE_GAMEPLAY;
                }

                DrawRectangleRounded(btnSettings, 0.3f, 6, (Color){ 20, 30, 50, 255 });
                DrawRectangleRoundedLines(btnSettings, 0.3f, 6, 2, (Color){ 0, 210, 255, 255 });
                DrawText("SETTINGS", (int)btnSettings.x + 110, (int)btnSettings.y + 19, 20, WHITE);
                if (CheckCollisionPointRec(GetMousePosition(), btnSettings) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentState = STATE_SETTINGS;
                }
            }
            // SETTINGS
            else if (currentState == STATE_SETTINGS) {
                DrawRectangle(0, 0, screenW, screenH, (Color){ 10, 14, 24, 235 });
                DrawText("MOBILE SETTINGS", screenW / 2 - 120, 90, 24, GOLD);

                Rectangle btnBack = { (float)screenW / 2 - 140, 480, 280, 55 };
                DrawRectangleRounded(btnBack, 0.3f, 6, (Color){ 0, 180, 230, 255 });
                DrawText("BACK TO MENU", (int)btnBack.x + 70, (int)btnBack.y + 18, 18, WHITE);
                if (CheckCollisionPointRec(GetMousePosition(), btnBack) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentState = STATE_MAIN_MENU;
                }
            }
            // PAUSE MENU
            else if (currentState == STATE_PAUSED) {
                DrawRectangle(0, 0, screenW, screenH, (Color){ 5, 8, 15, 210 });
                DrawText("GAME PAUSED", screenW / 2 - 100, screenH / 2 - 140, 26, GOLD);

                Rectangle btnResume = { (float)screenW / 2 - 140, (float)screenH / 2 - 70, 280, 55 };
                Rectangle btnQuit   = { (float)screenW / 2 - 140, (float)screenH / 2 + 10, 280, 55 };

                DrawRectangleRounded(btnResume, 0.3f, 6, (Color){ 0, 160, 220, 255 });
                DrawText("RESUME", (int)btnResume.x + 100, (int)btnResume.y + 18, 18, WHITE);
                if (CheckCollisionPointRec(GetMousePosition(), btnResume) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentState = STATE_GAMEPLAY;
                }

                DrawRectangleRounded(btnQuit, 0.3f, 6, (Color){ 45, 20, 25, 255 });
                DrawText("MAIN MENU", (int)btnQuit.x + 90, (int)btnQuit.y + 18, 18, RED);
                if (CheckCollisionPointRec(GetMousePosition(), btnQuit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    currentState = STATE_MAIN_MENU;
                }
            }
            // GAMEPLAY HUD
            else if (currentState == STATE_GAMEPLAY) {
                Color rankColor;
                const char* rankStr = GetRankLabel(weaponCatalog[equippedWeapon].rank, &rankColor);

                // Top Left Hunter Card
                DrawRectangleRounded((Rectangle){ 20, 20, 290, 120 }, 0.2f, 6, (Color){ 12, 16, 26, 235 });
                DrawRectangleRoundedLines((Rectangle){ 20, 20, 290, 120 }, 0.2f, 6, 2, rankColor);

                DrawText(TextFormat("HUNTER LVL %d", saveData.hunterLevel), 32, 28, 15, GOLD);
                DrawText(TextFormat("[%s]", rankStr), 195, 28, 15, rankColor);
                DrawText(TextFormat("Wep: %s", weaponCatalog[equippedWeapon].name), 32, 48, 13, RAYWHITE);
                DrawText(TextFormat("Gold: %d  |  Wave: %d", goldCoins, currentHorde), 32, 68, 14, GOLD);
                DrawText(TextFormat("HP: %.0f / %.0f", playerHealth, playerMaxHealth), 32, 88, 14, playerHealth > 25 ? GREEN : RED);

                // Touch Analog Joystick
                if (!isBuildMode && !isInventoryOpen) {
                    DrawCircleV(joystick.center, joystick.radius, (Color){ 255, 255, 255, 30 });
                    DrawCircleLines(joystick.center.x, joystick.center.y, joystick.radius, (Color){ 255, 255, 255, 80 });
                    DrawCircleV(joystick.knob, 38.0f, (Color){ 0, 210, 255, 190 });

                    // Action Buttons
                    DrawRectangleRounded(btnMobileBuild, 0.3f, 6, (Color){ 0, 180, 240, 230 });
                    DrawRectangleRoundedLines(btnMobileBuild, 0.3f, 6, 3, WHITE);
                    DrawText("BUILD\nMODE", (int)btnMobileBuild.x + 24, (int)btnMobileBuild.y + 35, 18, WHITE);

                    DrawRectangleRounded(btnMobileArsenal, 0.3f, 6, (Color){ 130, 40, 200, 230 });
                    DrawRectangleRoundedLines(btnMobileArsenal, 0.3f, 6, 2, WHITE);
                    DrawText("HUNTER\nVAULT", (int)btnMobileArsenal.x + 18, (int)btnMobileArsenal.y + 28, 15, WHITE);

                    DrawRectangleRounded(btnMobileChest, 0.3f, 6, (Color){ 215, 175, 55, 220 });
                    DrawRectangleRoundedLines(btnMobileChest, 0.3f, 6, 2, WHITE);
                    DrawText("OPEN\nCHEST", (int)btnMobileChest.x + 22, (int)btnMobileChest.y + 28, 15, BLACK);
                }

                // Pause Button (Top Right)
                DrawRectangleRounded(btnMobilePause, 0.3f, 6, (Color){ 25, 30, 45, 220 });
                DrawRectangleRoundedLines(btnMobilePause, 0.3f, 6, 2, (Color){ 0, 210, 255, 255 });
                DrawText("||", (int)btnMobilePause.x + 24, (int)btnMobilePause.y + 18, 24, WHITE);

                // Build Mode Mobile Touch Interface
                if (isBuildMode) {
                    DrawRectangle(0, 0, screenW, screenH, (Color){ 10, 15, 25, 90 });
                    DrawRectangleRounded(btnMobileBuild, 0.3f, 6, (Color){ 220, 50, 60, 240 });
                    DrawText("RESUME", (int)btnMobileBuild.x + 18, (int)btnMobileBuild.y + 45, 18, WHITE);

                    // Touch building selection bar
                    float btnW = ((float)screenW - 40.0f) / 10.0f;
                    const char* bLabels[] = { "", "Wall\n5g", "Tower\n20g", "Fire\n15g", "Spike\n10g", "Vault\n30g", "Tesla\n40g", "Frost\n25g", "Mortar\n50g", "Shrine\n45g", "Spring\n35g" };

                    for (int b = 1; b <= 10; b++) {
                        Rectangle bRect = { 20.0f + (b - 1) * btnW, (float)screenH - 145.0f, btnW - 4.0f, 60.0f };
                        bool isSel = (selectedBuild == (BuildingType)b);
                        DrawRectangleRec(bRect, isSel ? (Color){ 0, 210, 255, 230 } : (Color){ 25, 30, 45, 210 });
                        DrawRectangleLinesEx(bRect, 2, isSel ? WHITE : (Color){ 70, 80, 100, 255 });
                        DrawText(bLabels[b], (int)bRect.x + 6, (int)bRect.y + 12, 12, WHITE);

                        for (int t = 0; t < touchCount; t++) {
                            if (CheckCollisionPointRec(GetTouchPosition(t), bRect)) selectedBuild = (BuildingType)b;
                        }
                    }

                    // Mobile Confirm Build Button
                    if (hasTouchSnap) {
                        Rectangle btnConfirm = { (float)screenW / 2 - 120, (float)screenH - 220, 240, 55 };
                        DrawRectangleRounded(btnConfirm, 0.3f, 6, (Color){ 50, 200, 90, 255 });
                        DrawText("TAP TO BUILD", (int)btnConfirm.x + 55, (int)btnConfirm.y + 18, 16, WHITE);

                        if (CheckCollisionPointRec(GetMousePosition(), btnConfirm) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            int costs[] = { 0, 5, 20, 15, 10, 30, 40, 25, 50, 45, 35 };
                            int cost = costs[selectedBuild];
                            if (goldCoins >= cost) {
                                for (int i = 0; i < MAX_BUILDINGS; i++) {
                                    if (!buildings[i].active) {
                                        buildings[i].position = touchSnapTarget;
                                        buildings[i].type = selectedBuild;
                                        buildings[i].tier = 1;
                                        buildings[i].actionCooldown = 0.0f;
                                        buildings[i].animTimer = 0.4f;
                                        buildings[i].active = true;
                                        goldCoins -= cost;
                                        hasTouchSnap = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                // 50-Weapon Vault Touch Overlay
                if (isInventoryOpen) {
                    DrawRectangle(0, 0, screenW, screenH, (Color){ 5, 8, 15, 240 });
                    DrawText("HUNTER WEAPON VAULT", screenW / 2 - 140, 35, 22, GOLD);

                    // Prev / Next Page Touch Buttons
                    Rectangle btnPrev = { 40, (float)screenH - 80, 140, 50 };
                    Rectangle btnNext = { (float)screenW - 180, (float)screenH - 80, 140, 50 };
                    Rectangle btnClose = { (float)screenW / 2 - 80, (float)screenH - 80, 160, 50 };

                    DrawRectangleRounded(btnPrev, 0.3f, 6, (inventoryPage > 0) ? (Color){ 0, 160, 220, 255 } : DARKGRAY);
                    DrawText("<- PREV", (int)btnPrev.x + 35, (int)btnPrev.y + 16, 16, WHITE);
                    if (inventoryPage > 0 && CheckCollisionPointRec(GetMousePosition(), btnPrev) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) inventoryPage--;

                    DrawRectangleRounded(btnNext, 0.3f, 6, (inventoryPage < 4) ? (Color){ 0, 160, 220, 255 } : DARKGRAY);
                    DrawText("NEXT ->", (int)btnNext.x + 35, (int)btnNext.y + 16, 16, WHITE);
                    if (inventoryPage < 4 && CheckCollisionPointRec(GetMousePosition(), btnNext) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) inventoryPage++;

                    DrawRectangleRounded(btnClose, 0.3f, 6, (Color){ 200, 50, 60, 255 });
                    DrawText("CLOSE", (int)btnClose.x + 50, (int)btnClose.y + 16, 16, WHITE);
                    if (CheckCollisionPointRec(GetMousePosition(), btnClose) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) isInventoryOpen = false;

                    int startIdx = inventoryPage * 10;
                    for (int i = 0; i < 10 && (startIdx + i) < TOTAL_WEAPONS; i++) {
                        int wIdx = startIdx + i;
                        WeaponDef w = weaponCatalog[wIdx];
                        int row = i / 2;
                        int col = i % 2;

                        Rectangle card = { (float)(screenW / 2 - 460 + col * 470), (float)(80 + row * 85), 450.0f, 75.0f };
                        Color cardRankCol;
                        const char* rLbl = GetRankLabel(w.rank, &cardRankCol);

                        DrawRectangleRec(card, (equippedWeapon == wIdx) ? (Color){ 30, 45, 75, 255 } : (Color){ 20, 25, 38, 255 });
                        DrawRectangleLinesEx(card, 2, (equippedWeapon == wIdx) ? GOLD : cardRankCol);

                        DrawText(w.name, (int)card.x + 10, (int)card.y + 8, 14, RAYWHITE);
                        DrawText(TextFormat("[%s]", rLbl), (int)card.x + 360, (int)card.y + 8, 12, cardRankCol);
                        DrawText(TextFormat("DMG: %.0f | Spd: %.2fs", w.damage, w.fireRate), (int)card.x + 10, (int)card.y + 32, 12, LIGHTGRAY);

                        Rectangle btnAction = { card.x + 330, card.y + 32, 105, 32 };
                        if (w.unlocked) {
                            if (equippedWeapon == wIdx) {
                                DrawRectangleRec(btnAction, DARKGRAY);
                                DrawText("EQUIPPED", (int)btnAction.x + 14, (int)btnAction.y + 9, 12, GREEN);
                            } else {
                                DrawRectangleRec(btnAction, (Color){ 0, 160, 220, 255 });
                                DrawText("EQUIP", (int)btnAction.x + 32, (int)btnAction.y + 9, 12, WHITE);
                                if (CheckCollisionPointRec(GetMousePosition(), btnAction) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                    equippedWeapon = wIdx;
                                    saveData.equippedWeaponId = wIdx;
                                    SaveGameData(saveData);
                                }
                            }
                        } else {
                            DrawRectangleRec(btnAction, (goldCoins >= w.unlockCost) ? (Color){ 180, 120, 30, 255 } : DARKGRAY);
                            DrawText(TextFormat("%dg", w.unlockCost), (int)btnAction.x + 35, (int)btnAction.y + 9, 12, (goldCoins >= w.unlockCost) ? GOLD : LIGHTGRAY);
                            if (goldCoins >= w.unlockCost && CheckCollisionPointRec(GetMousePosition(), btnAction) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                goldCoins -= w.unlockCost;
                                weaponCatalog[wIdx].unlocked = true;
                                saveData.unlockedWeapons[wIdx] = true;
                                equippedWeapon = wIdx;
                                saveData.equippedWeaponId = wIdx;
                                SaveGameData(saveData);
                            }
                        }
                    }
                }
            }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}