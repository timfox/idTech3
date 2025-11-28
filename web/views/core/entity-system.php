<?php
/**
 * Entity System - id Tech 3 Game Object Management
 */
$title = 'Entity System - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/entity-system' => 'Entity System'
];
?>

<h1>Entity System - Game Object Management</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 entity system manages all game objects through a hybrid approach combining direct struct access with component-like functionality. Understanding this system is essential for game logic programming and performance optimization.</p>
    
    <div class="feature-list">
        <h3>Entity System Features</h3>
        <ul>
            <li><strong>Unified Entity Structure:</strong> Single struct type for all game objects</li>
            <li><strong>State Synchronization:</strong> Automatic client-server entity sync</li>
            <li><strong>Event System:</strong> Decoupled communication between entities</li>
            <li><strong>Lifecycle Management:</strong> Spawn, think, and cleanup automation</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Core Entity Architecture</h2>
    
    <h3>Entity Structure</h3>
    <div class="code-block">
        <pre><code>// g_local.h - Core entity definition
#define MAX_GENTITIES 1024
#define ENTITYNUM_NONE (MAX_GENTITIES-1)
#define ENTITYNUM_WORLD (MAX_GENTITIES-2)

typedef struct gentity_s {
    entityState_t s;        // Communicated by server to clients
    entityShared_t r;       // Shared by both game and engine
    
    // DO NOT MODIFY ANYTHING ABOVE THIS - SERVER EXPECTS THEM
    
    struct gclient_s* client; // NULL if not a player
    qboolean inuse;
    
    char* classname;        // Set in spawn function
    int spawnflags;
    qboolean neverFree;     // If true, never freed automatically
    int flags;              // FL_* variables
    
    char* model;
    char* model2;
    int freetime;           // Level.time when freed
    int eventTime;          // Events will be cleared after this time
    qboolean freeAfterEvent;
    qboolean unlinkAfterEvent;
    
    qboolean physicsObject; // Physics interaction enabled
    float physicsBounce;    // 1.0 = continuous bounce, 0.0 = no bounce
    int clipmask;           // Brushes with this content value will be collided against
    
    // Movers
    moverState_t moverState;
    int soundPos1;
    int sound1to2;
    int sound2to1;
    int soundPos2;
    int soundLoop;
    gentity_t* parent;
    gentity_t* nextTrain;
    gentity_t* prevTrain;
    
    vec3_t pos1, pos2;
    
    char* message;
    
    int timestamp;          // Body queue sinking, etc
    
    float angle;            // Set in editor, -1 = up, -2 = down
    char* target;
    char* targetname;
    char* team;
    char* targetShaderName;
    char* targetShaderNewName;
    gentity_t* target_ent;
    
    float speed;
    vec3_t movedir;
    
    int nextthink;
    void (*think)(gentity_t* self);
    void (*reached)(gentity_t* self); // Movers call this when finished
    void (*blocked)(gentity_t* self, gentity_t* other);
    void (*touch)(gentity_t* self, gentity_t* other, trace_t* trace);
    void (*use)(gentity_t* self, gentity_t* other, gentity_t* activator);
    void (*pain)(gentity_t* self, gentity_t* attacker, int damage);
    void (*die)(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod);
    
    int pain_debounce_time;
    int fly_sound_debounce_time; // Wind tunnel
    int last_move_time;
    
    int health;
    qboolean takedamage;
    
    int damage;
    int splashDamage;       // Quad will increase this without increasing radius
    int splashRadius;
    int methodOfDeath;
    int splashMethodOfDeath;
    
    int count;
    
    gentity_t* chain;
    gentity_t* enemy;
    gentity_t* activator;
    gentity_t* teamchain;   // Next entity in team
    gentity_t* teammaster;  // Master of team
    
    int watertype;
    int waterlevel;
    
    int noise_index;
    
    // Timing variables
    float wait;
    float random;
    
    gitem_t* item;          // For bonus items
} gentity_t;</code></pre>
    </div>
    
    <h3>Entity State Synchronization</h3>
    <div class="code-block">
        <pre><code>// Entity state sent to clients
typedef struct entityState_s {
    int number;             // Entity number
    int eType;              // Entity type (ET_GENERAL, ET_PLAYER, etc.)
    int eFlags;             // Entity flags
    
    trajectory_t pos;       // Position interpolation
    trajectory_t apos;      // Angle interpolation
    
    int time;
    int time2;
    
    vec3_t origin;
    vec3_t origin2;
    
    vec3_t angles;
    vec3_t angles2;
    
    int otherEntityNum;     // Player in a tank, etc.
    int otherEntityNum2;
    
    int groundEntityNum;    // -1 = in air
    
    int constantLight;      // R + (G<<8) + (B<<16) + (intensity<<24)
    int loopSound;          // Constantly loop this sound
    
    int modelindex;
    int modelindex2;
    int clientNum;          // 0 to (MAX_CLIENTS - 1), for players and corpses
    int frame;
    
    int solid;              // For client side prediction, trap_linkentity sets this
    
    int event;              // Impulse events -- muzzle flashes, footsteps, etc.
    int eventParm;
    
    // For players
    int powerups;           // Bit flags
    int weapon;             // Determines weapon and flash model, etc.
    int legsAnim;           // Mask off ANIM_TOGGLEBIT
    int torsoAnim;          // Mask off ANIM_TOGGLEBIT
    
    int generic1;
} entityState_t;

// Shared entity information
typedef struct {
    qboolean linked;        // qfalse if not in any good cluster
    int linkcount;
    
    int svFlags;            // SVF_NOCLIENT, SVF_BROADCAST, etc.
    
    vec3_t mins, maxs;      // From worldspawn
    int contents;           // CONTENTS_TRIGGER, CONTENTS_SOLID, etc.
    
    vec3_t absmin, absmax;  // Derived from mins/maxs and origin + rotation
    
    // currentOrigin will be used for all collision detection and world linking.
    // It will not necessarily be the same as the trajectory evaluation for the
    // current time, because each entity must be moved one at a time after
    // time is advanced to avoid simultaneous collision issues
    vec3_t currentOrigin;
    vec3_t currentAngles;
    
    // When a trace call is made and passEntityNum != ENTITYNUM_NONE,
    // an entity will be excluded from testing
    int ownerNum;
} entityShared_t;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Entity Lifecycle Management</h2>
    
    <h3>Entity Spawning System</h3>
    <div class="code-block">
        <pre><code>// g_spawn.c - Entity spawning and management
gentity_t g_entities[MAX_GENTITIES];
gclient_t g_clients[MAX_CLIENTS];

gentity_t* G_Spawn(void) {
    int i, force;
    gentity_t* e;
    
    e = NULL; // Shut up warning
    i = 0;
    
    for (force = 0; force < 2; force++) {
        // If we go through all entities and can't find one to free,
        // override the normal minimum times before freeing
        e = &g_entities[MAX_CLIENTS];
        
        for (i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
            if (e->inuse) {
                continue;
            }
            
            // The first couple seconds of server time can involve a lot
            // of freeing and allocating, so relax the replacement policy
            if (!force && e->freetime > level.time - 2000) {
                continue;
            }
            
            // Reuse this slot
            G_InitGentity(e);
            return e;
        }
        
        if (i != MAX_GENTITIES) {
            break;
        }
    }
    
    if (i == ENTITYNUM_NONE) {
        for (i = 0; i < MAX_GENTITIES; i++) {
            G_Printf("%4i: %s\n", i, g_entities[i].classname);
        }
        G_Error("G_Spawn: no free entities");
    }
    
    // Open up a new slot
    level.num_entities++;
    
    // Let the server system know that there are more entities
    trap_LocateGameData(g_entities, level.num_entities, sizeof(gentity_t),
                       &level.clients[0].ps, sizeof(level.clients[0]));
    
    G_InitGentity(e);
    return e;
}

void G_InitGentity(gentity_t* e) {
    e->inuse = qtrue;
    e->classname = "noclass";
    e->s.number = e - g_entities;
    e->r.ownerNum = ENTITYNUM_NONE;
}

void G_FreeEntity(gentity_t* ed) {
    trap_UnlinkEntity(ed);    // Unlink from world
    
    if (ed->neverFree) {
        return;
    }
    
    memset(ed, 0, sizeof(*ed));
    ed->classname = "freed";
    ed->freetime = level.time;
    ed->inuse = qfalse;
}</code></pre>
    </div>
    
    <h3>Entity Factory System</h3>
    <div class="code-block">
        <pre><code>// Spawn function registry
typedef struct {
    char* name;
    void (*spawn)(gentity_t* ent);
} spawn_t;

spawn_t spawns[] = {
    // Info entities
    {"info_player_start", SP_info_player_start},
    {"info_player_deathmatch", SP_info_player_deathmatch},
    {"info_player_intermission", SP_info_player_intermission},
    {"info_null", SP_info_null},
    {"info_notnull", SP_info_notnull},
    
    // Functional entities
    {"func_plat", SP_func_plat},
    {"func_button", SP_func_button},
    {"func_door", SP_func_door},
    {"func_train", SP_func_train},
    {"func_timer", SP_func_timer},
    
    // Triggers
    {"trigger_always", SP_trigger_always},
    {"trigger_multiple", SP_trigger_multiple},
    {"trigger_push", SP_trigger_push},
    {"trigger_teleport", SP_trigger_teleport},
    {"trigger_hurt", SP_trigger_hurt},
    
    // Targets
    {"target_give", SP_target_give},
    {"target_remove_powerups", SP_target_remove_powerups},
    {"target_delay", SP_target_delay},
    {"target_speaker", SP_target_speaker},
    {"target_print", SP_target_print},
    {"target_laser", SP_target_laser},
    {"target_teleporter", SP_target_teleporter},
    {"target_relay", SP_target_relay},
    {"target_kill", SP_target_kill},
    {"target_position", SP_target_position},
    {"target_location", SP_target_location},
    
    // Lights
    {"light", SP_light},
    
    // Paths
    {"path_corner", SP_path_corner},
    
    // Weapons
    {"weapon_gauntlet", SP_weapon_gauntlet},
    {"weapon_shotgun", SP_weapon_shotgun},
    {"weapon_machinegun", SP_weapon_machinegun},
    
    // Items
    {"item_armor_shard", SP_item_armor_shard},
    {"item_armor_combat", SP_item_armor_combat},
    {"item_health_small", SP_item_health_small},
    {"item_health", SP_item_health},
    {"item_health_large", SP_item_health_large},
    {"item_health_mega", SP_item_health_mega},
    
    {0, 0}
};

qboolean G_CallSpawn(gentity_t* ent) {
    spawn_t* s;
    gitem_t* item;
    
    if (!ent->classname) {
        G_Printf("G_CallSpawn: NULL classname\n");
        return qfalse;
    }
    
    // Check item spawn first
    item = BG_FindItemForClassname(ent->classname);
    if (item) {
        G_SpawnItem(ent, item);
        return qtrue;
    }
    
    // Check normal spawns
    for (s = spawns; s->name; s++) {
        if (!strcmp(s->name, ent->classname)) {
            s->spawn(ent);
            return qtrue;
        }
    }
    
    G_Printf("%s doesn't have a spawn function\n", ent->classname);
    return qfalse;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Entity Events and Communication</h2>
    
    <h3>Event System</h3>
    <div class="code-block">
        <pre><code>// g_combat.c - Entity event handling
void G_AddEvent(gentity_t* ent, int event, int eventParm) {
    gentity_t* other;
    
    if (!event) {
        G_Printf("G_AddEvent: zero event added for entity %d\n", ent->s.number);
        return;
    }
    
    // Clients need to add the event in playerState_t instead of entityState_t
    if (ent->client) {
        ent->client->ps.externalEvent = event;
        ent->client->ps.externalEventParm = eventParm;
        ent->client->ps.externalEventTime = level.time;
    } else {
        ent->s.event = event;
        ent->s.eventParm = eventParm;
    }
    ent->eventTime = level.time;
    
    // Send event to all clients in potentially hearable sight (PHS)
    if (ent->r.svFlags & SVF_BROADCAST) {
        // Send to all clients
        for (int i = 0; i < level.maxclients; i++) {
            other = &g_entities[i];
            if (other->inuse && other->client) {
                other->client->ps.externalEvent = event;
                other->client->ps.externalEventParm = eventParm;
                other->client->ps.externalEventTime = level.time;
            }
        }
    }
}

// Entity use system
void G_UseTargets(gentity_t* ent, gentity_t* activator) {
    gentity_t* t;
    
    if (!ent) {
        return;
    }
    
    if (ent->targetShaderName && ent->targetShaderNewName) {
        float f = level.time * 0.001;
        AddRemap(ent->targetShaderName, ent->targetShaderNewName, f);
        trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
    }
    
    if (!ent->target) {
        return;
    }
    
    t = NULL;
    while ((t = G_Find(t, FOFS(targetname), ent->target)) != NULL) {
        if (t == ent) {
            G_Printf("WARNING: Entity used itself.\n");
        } else {
            if (t->use) {
                t->use(t, ent, activator);
            }
        }
        if (!ent->inuse) {
            G_Printf("entity was removed while using targets\n");
            return;
        }
    }
}

// Entity finding and iteration
gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match) {
    char* s;
    
    if (!from) {
        from = g_entities;
    } else {
        from++;
    }
    
    for (; from < &g_entities[level.num_entities]; from++) {
        if (!from->inuse) {
            continue;
        }
        s = *(char**)((byte*)from + fieldofs);
        if (!s) {
            continue;
        }
        if (!Q_stricmp(s, match)) {
            return from;
        }
    }
    
    return NULL;
}</code></pre>
    </div>
    
    <h3>Entity Targeting System</h3>
    <div class="code-block">
        <pre><code>// Target/targetname entity linking
void G_SetMovedir(vec3_t angles, vec3_t movedir) {
    if (VectorCompare(angles, vec3_origin)) {
        VectorCopy(vec3_origin, movedir);
        return;
    }
    
    if (angles[1] == -1) {
        VectorSet(movedir, 0, 0, 1);
    } else if (angles[1] == -2) {
        VectorSet(movedir, 0, 0, -1);
    } else {
        AngleVectors(angles, movedir, NULL, NULL);
    }
}

void G_InitGentity(gentity_t* e) {
    e->inuse = qtrue;
    e->classname = "noclass";
    e->s.number = e - g_entities;
    e->r.ownerNum = ENTITYNUM_NONE;
    
    // Clear all the targeting fields
    e->target = NULL;
    e->targetname = NULL;
    e->team = NULL;
    e->target_ent = NULL;
}

// Entity team system for grouped entities
void G_FindTeams(void) {
    gentity_t* e, *e2, *chain;
    int i, j;
    int c, c2;
    
    c = 0;
    c2 = 0;
    
    for (i = MAX_CLIENTS, e = g_entities + i; i < level.num_entities; i++, e++) {
        if (!e->inuse) {
            continue;
        }
        if (!e->team) {
            continue;
        }
        if (e->flags & FL_TEAMSLAVE) {
            continue;
        }
        
        chain = e;
        e->teammaster = e;
        c++;
        c2++;
        
        for (j = i + 1, e2 = e + 1; j < level.num_entities; j++, e2++) {
            if (!e2->inuse) {
                continue;
            }
            if (!e2->team) {
                continue;
            }
            if (e2->flags & FL_TEAMSLAVE) {
                continue;
            }
            if (!strcmp(e->team, e2->team)) {
                c2++;
                chain->teamchain = e2;
                e2->teammaster = e;
                chain = e2;
                e2->flags |= FL_TEAMSLAVE;
            }
        }
    }
    
    G_Printf("%i teams with %i entities\n", c, c2);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Entity State Management</h2>
    
    <h3>Entity Thinking System</h3>
    <div class="code-block">
        <pre><code>// g_main.c - Entity thinking and updates
void G_RunFrame(int levelTime) {
    int i;
    gentity_t* ent;
    
    // If we are waiting for the level to restart, do nothing
    if (level.restarted) {
        return;
    }
    
    level.framenum++;
    level.previousTime = level.time;
    level.time = levelTime;
    level.msec = level.time - level.previousTime;
    
    // Update all entities
    ent = &g_entities[0];
    for (i = 0; i < level.num_entities; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }
        
        // Clear events that are too old
        if (level.time - ent->eventTime > EVENT_VALID_MSEC) {
            if (ent->s.event) {
                ent->s.event = 0; // &= EV_EVENT_BITS;
                if (ent->client) {
                    ent->client->ps.externalEvent = 0;
                }
            }
            if (ent->freeAfterEvent) {
                // Tempentities or corpses that disappeared
                G_FreeEntity(ent);
                continue;
            } else if (ent->unlinkAfterEvent) {
                // Items that will respawn
                ent->unlinkAfterEvent = qfalse;
                trap_UnlinkEntity(ent);
            }
        }
        
        // Temporary entities don't think
        if (ent->freeAfterEvent) {
            continue;
        }
        
        if (!ent->r.linked && ent->neverFree) {
            continue;
        }
        
        if (ent->s.eType == ET_MISSILE) {
            G_RunMissile(ent);
            continue;
        }
        
        if (ent->s.eType == ET_ITEM || ent->physicsObject) {
            G_RunItem(ent);
            continue;
        }
        
        if (ent->s.eType == ET_MOVER) {
            G_RunMover(ent);
            continue;
        }
        
        if (i < MAX_CLIENTS) {
            G_RunClient(ent);
            continue;
        }
        
        G_RunThink(ent);
    }
    
    // Perform final fixups on the players
    ent = &g_entities[0];
    for (i = 0; i < level.maxclients; i++, ent++) {
        if (ent->inuse) {
            ClientEndFrame(ent);
        }
    }
    
    // See if it is time to do a tournement restart
    CheckTournament();
    
    // See if it is time to end the level
    CheckExitRules();
    
    // Update to team status?
    CheckTeamStatus();
    
    // Cancel vote if timed out
    CheckVote();
    
    // Check team votes
    CheckTeamVote(TEAM_RED);
    CheckTeamVote(TEAM_BLUE);
    
    // For tracking changes
    CheckCvars();
    
    if (g_listEntity->integer) {
        for (i = 0; i < MAX_GENTITIES; i++) {
            G_Printf("%4i: %s\n", i, g_entities[i].classname);
        }
        trap_Cvar_Set("g_listEntity", "0");
    }
}

void G_RunThink(gentity_t* ent) {
    float thinktime;
    
    thinktime = ent->nextthink;
    if (thinktime <= 0) {
        return;
    }
    if (thinktime > level.time + 1) {
        return;
    }
    
    ent->nextthink = 0;
    if (!ent->think) {
        G_Error("NULL ent->think");
    }
    ent->think(ent);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="core/main-loop">Main Loop Analysis</a></li>
        <li><a href="core/memory-management">Memory Management</a></li>
        <li><a href="gameplay/gameplay">Gameplay Systems</a></li>
        <li><a href="networking/networking">Networking</a></li>
    </ul>
</div>