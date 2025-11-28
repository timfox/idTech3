<?php
/**
 * Physics System Documentation
 */
$title = 'Physics System - id Tech 3 Documentation';
$breadcrumbs = [
    '/physics' => 'Physics',
    '/physics/physics' => 'Physics System'
];
?>

<h1>Physics System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 physics system provides realistic movement, collision detection, and environmental interaction. Built on Quake's traditional physics model, it emphasizes fast-paced gameplay while maintaining predictable and consistent behavior.</p>
    
    <div class="feature-list">
        <h3>Physics Features</h3>
        <ul>
            <li><strong>Predictive Movement:</strong> Client-side prediction with server correction</li>
            <li><strong>Collision Detection:</strong> BSP-based world collision and hull testing</li>
            <li><strong>Projectile Physics:</strong> Ballistic trajectories and impact simulation</li>
            <li><strong>Environmental Effects:</strong> Water, slick surfaces, and jump pads</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Movement Physics</h2>
    
    <h3>Ground Movement</h3>
    <div class="code-block">
        <pre><code>// Movement parameters
#define PM_STEPSIZE         18      // Maximum step height
#define PM_MAXSPEED         320     // Maximum ground speed
#define PM_ACCELERATE       10      // Ground acceleration
#define PM_FRICTION         6       // Ground friction
#define PM_STOPSPEED        100     // Minimum speed before friction applies

// Movement calculation
void PM_GroundMove(void) {
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;
    
    // Calculate wish velocity
    scale = PM_CmdScale(&pm->cmd);
    VectorScale(pml.forward, pm->cmd.forwardmove, wishvel);
    VectorMA(wishvel, pm->cmd.rightmove, pml.right, wishvel);
    wishvel[2] = 0;
    
    // Determine magnitude and direction
    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;
    
    // Apply acceleration and friction
    PM_Accelerate(wishdir, wishspeed, PM_ACCELERATE);
    PM_GroundFriction();
}</code></pre>
    </div>
    
    <h3>Air Movement</h3>
    <div class="code-block">
        <pre><code>// Air movement parameters
#define PM_AIRACCELERATE    1       // Air acceleration (much lower than ground)
#define PM_AIRMAXSPEED      30      // Maximum air control speed
#define PM_GRAVITY          800     // Gravity acceleration

// Air movement allows for strafe jumping
void PM_AirMove(void) {
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;
    
    PM_Friction();
    
    scale = PM_CmdScale(&pm->cmd);
    
    // Project moves down to flat plane
    pml.forward[2] = 0;
    pml.right[2] = 0;
    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);
    
    VectorScale(pml.forward, pm->cmd.forwardmove, wishvel);
    VectorMA(wishvel, pm->cmd.rightmove, pml.right, wishvel);
    
    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;
    
    // Clamp to server defined max speed
    if (wishspeed > PM_AIRMAXSPEED) {
        VectorScale(wishvel, PM_AIRMAXSPEED/wishspeed, wishvel);
        wishspeed = PM_AIRMAXSPEED;
    }
    
    PM_Accelerate(wishdir, wishspeed, PM_AIRACCELERATE);
    
    // Add gravity
    pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Collision Detection</h2>
    
    <h3>World Collision</h3>
    <div class="code-block">
        <pre><code>// Collision trace structure
typedef struct trace_s {
    qboolean    allsolid;       // If true, plane is not valid
    qboolean    startsolid;     // If true, the initial point was in a solid area
    float       fraction;       // Time completed, 1.0 = didn't hit anything
    vec3_t      endpos;         // Final position
    cplane_t    plane;          // Surface normal at impact
    int         surfaceFlags;   // Surface hit
    int         contents;       // Contents on other side of surface
    int         entityNum;      // Entity the surface is on
} trace_t;

// Perform collision trace
void CM_Trace(trace_t *results, const vec3_t start, const vec3_t end,
              const vec3_t mins, const vec3_t maxs, int model, 
              int brushmask, int cylinder) {
    
    // Clear trace results
    memset(results, 0, sizeof(*results));
    results->fraction = 1.0f;
    VectorCopy(end, results->endpos);
    
    if (!cm.numBrushes) {
        return;
    }
    
    // Perform BSP tree traversal for collision
    CM_TraceThroughTree(results, start, end, mins, maxs, 
                       &cm.nodes[model], brushmask, cylinder);
}</code></pre>
    </div>
    
    <h3>Player Collision Hull</h3>
    <div class="code-block">
        <pre><code>// Player bounding box dimensions
vec3_t playerMins = {-15, -15, -24};    // Standing mins
vec3_t playerMaxs = {15, 15, 32};       // Standing maxs
vec3_t playerCrouchMins = {-15, -15, -24}; // Crouching mins  
vec3_t playerCrouchMaxs = {15, 15, 16};    // Crouching maxs

// Player states affecting collision
typedef enum {
    PM_NORMAL,          // Normal ground movement
    PM_FLOAT,           // No gravity (spectator mode)
    PM_NOCLIP,          // No collision
    PM_SPECTATOR,       // Spectator mode
    PM_DEAD,            // Dead player
    PM_FREEZE,          // Frozen in place
    PM_INTERMISSION     // End of level
} pmtype_t;

// Adjust player hull based on state
void PM_CheckBounds(void) {
    if (pm->ps->pm_type == PM_SPECTATOR) {
        VectorSet(pm->mins, -15, -15, -15);
        VectorSet(pm->maxs, 15, 15, 15);
    } else if (pm->ps->eFlags & EF_CROUCHING) {
        VectorCopy(playerCrouchMins, pm->mins);
        VectorCopy(playerCrouchMaxs, pm->maxs);
    } else {
        VectorCopy(playerMins, pm->mins);
        VectorCopy(playerMaxs, pm->maxs);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Projectile Physics</h2>
    
    <h3>Ballistic Trajectories</h3>
    <div class="code-block">
        <pre><code>// Projectile types
typedef enum {
    WP_MACHINEGUN,      // Instant hit (hitscan)
    WP_SHOTGUN,         // Multiple instant pellets
    WP_GRENADE_LAUNCHER,// Bouncing projectile with timer
    WP_ROCKET_LAUNCHER, // Fast projectile with splash
    WP_LIGHTNING,       // Continuous beam
    WP_RAILGUN,         // Instant hit through walls
    WP_PLASMA,          // Medium speed projectile
    WP_BFG              // Large slow projectile
} weapon_t;

// Projectile creation
gentity_t *fire_grenade(gentity_t *self, vec3_t start, vec3_t dir) {
    gentity_t *bolt;
    
    bolt = G_Spawn();
    bolt->classname = "grenade";
    bolt->nextthink = level.time + 2500;    // 2.5 second fuse
    bolt->think = G_ExplodeMissile;
    bolt->s.eType = ET_MISSILE;
    bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    bolt->s.weapon = WP_GRENADE_LAUNCHER;
    bolt->r.ownerNum = self->s.number;
    bolt->parent = self;
    bolt->damage = 100;
    bolt->splashDamage = 100;
    bolt->splashRadius = 150;
    bolt->methodOfDeath = MOD_GRENADE;
    bolt->methodOfSplashDeath = MOD_GRENADE_SPLASH;
    bolt->clipmask = MASK_SHOT;
    
    // Set initial position and velocity
    VectorCopy(start, bolt->s.pos.trBase);
    VectorScale(dir, 700, bolt->s.pos.trDelta);  // Initial velocity
    SnapVector(bolt->s.pos.trDelta);
    
    // Enable gravity and bouncing
    bolt->s.pos.trType = TR_GRAVITY;
    bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;
    
    return bolt;
}</code></pre>
    </div>
    
    <h3>Projectile Movement</h3>
    <div class="code-block">
        <pre><code>// Trajectory types
typedef enum {
    TR_STATIONARY,      // Not moving
    TR_INTERPOLATE,     // Linear interpolation
    TR_LINEAR,          // Linear movement
    TR_LINEAR_STOP,     // Linear movement with stop
    TR_SINE,            // Sine wave movement
    TR_GRAVITY,         // Affected by gravity
    TR_ACCELERATE,      // Constant acceleration
    TR_DECELERATE       // Constant deceleration
} trType_t;

// Update projectile position
void BG_EvaluateTrajectory(const trajectory_t *tr, int atTime, vec3_t result) {
    float deltaTime;
    float phase;
    
    switch (tr->trType) {
    case TR_STATIONARY:
    case TR_INTERPOLATE:
        VectorCopy(tr->trBase, result);
        break;
        
    case TR_LINEAR:
        deltaTime = (atTime - tr->trTime) * 0.001; // Convert to seconds
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        break;
        
    case TR_GRAVITY:
        deltaTime = (atTime - tr->trTime) * 0.001;
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        // Apply gravity: s = ut + 0.5at²
        result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;
        break;
        
    case TR_ACCELERATE:
        deltaTime = (atTime - tr->trTime) * 0.001;
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        // Apply acceleration
        result[0] += 0.5 * tr->trAccel[0] * deltaTime * deltaTime;
        result[1] += 0.5 * tr->trAccel[1] * deltaTime * deltaTime;
        result[2] += 0.5 * tr->trAccel[2] * deltaTime * deltaTime;
        break;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Advanced Movement Techniques</h2>
    
    <h3>Strafe Jumping</h3>
    <div class="code-block">
        <pre><code>// Strafe jumping mechanics
// Players can gain speed by coordinating mouse movement with strafe keys

void PM_AirMove(void) {
    // ... standard air movement code ...
    
    // The key to strafe jumping:
    // 1. Move mouse smoothly left/right while holding strafe key
    // 2. Air acceleration allows speed gain perpendicular to current velocity
    // 3. Maximum speed limited by PM_AIRMAXSPEED for air control
    
    if (wishspeed > PM_AIRMAXSPEED) {
        VectorScale(wishvel, PM_AIRMAXSPEED/wishspeed, wishvel);
        wishspeed = PM_AIRMAXSPEED;
    }
    
    // This limitation prevents infinite acceleration while allowing
    // skilled players to maintain higher speeds through technique
    PM_Accelerate(wishdir, wishspeed, PM_AIRACCELERATE);
}

// Optimal strafe jumping requires:
// - 125 FPS for consistent physics timing
// - Smooth mouse movement (typically 30-45 degree arcs)  
// - Synchronized strafe key presses with mouse direction
// - No forward key press during air movement</code></pre>
    </div>
    
    <h3>Rocket Jumping</h3>
    <div class="code-block">
        <pre><code>// Rocket jumping physics
void G_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
             vec3_t dir, vec3_t point, int damage, int dflags, int mod) {
    
    // Self-damage from explosives
    if (attacker == targ && mod == MOD_ROCKET_SPLASH) {
        // Reduce self-damage
        damage *= 0.5;
        
        // Calculate knockback force
        float knockback = damage * 8;
        if (knockback > 200) knockback = 200;
        
        // Apply upward and directional force
        vec3_t kvel;
        VectorNormalize(dir);
        VectorScale(dir, knockback, kvel);
        
        // Add upward component for rocket jumps
        if (kvel[2] < 0) kvel[2] = 0;
        kvel[2] += knockback * 0.5;
        
        // Apply to player velocity
        VectorAdd(targ->client->ps.velocity, kvel, targ->client->ps.velocity);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Environmental Physics</h2>
    
    <h3>Water Physics</h3>
    <div class="code-block">
        <pre><code>// Water movement mechanics
void PM_WaterMove(void) {
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;
    
    // Check for waterlevel
    if (PM_CheckWaterJump()) {
        PM_WaterJumpMove();
        return;
    }
    
    // Slower movement in water
    scale = PM_CmdScale(&pm->cmd);
    
    // Full 3D movement in water
    VectorScale(pml.forward, pm->cmd.forwardmove, wishvel);
    VectorMA(wishvel, pm->cmd.rightmove, pml.right, wishvel);
    VectorMA(wishvel, pm->cmd.upmove, pml.up, wishvel);
    
    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    
    // Water movement is slower than ground movement
    if (wishspeed > PM_MAXSPEED * PM_WATERSCALE) {
        VectorScale(wishvel, PM_MAXSPEED * PM_WATERSCALE / wishspeed, wishvel);
        wishspeed = PM_MAXSPEED * PM_WATERSCALE;
    }
    wishspeed *= scale;
    
    PM_Accelerate(wishdir, wishspeed, PM_WATERACCELERATE);
    
    // Add gravity in water (reduced)
    if (pm->waterlevel == 1) {
        pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime * 0.5;
    }
}</code></pre>
    </div>
    
    <h3>Jump Pads</h3>
    <div class="code-block">
        <pre><code>// Jump pad trigger
void Touch_JumpPad(gentity_t *self, gentity_t *other, trace_t *trace) {
    vec3_t velocity;
    
    if (!other->client) return;
    
    // Calculate velocity needed to reach target
    VectorCopy(self->s.origin2, velocity);
    
    // Set player velocity
    VectorCopy(velocity, other->client->ps.velocity);
    other->client->ps.pm_time = 160; // Disable control briefly
    other->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    
    // Play sound effect
    G_AddEvent(other, EV_JUMP_PAD, 0);
}

// Jump pad trajectory calculation
void SP_trigger_push(gentity_t *self) {
    vec3_t size;
    float height, time, forward;
    float dist;
    
    // Calculate push velocity based on target location
    VectorSubtract(self->target_ent->s.origin, self->s.origin, self->s.origin2);
    dist = VectorNormalize(self->s.origin2);
    
    // Calculate required upward velocity
    height = self->target_ent->s.origin[2] - self->s.origin[2];
    time = sqrt(height / (0.5 * DEFAULT_GRAVITY));
    if (!time) time = 1;
    
    // Calculate forward velocity  
    forward = dist / time;
    VectorScale(self->s.origin2, forward, self->s.origin2);
    self->s.origin2[2] = time * DEFAULT_GRAVITY;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Network Physics</h2>
    
    <h3>Client Prediction</h3>
    <div class="code-block">
        <pre><code>// Client-side movement prediction
void CL_PredictMovement(void) {
    int frame;
    usercmd_t oldestCmd;
    usercmd_t latestCmd;
    
    if (cls.state != CA_ACTIVE) return;
    if (cl_predict->integer == 0) return;
    
    // Get the most recent command we have
    frame = cls.netchan.outgoingSequence & CMD_MASK;
    latestCmd = cl.cmds[frame];
    
    // Run prediction from last acknowledged server frame
    playerState_t predicted = cl.snap.ps;
    for (int i = cl.snap.serverTime; i < latestCmd.serverTime; i += 8) {
        Pmove(&pmove);
    }
    
    // Check for prediction errors
    if (VectorCompare(predicted.origin, cl.snap.ps.origin) == qfalse) {
        // Prediction error - server position differs from predicted
        if (cl_showMiss->integer) {
            Com_Printf("Prediction miss on frame %i\n", cl.snap.serverTime);
        }
        
        // Smooth out the correction over several frames
        VectorCopy(cl.snap.ps.origin, predicted.origin);
    }
}</code></pre>
    </div>
    
    <h3>Lag Compensation</h3>
    <div class="code-block">
        <pre><code>// Server-side lag compensation for hitscan weapons
void G_TimeShiftAllClients(int time, gentity_t *skip) {
    int i;
    gentity_t *ent;
    
    if (time <= 0) return;
    
    // Move all clients back in time
    for (i = 0; i < level.maxclients; i++) {
        ent = &g_entities[i];
        if (!ent->inuse || ent == skip) continue;
        if (!ent->client) continue;
        
        // Save current position
        VectorCopy(ent->r.currentOrigin, ent->client->saved.origin);
        VectorCopy(ent->r.currentAngles, ent->client->saved.angles);
        ent->client->saved.time = level.time;
        
        // Find position at specified time
        BG_EvaluateTrajectoryDelta(&ent->s.pos, time, ent->r.currentOrigin);
        BG_EvaluateTrajectoryDelta(&ent->s.apos, time, ent->r.currentAngles);
        
        // Update bounding box
        trap_LinkEntity(ent);
    }
}

void G_UnTimeShiftAllClients(gentity_t *skip) {
    int i;
    gentity_t *ent;
    
    // Restore all clients to current time
    for (i = 0; i < level.maxclients; i++) {
        ent = &g_entities[i];
        if (!ent->inuse || ent == skip) continue;
        if (!ent->client) continue;
        
        // Restore saved position
        VectorCopy(ent->client->saved.origin, ent->r.currentOrigin);
        VectorCopy(ent->client->saved.angles, ent->r.currentAngles);
        trap_LinkEntity(ent);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Physics Configuration</h2>
    
    <h3>Movement Variables</h3>
    <div class="code-block">
        <pre><code># Physics-related console variables
seta pmove_fixed "1"             // Fixed physics timestep
seta pmove_msec "8"              // Physics timestep in milliseconds
seta g_knockback "1000"          // Knockback force multiplier
seta g_gravity "800"             // Gravity strength
seta g_speed "320"               // Base movement speed

# Advanced movement settings
seta g_synchronousClients "0"    // Synchronous client updates
seta sv_fps "20"                 // Server physics framerate
seta com_maxfps "125"            // Client framerate cap (affects physics)

# Projectile settings
seta g_projectileNudge "0"       // Projectile spawn nudging
seta g_rocketAcceleration "0"    // Rocket acceleration
seta g_grenadeTime "2.5"         // Grenade fuse time</code></pre>
    </div>
    
    <h3>Collision Settings</h3>
    <div class="code-block">
        <pre><code># Collision and clipping
seta g_maxGameClients "0"        // Maximum clients in game
seta g_allowVote "1"             // Allow voting
seta sv_maxPing "0"              // Maximum ping allowed
seta sv_minPing "0"              // Minimum ping required

# Hit detection
seta g_antiLag "1"               // Anti-lag compensation
seta g_smoothClients "1"         // Smooth client movement
seta sv_maxRate "0"              // Maximum download rate</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="physics/collision">Collision Detection</a></li>
        <li><a href="physics/movement">Movement Systems</a></li>
        <li><a href="networking/prediction">Client Prediction</a></li>
        <li><a href="gameplay/weapons">Weapon Physics</a></li>
    </ul>
</div> 