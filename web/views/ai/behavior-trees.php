<?php
/**
 * AI Behavior Trees Documentation
 */
$title = 'Behavior Trees - id Tech 3 Documentation';
$breadcrumbs = [
    '/ai' => 'AI',
    '/ai/behavior-trees' => 'Behavior Trees'
];
?>

<h1>AI Behavior Trees</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Behavior Trees provide a hierarchical, modular approach to AI decision making in id Tech 3. They offer more flexibility than traditional finite state machines while remaining easy to design and debug.</p>
    
    <div class="feature-list">
        <h3>Behavior Tree Features</h3>
        <ul>
            <li><strong>Hierarchical Structure:</strong> Tree-based organization</li>
            <li><strong>Modular Nodes:</strong> Reusable behavior components</li>
            <li><strong>Dynamic Execution:</strong> Runtime behavior switching</li>
            <li><strong>Visual Design:</strong> Easy to create and debug</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Node Types</h2>
    
    <h3>Composite Nodes</h3>
    <p>Control flow nodes that manage child execution:</p>
    <div class="code-block">
        <pre><code>typedef enum {
    BT_SELECTOR,     // OR - Execute until one succeeds
    BT_SEQUENCE,     // AND - Execute all in order
    BT_PARALLEL,     // Execute multiple children simultaneously
    BT_RANDOM,       // Execute random child
    BT_PRIORITY      // Execute highest priority available
} bt_composite_t;

// Composite node structure
typedef struct bt_composite_s {
    bt_node_type_t type;
    int num_children;
    struct bt_node_s **children;
    int current_child;       // Currently executing child
    bt_status_t last_status; // Last execution result
} bt_composite_t;</code></pre>
    </div>
    
    <h3>Decorator Nodes</h3>
    <p>Modify or control single child execution:</p>
    <div class="code-block">
        <pre><code>typedef enum {
    BT_INVERTER,     // Invert child result
    BT_SUCCEEDER,    // Always return success
    BT_FAILER,       // Always return failure
    BT_REPEATER,     // Repeat child N times
    BT_RETRY,        // Retry child until success
    BT_TIMER,        // Add time-based execution
    BT_COOLDOWN      // Prevent execution for time period
} bt_decorator_t;

// Decorator node structure
typedef struct bt_decorator_s {
    bt_node_type_t type;
    struct bt_node_s *child;
    float timer;             // Timer value
    float cooldown_time;     // Cooldown duration
    int repeat_count;        // Repetition counter
    int max_repeats;         // Maximum repetitions
} bt_decorator_t;</code></pre>
    </div>
    
    <h3>Leaf Nodes</h3>
    <p>Action and condition nodes that perform actual work:</p>
    <div class="code-block">
        <pre><code>typedef enum {
    BT_ACTION,       // Perform action
    BT_CONDITION     // Check condition
} bt_leaf_t;

// Action node
typedef struct bt_action_s {
    bt_node_type_t type;
    const char *name;
    bt_status_t (*execute)(struct bot_state_s *bs);
    void *user_data;
} bt_action_t;

// Condition node
typedef struct bt_condition_s {
    bt_node_type_t type;
    const char *name;
    qboolean (*check)(struct bot_state_s *bs);
    void *user_data;
} bt_condition_t;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Execution Status</h2>
    
    <h3>Node Status Types</h3>
    <div class="code-block">
        <pre><code>typedef enum {
    BT_SUCCESS,      // Node completed successfully
    BT_FAILURE,      // Node failed to complete
    BT_RUNNING,      // Node is still executing
    BT_INVALID       // Invalid/error state
} bt_status_t;

// Base node structure
typedef struct bt_node_s {
    bt_node_type_t type;
    const char *name;
    bt_status_t status;
    float last_execution_time;
    struct bt_node_s *parent;
    
    union {
        bt_composite_t composite;
        bt_decorator_t decorator;
        bt_action_t action;
        bt_condition_t condition;
    } data;
} bt_node_t;</code></pre>
    </div>
    
    <h3>Execution Flow</h3>
    <p>Behavior tree execution follows a specific pattern:</p>
    <div class="code-block">
        <pre><code>bt_status_t BT_Execute(bt_node_t *node, bot_state_t *bs) {
    if (!node || !bs) {
        return BT_INVALID;
    }
    
    switch (node->type) {
        case BT_SELECTOR:
            return BT_ExecuteSelector(node, bs);
        case BT_SEQUENCE:
            return BT_ExecuteSequence(node, bs);
        case BT_PARALLEL:
            return BT_ExecuteParallel(node, bs);
        case BT_ACTION:
            return BT_ExecuteAction(node, bs);
        case BT_CONDITION:
            return BT_ExecuteCondition(node, bs);
        default:
            return BT_INVALID;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Common Behavior Patterns</h2>
    
    <h3>Combat Behavior Tree</h3>
    <div class="code-block">
        <pre><code>// Combat behavior tree structure
BT_Selector "Combat Root"
├── BT_Sequence "Emergency Actions"
│   ├── BT_Condition "Health Low"
│   └── BT_Action "Seek Health"
├── BT_Sequence "Engage Enemy"
│   ├── BT_Condition "Enemy Visible"
│   ├── BT_Selector "Attack Options"
│   │   ├── BT_Sequence "Rocket Attack"
│   │   │   ├── BT_Condition "Has Rocket Launcher"
│   │   │   ├── BT_Condition "Safe Distance"
│   │   │   └── BT_Action "Fire Rocket"
│   │   ├── BT_Sequence "Railgun Attack"
│   │   │   ├── BT_Condition "Has Railgun"
│   │   │   ├── BT_Condition "Clear Shot"
│   │   │   └── BT_Action "Fire Railgun"
│   │   └── BT_Action "Fire Current Weapon"
│   └── BT_Action "Move To Attack Position"
└── BT_Action "Patrol"</code></pre>
    </div>
    
    <h3>Navigation Behavior</h3>
    <div class="code-block">
        <pre><code>// Navigation behavior implementation
bt_status_t BT_MoveToPosition(bot_state_t *bs) {
    vec3_t target;
    
    // Get target position from blackboard
    if (!BT_GetBlackboardVec3(bs, "target_position", target)) {
        return BT_FAILURE;
    }
    
    // Check if already at target
    float dist = VectorDistance(bs->origin, target);
    if (dist < 32.0f) {
        return BT_SUCCESS;
    }
    
    // Move towards target
    vec3_t dir;
    VectorSubtract(target, bs->origin, dir);
    VectorNormalize(dir);
    
    // Apply movement
    bs->cmd.forwardmove = 127;
    bs->cmd.rightmove = 0;
    bs->cmd.upmove = 0;
    
    // Set view angles towards target
    VectorToAngles(dir, bs->ideal_viewangles);
    
    return BT_RUNNING;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Blackboard System</h2>
    
    <h3>Shared Memory</h3>
    <p>Blackboard provides shared data storage for behavior tree nodes:</p>
    <div class="code-block">
        <pre><code>typedef struct bt_blackboard_s {
    struct {
        char key[64];
        void *value;
        size_t size;
        int type;
    } entries[MAX_BLACKBOARD_ENTRIES];
    int num_entries;
} bt_blackboard_t;

// Blackboard operations
qboolean BT_SetBlackboardInt(bot_state_t *bs, const char *key, int value);
qboolean BT_SetBlackboardFloat(bot_state_t *bs, const char *key, float value);
qboolean BT_SetBlackboardVec3(bot_state_t *bs, const char *key, vec3_t value);
qboolean BT_SetBlackboardString(bot_state_t *bs, const char *key, const char *value);

qboolean BT_GetBlackboardInt(bot_state_t *bs, const char *key, int *value);
qboolean BT_GetBlackboardFloat(bot_state_t *bs, const char *key, float *value);
qboolean BT_GetBlackboardVec3(bot_state_t *bs, const char *key, vec3_t value);
qboolean BT_GetBlackboardString(bot_state_t *bs, const char *key, char *value, int maxlen);</code></pre>
    </div>
    
    <h3>Context Sharing</h3>
    <p>Example of using blackboard for communication:</p>
    <div class="code-block">
        <pre><code>// Store enemy information
bt_status_t BT_DetectEnemy(bot_state_t *bs) {
    int enemy = BotFindEnemy(bs, -1);
    if (enemy >= 0) {
        // Store enemy info in blackboard
        BT_SetBlackboardInt(bs, "enemy_id", enemy);
        BT_SetBlackboardVec3(bs, "enemy_position", g_entities[enemy].s.origin);
        BT_SetBlackboardFloat(bs, "enemy_distance", 
                             VectorDistance(bs->origin, g_entities[enemy].s.origin));
        return BT_SUCCESS;
    }
    return BT_FAILURE;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Behavior Tree Editor</h2>
    
    <h3>Visual Design Tools</h3>
    <p>Creating behavior trees through visual editors:</p>
    <div class="code-block">
        <pre><code>// Behavior tree definition format
{
    "name": "Bot Combat AI",
    "root": {
        "type": "selector",
        "children": [
            {
                "type": "sequence",
                "name": "Emergency Response",
                "children": [
                    {
                        "type": "condition",
                        "name": "Health Critical",
                        "function": "CheckHealthCritical"
                    },
                    {
                        "type": "action",
                        "name": "Seek Health Pack",
                        "function": "SeekHealthPack"
                    }
                ]
            },
            {
                "type": "action",
                "name": "Combat Behavior",
                "function": "CombatBehavior"
            }
        ]
    }
}</code></pre>
    </div>
    
    <h3>Runtime Compilation</h3>
    <p>Loading and compiling behavior trees at runtime:</p>
    <div class="code-block">
        <pre><code>bt_node_t *BT_LoadFromFile(const char *filename) {
    char *buffer;
    int len = trap_FS_ReadFile(filename, (void **)&buffer);
    
    if (len <= 0) {
        Com_Printf("Failed to load behavior tree: %s\n", filename);
        return NULL;
    }
    
    // Parse JSON/XML format
    bt_node_t *root = BT_ParseTree(buffer);
    
    trap_FS_FreeFile(buffer);
    
    return root;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Optimization</h2>
    
    <h3>Execution Scheduling</h3>
    <div class="code-block">
        <pre><code>// Staggered execution to reduce CPU load
void BT_UpdateBots(void) {
    static int bot_index = 0;
    int bots_per_frame = 2; // Limit bots updated per frame
    
    for (int i = 0; i < bots_per_frame && bot_index < level.maxclients; i++) {
        gentity_t *ent = &g_entities[bot_index];
        
        if (ent->inuse && (ent->r.svFlags & SVF_BOT)) {
            bot_state_t *bs = ent->client->ps.stats[STAT_BOT_STATE];
            if (bs && bs->behavior_tree) {
                BT_Execute(bs->behavior_tree, bs);
            }
        }
        
        bot_index = (bot_index + 1) % level.maxclients;
    }
}</code></pre>
    </div>
    
    <h3>Memory Management</h3>
    <ul>
        <li><strong>Node Pooling:</strong> Reuse node objects</li>
        <li><strong>Lazy Loading:</strong> Load trees on demand</li>
        <li><strong>Memory Limits:</strong> Bound tree complexity</li>
        <li><strong>Garbage Collection:</strong> Clean unused nodes</li>
    </ul>
</div>

<div class="section">
    <h2>Debugging Behavior Trees</h2>
    
    <h3>Debug Visualization</h3>
    <div class="code-block">
        <pre><code># Debug behavior trees
seta bot_debugBehaviorTrees "1"  // Enable BT debugging
seta bot_showBTExecution "1"     // Show execution path
seta bot_BTStepMode "0"          // Step through execution
seta bot_logBTEvents "1"         // Log BT events</code></pre>
    </div>
    
    <h3>Common Issues</h3>
    <div class="troubleshooting">
        <h4>Infinite loops</h4>
        <ul>
            <li>Add execution limits to repeater nodes</li>
            <li>Use proper failure conditions</li>
            <li>Implement timeout decorators</li>
        </ul>
        
        <h4>Performance problems</h4>
        <ul>
            <li>Limit tree depth and complexity</li>
            <li>Use frame-spreading for expensive operations</li>
            <li>Cache frequently accessed data</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="ai/ai">AI Systems Overview</a></li>
        <li><a href="ai/pathfinding">AI Pathfinding</a></li>
        <li><a href="ai/goap">GOAP Implementation</a></li>
        <li><a href="development/scripting">AI Scripting</a></li>
    </ul>
</div> 