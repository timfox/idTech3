<?php
/**
 * AI Pathfinding Documentation
 */
$title = 'AI Pathfinding - id Tech 3 Documentation';
$breadcrumbs = [
    '/ai' => 'AI',
    '/ai/pathfinding' => 'Pathfinding'
];
?>

<h1>AI Pathfinding</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 pathfinding system uses the Area Awareness System (AAS) to provide navigation data for AI bots. This system creates a navigation mesh that allows bots to find paths through the game world.</p>
    
    <div class="feature-list">
        <h3>Pathfinding Features</h3>
        <ul>
            <li><strong>AAS Navigation:</strong> Area-based pathfinding system</li>
            <li><strong>Route Planning:</strong> Optimal path calculation</li>
            <li><strong>Dynamic Obstacles:</strong> Real-time path adjustment</li>
            <li><strong>Multi-level Support:</strong> 3D navigation handling</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Area Awareness System (AAS)</h2>
    
    <h3>AAS Structure</h3>
    <p>The AAS divides the game world into navigable areas:</p>
    <div class="code-block">
        <pre><code>typedef struct {
    int numfaces;           // Number of area faces
    int firstface;          // First face index
    vec3_t mins, maxs;      // Area bounding box
    vec3_t center;          // Area center point
    int areanumber;         // Unique area ID
    int presencetype;       // What can be in this area
    int numreachableareas;  // Connected areas count
    int firstreachablearea; // First reachability index
} aas_area_t;</code></pre>
    </div>
    
    <h3>Reachability Information</h3>
    <p>Connections between areas are defined by reachability data:</p>
    <div class="code-block">
        <pre><code>typedef struct {
    int areanum;            // Destination area
    int facenum;            // Face to travel through
    int edgenum;            // Edge to travel along
    vec3_t start;           // Start position
    vec3_t end;             // End position
    int traveltype;         // Type of travel required
    unsigned short int traveltime; // Time to traverse
} aas_reachability_t;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Travel Types</h2>
    
    <h3>Movement Types</h3>
    <div class="code-block">
        <pre><code>// Travel type definitions
#define TRAVEL_INVALID      1   // Invalid travel
#define TRAVEL_WALK         2   // Normal walking
#define TRAVEL_CROUCH       3   // Crouching movement
#define TRAVEL_BARRIERJUMP  4   // Jump over barrier
#define TRAVEL_JUMP         5   // Standard jump
#define TRAVEL_LADDER       6   // Ladder climbing
#define TRAVEL_WALKOFFLEDGE 7   // Walk off edge
#define TRAVEL_SWIM         8   // Swimming
#define TRAVEL_WATERJUMP    9   // Jump out of water
#define TRAVEL_TELEPORT     10  // Teleporter use
#define TRAVEL_ELEVATOR     11  // Elevator travel
#define TRAVEL_ROCKETJUMP   12  // Rocket jumping
#define TRAVEL_BFGJUMP      13  // BFG jumping
#define TRAVEL_GRAPPLEHOOK  14  // Grapple hook
#define TRAVEL_DOUBLEJUMP   15  // Double jump
#define TRAVEL_RAMPJUMP     16  // Ramp jumping
#define TRAVEL_STRAFEJUMP   17  // Strafe jumping
#define TRAVEL_JUMPPAD      18  // Jump pad</code></pre>
    </div>
    
    <h3>Travel Flags</h3>
    <ul>
        <li><strong>TFL_INVALID:</strong> Cannot travel this way</li>
        <li><strong>TFL_WALK:</strong> Normal ground movement</li>
        <li><strong>TFL_AIR:</strong> Requires aerial movement</li>
        <li><strong>TFL_WATER:</strong> Underwater travel</li>
        <li><strong>TFL_SLIME:</strong> Dangerous liquid travel</li>
        <li><strong>TFL_LAVA:</strong> Extremely dangerous travel</li>
    </ul>
</div>

<div class="section">
    <h2>Pathfinding Algorithms</h2>
    
    <h3>Route Calculation</h3>
    <p>The bot routing system uses A* pathfinding with AAS data:</p>
    <div class="code-block">
        <pre><code>// Route calculation
int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end) {
    if (!aasworld.loaded) return 0;
    if (areanum <= 0 || areanum >= aasworld.numareas) return 0;
    
    aas_area_t *area = &aasworld.areas[areanum];
    
    // Calculate base travel time
    float dist = VectorDistance(start, end);
    int time = (int)(dist * 100 / 300); // Assume 300 units/sec
    
    // Apply area modifiers
    if (area->presencetype & PRESENCE_CROUCH) {
        time = (int)(time * 1.5f); // Slower when crouching
    }
    
    return time;
}</code></pre>
    </div>
    
    <h3>Route Caching</h3>
    <p>Frequently used routes are cached for performance:</p>
    <div class="code-block">
        <pre><code>typedef struct bot_routecache_s {
    int type;               // Route type
    float time;             // Creation time
    int areanum;            // Start area
    vec3_t origin;          // Start position
    int goalareanum;        // Goal area
    vec3_t goalorigin;      // Goal position
    int traveltime;         // Total travel time
    struct bot_routecache_s *next;
    struct bot_routecache_s *prev;
} bot_routecache_t;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Bot Navigation Implementation</h2>
    
    <h3>Movement States</h3>
    <div class="code-block">
        <pre><code>// Bot movement state
typedef struct {
    int areanum;            // Current area
    int lastareanum;        // Previous area
    vec3_t origin;          // Current position
    vec3_t viewangles;      // View direction
    vec3_t ideal_viewangles; // Desired view direction
    vec3_t viewangles_diff; // Angle difference
    int reachability_time;  // Time to reach goal
    int areanum_time;       // Time in current area
    int lastgoalareanum;    // Previous goal area
    int lastforwardmove;    // Last forward movement
    int lastsidemove;       // Last side movement
    int attackcrouch_time;  // Attack while crouching
    int jumpreach_time;     // Jump reachability time
    float thinktime;        // AI think time
} bot_movestate_t;</code></pre>
    </div>
    
    <h3>Navigation Functions</h3>
    <div class="code-block">
        <pre><code>// Core navigation functions
int BotChooseMovement(bot_state_t *bs) {
    // Get current position and goal
    vec3_t origin, goal;
    VectorCopy(bs->origin, origin);
    VectorCopy(bs->teamgoal.origin, goal);
    
    // Find route to goal
    int areanum = AAS_PointAreaNum(origin);
    int goalareanum = AAS_PointAreaNum(goal);
    
    if (!areanum || !goalareanum) {
        return qfalse;
    }
    
    // Calculate optimal route
    int traveltime = AAS_AreaTravelTimeToGoalArea(areanum, origin, 
                                                 goalareanum, TFL_DEFAULT);
    
    if (!traveltime) {
        // No valid route found
        return qfalse;
    }
    
    // Get next movement direction
    aas_reachability_t reach;
    int reachnum = AAS_BestReachableArea(areanum, origin, goalareanum, 
                                        TFL_DEFAULT, &reach);
    
    if (reachnum) {
        // Move towards reachability point
        BotMoveTowardsPosition(bs, reach.start);
        return qtrue;
    }
    
    return qfalse;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Advanced Pathfinding</h2>
    
    <h3>Dynamic Obstacles</h3>
    <p>Handling moving obstacles and players:</p>
    <div class="code-block">
        <pre><code>// Obstacle avoidance
int BotAvoidObstacles(bot_state_t *bs, vec3_t dir) {
    vec3_t angles, forward, right;
    
    // Convert direction to angles
    VectorToAngles(dir, angles);
    AngleVectors(angles, forward, right, NULL);
    
    // Check for obstacles in path
    trace_t trace;
    vec3_t end;
    
    VectorMA(bs->origin, 100, forward, end);
    trap_Trace(&trace, bs->origin, NULL, NULL, end, bs->client, MASK_SHOT);
    
    if (trace.fraction < 1.0) {
        // Obstacle detected, try alternate routes
        vec3_t side_dir;
        
        // Try right side
        VectorMA(forward, 0.7f, right, side_dir);
        VectorNormalize(side_dir);
        
        VectorMA(bs->origin, 100, side_dir, end);
        trap_Trace(&trace, bs->origin, NULL, NULL, end, bs->client, MASK_SHOT);
        
        if (trace.fraction > 0.8) {
            VectorCopy(side_dir, dir);
            return qtrue;
        }
        
        // Try left side
        VectorMA(forward, -0.7f, right, side_dir);
        VectorNormalize(side_dir);
        
        VectorMA(bs->origin, 100, side_dir, end);
        trap_Trace(&trace, bs->origin, NULL, NULL, end, bs->client, MASK_SHOT);
        
        if (trace.fraction > 0.8) {
            VectorCopy(side_dir, dir);
            return qtrue;
        }
        
        return qfalse; // No clear path
    }
    
    return qtrue; // Path is clear
}</code></pre>
    </div>
    
    <h3>Tactical Pathfinding</h3>
    <ul>
        <li><strong>Cover Seeking:</strong> Find paths that provide cover</li>
        <li><strong>Flanking Routes:</strong> Alternative attack paths</li>
        <li><strong>Retreat Paths:</strong> Emergency escape routes</li>
        <li><strong>Ambush Points:</strong> Strategic positioning</li>
    </ul>
</div>

<div class="section">
    <h2>AAS File Generation</h2>
    
    <h3>BSPC Tool</h3>
    <p>The Bot Strategy Programming Code (BSPC) tool generates AAS files:</p>
    <div class="code-block">
        <pre><code># Generate AAS file from BSP
bspc -bsp2aas mymap.bsp
bspc -reach mymap.bsp        # Calculate reachabilities
bspc -cluster mymap.bsp      # Create area clusters
bspc -optimize mymap.bsp     # Optimize navigation data</code></pre>
    </div>
    
    <h3>AAS Optimization</h3>
    <ul>
        <li><strong>Area Merging:</strong> Combine small adjacent areas</li>
        <li><strong>Route Optimization:</strong> Pre-calculate common routes</li>
        <li><strong>Memory Usage:</strong> Reduce data size</li>
        <li><strong>Load Time:</strong> Faster AAS loading</li>
    </ul>
</div>

<div class="section">
    <h2>Debugging Pathfinding</h2>
    
    <h3>Debug Visualization</h3>
    <div class="code-block">
        <pre><code># Debug AAS visualization
seta bot_developer "1"        // Enable bot debugging
seta bot_visualizejumppads "1" // Show jump pads
seta bot_forcereachability "1" // Force reachability display
seta g_drawAAS "1"            // Draw AAS areas
seta bot_testsolid "1"        // Test solid areas</code></pre>
    </div>
    
    <h3>Common Issues</h3>
    <div class="troubleshooting">
        <h4>Bots stuck in areas</h4>
        <ul>
            <li>Check AAS area connections</li>
            <li>Verify reachability data</li>
            <li>Regenerate AAS file</li>
        </ul>
        
        <h4>Poor pathfinding performance</h4>
        <ul>
            <li>Optimize route caching</li>
            <li>Reduce AAS complexity</li>
            <li>Limit bot thinking frequency</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="ai/ai">AI Systems Overview</a></li>
        <li><a href="ai/behavior-trees">Behavior Trees</a></li>
        <li><a href="ai/goap">GOAP Implementation</a></li>
        <li><a href="development/map-making">Map Making for AI</a></li>
    </ul>
</div> 