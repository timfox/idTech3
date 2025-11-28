<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GOAP Implementation in id Tech 3</title>
    <style>
        @font-face {
            font-family: 'Fusion';
            src: url('Fusion.ttf') format('truetype');
        }
        body {
            font-family: Helvetica, Arial, sans-serif;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background-color: #000;
            color: #0f0;
        }
        code {
            background-color: #111;
            padding: 2px 5px;
            border-radius: 3px;
            color: #0ff;
        }
        pre {
            background-color: #111;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
            color: #0ff;
            border: 1px solid #0f0;
        }
        h1, h2, h3 {
            font-family: 'Fusion', sans-serif;
            color: #f0f;
            text-shadow: 2px 2px #0f0;
        }
        a {
            color: #0ff;
        }
        a:hover {
            color: #f0f;
        }
    </style>
</head>
<body>
    <h1>Goal-Oriented Action Planning (GOAP) Implementation for id Tech 3</h1>
    
    <h2>Overview</h2>
    <p>GOAP implementation leveraging Quake 3's existing AI and bot systems. This implementation extends the bot AI system rather than replacing it.</p>

    <h2>Core Components</h2>
    <pre>
// Extend existing bot AI structures
typedef struct {
    int flags;          // State flags
    float priority;     // Goal priority
    void (*think)(gentity_t *ent); // Goal think function
} botGoal_t;

typedef struct {
    char name[MAX_QPATH];
    float cost;
    qboolean (*precondition)(gentity_t *ent);
    qboolean (*effect)(gentity_t *ent);
    qboolean (*perform)(gentity_t *ent);
} botAction_t;

// World state tracking
typedef struct {
    int flags;          // State flags
    int inventory;      // Inventory state
    int health;         // Health state
    int enemyState;     // Enemy state
} botWorldState_t;</pre>

    <h2>Implementation Steps</h2>

    <h3>1. Action Definition</h3>
    <pre>
// Example action using existing bot functions
static botAction_t seekCoverAction = {
    "SeekCover",
    1.0f,
    BotUnderFire,
    BotInCover,
    BotSeekCover
};

// Implementation using existing bot functions
qboolean BotSeekCover(gentity_t *ent) {
    bot_state_t *bs = &botstates[ent->s.number];
    
    // Use existing bot navigation
    if (BotFindCover(bs)) {
        // Use existing bot movement
        BotMoveToGoal(bs, bs->coverGoal);
        return qtrue;
    }
    return qfalse;
}</pre>

    <h3>2. Goal Planning</h3>
    <pre>
// Extend existing bot AI
void BotGOAPThink(gentity_t *ent) {
    bot_state_t *bs = &botstates[ent->s.number];
    botWorldState_t currentState;
    
    // Update world state using existing bot functions
    UpdateBotState(bs, &currentState);
    
    // Use existing bot goal system
    botGoal_t *currentGoal = BotGetCurrentGoal(bs);
    
    // Plan using existing bot navigation
    if (BotFindPath(bs, currentGoal)) {
        // Execute using existing bot movement
        BotMoveToGoal(bs, bs->goal);
    }
}</pre>

    <h3>3. World State Management</h3>
    <pre>
// Use existing bot state tracking
void UpdateBotState(bot_state_t *bs, botWorldState_t *state) {
    // Use existing bot perception
    state->flags = bs->flags;
    state->inventory = bs->inventory;
    state->health = bs->health;
    state->enemyState = bs->enemyState;
    
    // Use existing bot awareness
    if (BotEntityVisible(bs, bs->enemy, qtrue)) {
        state->flags |= BOT_FLAG_ENEMY_VISIBLE;
    }
}</pre>

    <h2>Integration with Quake 3</h2>
    <ul>
        <li>Use existing bot AI system (ai_main.c, ai_chat.c, etc.)</li>
        <li>Leverage bot navigation (ai_nav.c)</li>
        <li>Utilize bot combat (ai_combat.c)</li>
        <li>Integrate with bot items (ai_items.c)</li>
    </ul>

    <h2>Best Practices</h2>
    <ul>
        <li>Use existing bot functions where possible</li>
        <li>Extend rather than replace bot systems</li>
        <li>Leverage existing navigation and combat</li>
        <li>Use bot waypoint system for planning</li>
    </ul>

    <h2>Example Usage</h2>
    <pre>
// Add to existing bot AI
void BotSpawnInit(bot_state_t *bs) {
    // Initialize GOAP system
    bs->goapEnabled = qtrue;
    bs->currentGoal = NULL;
    
    // Use existing bot initialization
    BotInitAI(bs);
}

// Add to bot think function
void BotThink(bot_state_t *bs) {
    // Use existing bot think
    BotAIThink(bs);
    
    // Add GOAP planning
    if (bs->goapEnabled) {
        BotGOAPThink(bs->ent);
    }
}</pre>

    <h2>Performance Considerations</h2>
    <ul>
        <li>Use existing bot state caching</li>
        <li>Leverage bot waypoint system</li>
        <li>Use existing bot memory management</li>
        <li>Utilize bot prediction system</li>
    </ul>

    <h2>Common Issues and Solutions</h2>
    <ul>
        <li><strong>Planning Time:</strong> Use existing bot waypoint system</li>
        <li><strong>Memory Usage:</strong> Leverage bot memory management</li>
        <li><strong>AI Responsiveness:</strong> Use existing bot prediction</li>
        <li><strong>Path Finding:</strong> Use existing bot navigation</li>
        <li><strong>State Synchronization:</strong> Use existing bot state system</li>
        <li><strong>Resource Management:</strong> Use existing bot resource system</li>
    </ul>

    <h2>Integration with Game Systems</h2>
    <ul>
        <li><strong>Physics:</strong> Use existing bot movement</li>
        <li><strong>Animation:</strong> Use existing bot animations</li>
        <li><strong>Sound:</strong> Use existing bot sound system</li>
        <li><strong>Networking:</strong> Use existing bot networking</li>
    </ul>

    <h2>Debugging Tools</h2>
    <ul>
        <li><strong>Visualization:</strong> Use existing bot debug tools</li>
        <li><strong>Logging:</strong> Use existing bot logging</li>
        <li><strong>Profiling:</strong> Use existing bot profiling</li>
        <li><strong>State Inspection:</strong> Use existing bot state inspection</li>
    </ul>

    <h2>Future Improvements</h2>
    <ul>
        <li>Enhance existing bot decision making</li>
        <li>Improve bot waypoint system</li>
        <li>Extend bot combat system</li>
        <li>Enhance bot item management</li>
    </ul>
</body>
</html>