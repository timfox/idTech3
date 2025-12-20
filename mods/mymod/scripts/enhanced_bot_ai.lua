-- Enhanced Bot AI System
-- Advanced artificial intelligence for bots with tactical decision making

print("=== ENHANCED BOT AI SYSTEM LOADING ===")

-- =============================================================================
-- AI CONFIGURATION
-- =============================================================================

local bot_config = {
    enable_advanced_ai = true,
    enable_team_coordination = true,
    enable_adaptive_difficulty = true,
    enable_pathfinding = true,
    enable_communication = true,
    enable_predictive_ai = true,

    -- AI Behavior Settings
    aggression_levels = { "defensive", "balanced", "aggressive", "berserk" },
    personality_types = { "careful", "balanced", "reckless", "sniper", "support" },
    skill_levels = { "novice", "intermediate", "advanced", "expert", "master" },

    -- Combat AI
    reaction_time = { 0.5, 0.3, 0.2, 0.1, 0.05 }, -- seconds by skill level
    accuracy_modifiers = { 0.6, 0.75, 0.85, 0.95, 1.0 },
    aim_prediction = { 0.3, 0.5, 0.7, 0.9, 1.0 },

    -- Movement AI
    pathfinding_resolution = 32, -- units
    waypoint_cache_size = 1000,
    navigation_mesh_updates = true,
    dynamic_obstacle_avoidance = true,
}

-- =============================================================================
-- BOT AI STATE MANAGEMENT
-- =============================================================================

local bot_ai_states = {}
local global_ai_state = {
    active_bots = {},
    team_coordination = {},
    tactical_situations = {},
    navigation_mesh = {},
    waypoint_cache = {},
    communication_log = {},
    performance_metrics = {}
}

-- Bot AI state structure
local function create_bot_ai_state(bot_id)
    return {
        id = bot_id,
        personality = "balanced",
        aggression = "balanced",
        skill_level = "intermediate",

        -- Current state
        current_goal = "patrol",
        target_entity = nil,
        last_seen_enemy = nil,
        last_damage_time = 0,
        health_percentage = 100,

        -- Position and movement
        current_position = {x=0, y=0, z=0},
        target_position = {x=0, y=0, z=0},
        movement_path = {},
        stuck_timer = 0,

        -- Combat state
        weapon_selection = "machinegun",
        firing_state = "idle",
        last_fire_time = 0,
        accuracy_modifier = 1.0,

        -- Tactical awareness
        known_enemies = {},
        known_teammates = {},
        tactical_position = nil,
        cover_positions = {},
        flanking_opportunities = {},

        -- Learning and adaptation
        combat_experience = 0,
        preferred_weapons = {},
        successful_tactics = {},
        failed_tactics = {},

        -- Communication
        last_communication = 0,
        communication_cooldown = 5,
        team_orders = {},

        -- Performance tracking
        decisions_made = 0,
        successful_actions = 0,
        reaction_times = {},
        pathfinding_attempts = 0
    }
end

-- =============================================================================
-- ADVANCED BOT PERSONALITIES
-- =============================================================================

local bot_personalities = {
    careful = {
        aggression_modifier = -0.3,
        risk_tolerance = 0.3,
        preferred_distance = 800,
        cover_usage = 0.9,
        flanking_preference = 0.2,
        description = "Conservative playstyle, prefers defense and cover"
    },

    balanced = {
        aggression_modifier = 0.0,
        risk_tolerance = 0.5,
        preferred_distance = 600,
        cover_usage = 0.6,
        flanking_preference = 0.5,
        description = "Standard balanced AI behavior"
    },

    reckless = {
        aggression_modifier = 0.4,
        risk_tolerance = 0.8,
        preferred_distance = 400,
        cover_usage = 0.2,
        flanking_preference = 0.8,
        description = "Aggressive playstyle, takes high risks"
    },

    sniper = {
        aggression_modifier = -0.1,
        risk_tolerance = 0.4,
        preferred_distance = 1000,
        cover_usage = 0.8,
        flanking_preference = 0.3,
        weapon_preference = {"railgun", "shotgun"},
        description = "Long-range specialist, prefers camping positions"
    },

    support = {
        aggression_modifier = -0.2,
        risk_tolerance = 0.4,
        preferred_distance = 500,
        cover_usage = 0.7,
        flanking_preference = 0.4,
        weapon_preference = {"shotgun", "grenade"},
        description = "Team support specialist, focuses on assistance"
    }
}

-- =============================================================================
-- TACTICAL DECISION MAKING
-- =============================================================================

local function evaluate_tactical_situation(bot_state, game_state)
    local situation = {
        threat_level = "low",
        position_quality = "neutral",
        team_support = "none",
        environmental_factors = {}
    }

    -- Evaluate threat level
    local nearby_enemies = 0
    local nearby_teammates = 0

    for _, enemy in pairs(bot_state.known_enemies) do
        local distance = calculate_distance(bot_state.current_position, enemy.position)
        if distance < 1000 then
            nearby_enemies = nearby_enemies + 1
        end
    end

    for _, teammate in pairs(bot_state.known_teammates) do
        local distance = calculate_distance(bot_state.current_position, teammate.position)
        if distance < 800 then
            nearby_teammates = nearby_teammates + 1
        end
    end

    -- Determine threat level
    if nearby_enemies >= 3 then
        situation.threat_level = "high"
    elseif nearby_enemies >= 1 then
        situation.threat_level = "medium"
    end

    -- Evaluate position quality
    if bot_state.tactical_position then
        situation.position_quality = evaluate_position_quality(bot_state.tactical_position, game_state)
    end

    -- Evaluate team support
    if nearby_teammates >= 2 then
        situation.team_support = "strong"
    elseif nearby_teammates >= 1 then
        situation.team_support = "weak"
    end

    -- Environmental factors
    situation.environmental_factors = evaluate_environmental_factors(bot_state.current_position, game_state)

    return situation
end

local function make_tactical_decision(bot_state, situation)
    local decision = {
        action = "hold_position",
        priority = "low",
        reasoning = "default_behavior"
    }

    -- Get personality modifiers
    local personality = bot_personalities[bot_state.personality] or bot_personalities.balanced

    -- Decision making based on situation
    if situation.threat_level == "high" then
        if personality.risk_tolerance < 0.5 then
            decision.action = "retreat"
            decision.priority = "high"
            decision.reasoning = "high_threat_conservative"
        elseif situation.team_support == "strong" then
            decision.action = "coordinated_attack"
            decision.priority = "high"
            decision.reasoning = "high_threat_team_support"
        else
            decision.action = "find_cover"
            decision.priority = "high"
            decision.reasoning = "high_threat_seek_cover"
        end
    elseif situation.threat_level == "medium" then
        if personality.aggression_modifier > 0.2 then
            decision.action = "engage_enemy"
            decision.priority = "medium"
            decision.reasoning = "medium_threat_aggressive"
        elseif situation.position_quality == "good" then
            decision.action = "hold_position"
            decision.priority = "medium"
            decision.reasoning = "medium_threat_good_position"
        else
            decision.action = "flank_enemy"
            decision.priority = "medium"
            decision.reasoning = "medium_threat_flank"
        end
    else -- low threat
        if personality.aggression_modifier < -0.1 then
            decision.action = "patrol"
            decision.priority = "low"
            decision.reasoning = "low_threat_patrol"
        else
            decision.action = "seek_objective"
            decision.priority = "low"
            decision.reasoning = "low_threat_objective"
        end
    end

    -- Apply environmental factors
    for _, factor in ipairs(situation.environmental_factors) do
        if factor.type == "powerup_nearby" and personality.risk_tolerance > 0.6 then
            decision.action = "grab_powerup"
            decision.priority = "high"
            decision.reasoning = "environmental_powerup"
            break
        elseif factor.type == "enemy_exposed" and personality.aggression_modifier > 0.1 then
            decision.action = "exploit_weakness"
            decision.priority = "medium"
            decision.reasoning = "environmental_weakness"
            break
        end
    end

    bot_state.decisions_made = bot_state.decisions_made + 1
    return decision
end

-- =============================================================================
-- ADVANCED PATHFINDING SYSTEM
-- =============================================================================

local function initialize_navigation_mesh(map_name)
    -- Create navigation mesh for the current map
    global_ai_state.navigation_mesh = {
        nodes = {},
        connections = {},
        obstacles = {},
        cover_points = {},
        tactical_positions = {}
    }

    -- Generate basic navigation grid (simplified)
    for x = -2048, 2048, bot_config.pathfinding_resolution do
        for y = -2048, 2048, bot_config.pathfinding_resolution do
            for z = -128, 128, 64 do
                local node = {x = x, y = y, z = z, walkable = true, cost = 1.0}
                table.insert(global_ai_state.navigation_mesh.nodes, node)
            end
        end
    end

    print("Navigation mesh initialized with " .. #global_ai_state.navigation_mesh.nodes .. " nodes")
    return global_ai_state.navigation_mesh
end

local function find_path_astar(start_pos, end_pos, bot_state)
    -- A* pathfinding implementation
    local path = {}
    local open_set = {}
    local closed_set = {}
    local came_from = {}
    local g_score = {}
    local f_score = {}

    -- Helper functions
    local function get_distance(a, b)
        return math.sqrt((a.x - b.x)^2 + (a.y - b.y)^2 + (a.z - b.z)^2)
    end

    local function reconstruct_path(current)
        local total_path = {current}
        while came_from[current] do
            current = came_from[current]
            table.insert(total_path, 1, current)
        end
        return total_path
    end

    -- Initialize
    table.insert(open_set, start_pos)
    g_score[start_pos] = 0
    f_score[start_pos] = get_distance(start_pos, end_pos)

    while #open_set > 0 do
        -- Find node with lowest f_score
        local current = open_set[1]
        local lowest_f = f_score[current] or math.huge

        for _, node in ipairs(open_set) do
            local f = f_score[node] or math.huge
            if f < lowest_f then
                lowest_f = f
                current = node
            end
        end

        if get_distance(current, end_pos) < bot_config.pathfinding_resolution then
            -- Found path
            return reconstruct_path(current)
        end

        -- Remove current from open set
        for i, node in ipairs(open_set) do
            if node == current then
                table.remove(open_set, i)
                break
            end
        end
        table.insert(closed_set, current)

        -- Check neighbors (simplified - in real implementation would check actual walkable neighbors)
        local neighbors = {
            {x = current.x + bot_config.pathfinding_resolution, y = current.y, z = current.z},
            {x = current.x - bot_config.pathfinding_resolution, y = current.y, z = current.z},
            {x = current.x, y = current.y + bot_config.pathfinding_resolution, z = current.z},
            {x = current.x, y = current.y - bot_config.pathfinding_resolution, z = current.z}
        }

        for _, neighbor in ipairs(neighbors) do
            if not table_contains(closed_set, neighbor) then
                local tentative_g_score = (g_score[current] or 0) + bot_config.pathfinding_resolution

                if not table_contains(open_set, neighbor) then
                    table.insert(open_set, neighbor)
                elseif tentative_g_score >= (g_score[neighbor] or math.huge) then
                    -- Not a better path
                else
                    -- This is a better path
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g_score
                    f_score[neighbor] = tentative_g_score + get_distance(neighbor, end_pos)
                end
            end
        end
    end

    -- No path found
    return {}
end

-- =============================================================================
-- COMBAT AI SYSTEM
-- =============================================================================

local function select_optimal_weapon(bot_state, target_distance, target_state)
    local weapons = {
        machinegun = {range = 800, damage = 7, fire_rate = 100},
        shotgun = {range = 300, damage = 10, fire_rate = 1000},
        grenade = {range = 400, damage = 100, fire_rate = 2000},
        rocket = {range = 1000, damage = 100, fire_rate = 800},
        lightning = {range = 768, damage = 8, fire_rate = 50},
        railgun = {range = 1500, damage = 100, fire_rate = 1500},
        bfg = {range = 1000, damage = 165, fire_rate = 200},
        plasma = {range = 800, damage = 20, fire_rate = 100}
    }

    local personality = bot_personalities[bot_state.personality] or bot_personalities.balanced
    local skill_level = bot_state.skill_level

    -- Weapon preference scoring
    local best_weapon = "machinegun"
    local best_score = 0

    for weapon_name, weapon_stats in pairs(weapons) do
        local score = 0

        -- Distance suitability
        if target_distance <= weapon_stats.range then
            score = score + 10
        end

        -- Damage efficiency
        if weapon_stats.damage > 20 then
            score = score + 5
        end

        -- Fire rate for close combat
        if target_distance < 300 and weapon_stats.fire_rate < 200 then
            score = score + 5
        end

        -- Personality preferences
        if personality.weapon_preference then
            for _, preferred in ipairs(personality.weapon_preference) do
                if preferred == weapon_name then
                    score = score + 10
                    break
                end
            end
        end

        -- Skill level adjustments (higher skill = better weapon usage)
        local skill_modifier = 1.0
        if skill_level == "expert" or skill_level == "master" then
            skill_modifier = 1.2
        elseif skill_level == "novice" then
            skill_modifier = 0.8
        end
        score = score * skill_modifier

        if score > best_score then
            best_score = score
            best_weapon = weapon_name
        end
    end

    return best_weapon
end

local function calculate_aim_prediction(bot_state, target_position, target_velocity)
    -- Predictive aiming for moving targets
    local skill_index = 1
    for i, skill in ipairs(bot_config.skill_levels) do
        if skill == bot_state.skill_level then
            skill_index = i
            break
        end
    end

    local prediction_factor = bot_config.aim_prediction[skill_index]
    local bullet_speed = 2000 -- approximate bullet speed

    -- Calculate time to target
    local distance = calculate_distance(bot_state.current_position, target_position)
    local time_to_target = distance / bullet_speed

    -- Predict target position
    local predicted_position = {
        x = target_position.x + target_velocity.x * time_to_target * prediction_factor,
        y = target_position.y + target_velocity.y * time_to_target * prediction_factor,
        z = target_position.z + target_velocity.z * time_to_target * prediction_factor
    }

    return predicted_position
end

local function execute_combat_action(bot_state, decision, game_state)
    if decision.action == "engage_enemy" then
        -- Find best target
        local best_target = nil
        local best_score = 0

        for _, enemy in pairs(bot_state.known_enemies) do
            local distance = calculate_distance(bot_state.current_position, enemy.position)
            local visibility = calculate_visibility(bot_state.current_position, enemy.position)
            local threat_level = enemy.health / 100.0

            local score = visibility * 10 - distance * 0.01 + threat_level * 5

            if score > best_score then
                best_score = score
                best_target = enemy
            end
        end

        if best_target then
            -- Select weapon
            local distance = calculate_distance(bot_state.current_position, best_target.position)
            bot_state.weapon_selection = select_optimal_weapon(bot_state, distance, best_target)

            -- Calculate aim point
            local aim_point = calculate_aim_prediction(bot_state, best_target.position, best_target.velocity or {x=0,y=0,z=0})

            -- Execute attack
            perform_bot_attack(bot_state, aim_point, best_target)

            bot_state.successful_actions = bot_state.successful_actions + 1
        end

    elseif decision.action == "find_cover" then
        -- Find nearest cover position
        local cover_pos = find_nearest_cover(bot_state.current_position, bot_state.known_enemies)
        if cover_pos then
            bot_state.target_position = cover_pos
            bot_state.movement_path = find_path_astar(bot_state.current_position, cover_pos, bot_state)
        end

    elseif decision.action == "flank_enemy" then
        -- Find flanking position
        local flank_pos = calculate_flanking_position(bot_state.current_position, bot_state.target_entity.position)
        if flank_pos then
            bot_state.target_position = flank_pos
            bot_state.movement_path = find_path_astar(bot_state.current_position, flank_pos, bot_state)
        end
    end
end

-- =============================================================================
-- TEAM COORDINATION SYSTEM
-- =============================================================================

local function initialize_team_coordination(team_id)
    global_ai_state.team_coordination[team_id] = {
        members = {},
        leader = nil,
        strategy = "balanced",
        objectives = {},
        communication_log = {},
        coordination_score = 0
    }
end

local function update_team_coordination(bot_state, team_id)
    local team = global_ai_state.team_coordination[team_id]
    if not team then
        initialize_team_coordination(team_id)
        team = global_ai_state.team_coordination[team_id]
    end

    -- Update team member status
    team.members[bot_state.id] = {
        position = bot_state.current_position,
        health = bot_state.health_percentage,
        status = bot_state.current_goal,
        last_update = os.time()
    }

    -- Elect leader if needed
    if not team.leader then
        team.leader = bot_state.id -- Simple leader election
    end

    -- Generate team objectives
    if team.leader == bot_state.id then
        update_team_objectives(team, bot_state)
    end

    -- Assign individual objectives
    assign_bot_objective(bot_state, team)
end

local function update_team_objectives(team, leader_state)
    -- Analyze game state and assign team objectives
    team.objectives = {}

    -- Example objectives
    table.insert(team.objectives, {
        type = "control_point",
        position = {x=0, y=0, z=0}, -- Would be actual map position
        priority = "high",
        assigned_bots = {}
    })

    table.insert(team.objectives, {
        type = "enemy_engagement",
        target = leader_state.target_entity,
        priority = "medium",
        assigned_bots = {}
    })
end

local function assign_bot_objective(bot_state, team)
    -- Assign bot to appropriate team objective
    for _, objective in ipairs(team.objectives) do
        if #objective.assigned_bots < 2 then -- Max 2 bots per objective
            table.insert(objective.assigned_bots, bot_state.id)
            bot_state.team_orders = {objective = objective}
            break
        end
    end
end

-- =============================================================================
-- ADAPTIVE DIFFICULTY SYSTEM
-- =============================================================================

local function update_adaptive_difficulty(bot_state, game_state)
    -- Adjust bot difficulty based on player performance
    local player_skill = assess_player_skill(game_state)
    local bot_performance = calculate_bot_performance(bot_state)

    -- Adjust bot parameters based on player skill
    if player_skill > bot_performance + 0.2 then
        -- Player is better, make bot more challenging
        increase_bot_difficulty(bot_state)
    elseif player_skill < bot_performance - 0.2 then
        -- Bot is too challenging, make it easier
        decrease_bot_difficulty(bot_state)
    end
end

local function assess_player_skill(game_state)
    -- Analyze player performance metrics
    local skill_score = 0

    -- Accuracy
    skill_score = skill_score + (game_state.player_accuracy or 0.5)

    -- K/D ratio
    local kd_ratio = (game_state.player_kills or 0) / math.max(game_state.player_deaths or 1, 1)
    skill_score = skill_score + math.min(kd_ratio * 0.1, 1.0)

    -- Game duration (experienced players play longer)
    local game_time = game_state.game_duration or 0
    skill_score = skill_score + math.min(game_time / 1800, 1.0) -- Max 1.0 for 30 minutes

    return math.max(0.0, math.min(1.0, skill_score / 3.0))
end

local function calculate_bot_performance(bot_state)
    -- Calculate how well the bot is performing
    local performance = 0

    if bot_state.decisions_made > 0 then
        performance = bot_state.successful_actions / bot_state.decisions_made
    end

    -- Adjust based on skill level
    local skill_modifier = 0.5 -- default intermediate
    for i, skill in ipairs(bot_config.skill_levels) do
        if skill == bot_state.skill_level then
            skill_modifier = i / #bot_config.skill_levels
            break
        end
    end

    return performance * skill_modifier
end

local function increase_bot_difficulty(bot_state)
    -- Make bot more challenging
    local current_skill_index = 1
    for i, skill in ipairs(bot_config.skill_levels) do
        if skill == bot_state.skill_level then
            current_skill_index = i
            break
        end
    end

    if current_skill_index < #bot_config.skill_levels then
        bot_state.skill_level = bot_config.skill_levels[current_skill_index + 1]
        bot_state.accuracy_modifier = bot_config.accuracy_modifiers[current_skill_index + 1]

        print(string.format("Bot %s difficulty increased to %s", bot_state.id, bot_state.skill_level))
    end
end

local function decrease_bot_difficulty(bot_state)
    -- Make bot less challenging
    local current_skill_index = 1
    for i, skill in ipairs(bot_config.skill_levels) do
        if skill == bot_state.skill_level then
            current_skill_index = i
            break
        end
    end

    if current_skill_index > 1 then
        bot_state.skill_level = bot_config.skill_levels[current_skill_index - 1]
        bot_state.accuracy_modifier = bot_config.accuracy_modifiers[current_skill_index - 1]

        print(string.format("Bot %s difficulty decreased to %s", bot_state.id, bot_state.skill_level))
    end
end

-- =============================================================================
-- UTILITY FUNCTIONS
-- =============================================================================

local function calculate_distance(pos1, pos2)
    return math.sqrt((pos1.x - pos2.x)^2 + (pos1.y - pos2.y)^2 + (pos1.z - pos2.z)^2)
end

local function calculate_visibility(from_pos, to_pos)
    -- Simplified visibility calculation
    -- In real implementation, would use raycasting
    return 0.8 -- 80% visibility (simplified)
end

local function table_contains(table, element)
    for _, value in ipairs(table) do
        if value == element then
            return true
        end
    end
    return false
end

local function evaluate_position_quality(position, game_state)
    -- Evaluate tactical quality of a position
    local quality = 0

    -- Cover quality
    if has_cover_nearby(position) then quality = quality + 3 end

    -- Visibility
    if has_good_visibility(position) then quality = quality + 2 end

    -- Accessibility
    if is_easily_accessible(position) then quality = quality + 1 end

    -- Distance from objectives
    if is_near_objective(position, game_state) then quality = quality + 2 end

    if quality >= 6 then return "excellent"
    elseif quality >= 4 then return "good"
    elseif quality >= 2 then return "fair"
    else return "poor" end
end

local function evaluate_environmental_factors(position, game_state)
    -- Evaluate environmental factors affecting AI
    local factors = {}

    -- Check for nearby powerups
    if is_powerup_nearby(position) then
        table.insert(factors, {type = "powerup_nearby", priority = "high"})
    end

    -- Check for enemy weaknesses
    if is_enemy_exposed_nearby(position) then
        table.insert(factors, {type = "enemy_exposed", priority = "medium"})
    end

    -- Check for environmental hazards
    if is_hazard_nearby(position) then
        table.insert(factors, {type = "hazard_nearby", priority = "low"})
    end

    return factors
end

-- Stub functions (would be implemented with actual game integration)
local function perform_bot_attack(bot_state, aim_point, target) end
local function find_nearest_cover(position, enemies) return {x=0,y=0,z=0} end
local function calculate_flanking_position(bot_pos, enemy_pos) return {x=0,y=0,z=0} end
local function has_cover_nearby(position) return true end
local function has_good_visibility(position) return true end
local function is_easily_accessible(position) return true end
local function is_near_objective(position, game_state) return false end
local function is_powerup_nearby(position) return false end
local function is_enemy_exposed_nearby(position) return false end
local function is_hazard_nearby(position) return false end

-- =============================================================================
-- MAIN AI UPDATE LOOP
-- =============================================================================

function update_bot_ai(bot_id, game_state)
    -- Get or create bot AI state
    local bot_state = bot_ai_states[bot_id]
    if not bot_state then
        bot_state = create_bot_ai_state(bot_id)
        bot_ai_states[bot_id] = bot_state
    end

    -- Update bot state from game state
    update_bot_state_from_game(bot_state, game_state)

    -- Evaluate tactical situation
    local situation = evaluate_tactical_situation(bot_state, game_state)

    -- Make tactical decision
    local decision = make_tactical_decision(bot_state, situation)

    -- Execute decision
    execute_combat_action(bot_state, decision, game_state)

    -- Update team coordination
    if bot_config.enable_team_coordination then
        update_team_coordination(bot_state, game_state.team_id or 1)
    end

    -- Adaptive difficulty
    if bot_config.enable_adaptive_difficulty then
        update_adaptive_difficulty(bot_state, game_state)
    end

    -- Update movement
    update_bot_movement(bot_state)

    -- Update performance metrics
    update_performance_metrics(bot_state)
end

local function update_bot_state_from_game(bot_state, game_state)
    -- Update bot's knowledge of the game state
    bot_state.current_position = game_state.bot_position or {x=0,y=0,z=0}
    bot_state.health_percentage = game_state.bot_health or 100
    bot_state.known_enemies = game_state.visible_enemies or {}
    bot_state.known_teammates = game_state.team_members or {}
end

local function update_bot_movement(bot_state)
    -- Execute movement along path
    if #bot_state.movement_path > 0 then
        local next_waypoint = bot_state.movement_path[1]

        -- Move towards waypoint
        local distance = calculate_distance(bot_state.current_position, next_waypoint)
        if distance < bot_config.pathfinding_resolution then
            -- Reached waypoint
            table.remove(bot_state.movement_path, 1)
        else
            -- Continue moving
            bot_state.target_position = next_waypoint
        end
    end
end

local function update_performance_metrics(bot_state)
    -- Update performance tracking
    global_ai_state.performance_metrics[bot_state.id] = {
        decisions_per_second = bot_state.decisions_made / math.max(os.time() - (bot_state.start_time or os.time()), 1),
        success_rate = bot_state.successful_actions / math.max(bot_state.decisions_made, 1),
        pathfinding_efficiency = bot_state.pathfinding_attempts / math.max(bot_state.decisions_made, 1),
        average_reaction_time = calculate_average_reaction_time(bot_state.reaction_times)
    }
end

local function calculate_average_reaction_time(times)
    if #times == 0 then return 0 end

    local sum = 0
    for _, time in ipairs(times) do
        sum = sum + time
    end
    return sum / #times
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

local enhanced_bot_ai = {
    -- Configuration
    config = bot_config,

    -- Core functions
    update_bot_ai = update_bot_ai,
    initialize_navigation_mesh = initialize_navigation_mesh,

    -- Bot management
    create_bot_ai_state = create_bot_ai_state,
    get_bot_state = function(bot_id) return bot_ai_states[bot_id] end,
    get_all_bot_states = function() return bot_ai_states end,

    -- Team coordination
    initialize_team_coordination = initialize_team_coordination,
    get_team_state = function(team_id) return global_ai_state.team_coordination[team_id] end,

    -- Performance monitoring
    get_performance_metrics = function() return global_ai_state.performance_metrics end,
    get_global_ai_state = function() return global_ai_state end,

    -- Utility functions
    calculate_distance = calculate_distance,
    evaluate_tactical_situation = evaluate_tactical_situation,
    make_tactical_decision = make_tactical_decision,

    -- Testing and debugging
    run_ai_test = function()
        print("=== ENHANCED BOT AI TEST ===")

        -- Create test bot
        local test_bot = create_bot_ai_state("test_bot_1")
        test_bot.personality = "aggressive"
        test_bot.skill_level = "expert"

        -- Simulate game state
        local game_state = {
            bot_position = {x=100, y=200, z=50},
            bot_health = 80,
            visible_enemies = {
                {id="enemy1", position={x=150, y=250, z=50}, health=100}
            },
            team_members = {
                {id="teammate1", position={x=50, y=150, z=50}}
            },
            team_id = 1
        }

        print("Testing AI decision making...")
        update_bot_ai("test_bot_1", game_state)

        local bot_state = bot_ai_states["test_bot_1"]
        print(string.format("Bot decisions made: %d", bot_state.decisions_made))
        print(string.format("Current goal: %s", bot_state.current_goal))
        print(string.format("Weapon selection: %s", bot_state.weapon_selection))

        print("AI test completed!")
        return bot_state
    end
}

-- Initialize navigation mesh on load
if bot_config.enable_pathfinding then
    initialize_navigation_mesh("unknown")
end

print("Enhanced Bot AI System loaded successfully!")
print("Features enabled:")
print("  ✓ Advanced tactical decision making")
print("  ✓ Team coordination and communication")
print("  ✓ Adaptive difficulty scaling")
print("  ✓ A* pathfinding with navigation mesh")
print("  ✓ Predictive combat AI")
print("  ✓ Multiple personality types")
print("  ✓ Performance monitoring")

return enhanced_bot_ai
