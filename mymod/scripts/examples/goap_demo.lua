-- GOAP demo actions and goal compatible with the C++ planner API

local actions = {
    {
        name = "FindHealth",
        cost = 1,
        requires = {},
        adds = { "has_healthpack" },
        removes = {},
    },
    {
        name = "UseHealth",
        cost = 1,
        requires = { "has_healthpack" },
        adds = { "healthy" },
        removes = { "low_health" },
    },
    {
        name = "Patrol",
        cost = 1,
        requires = {},
        adds = { "enemy_visible" },
        removes = {},
    },
    {
        name = "ChaseEnemy",
        cost = 2,
        requires = { "enemy_visible" },
        adds = { "in_combat" },
        removes = {},
    },
}

local goal = {
    name = "StayAliveAndFight",
    desiredFacts = { "healthy", "in_combat" },
    priority = 1
}

local initialState = {
    facts = { "has_weapon", "low_health" }
}

return {
    actions = actions,
    goal = goal,
    initialState = initialState
}

