# Ratmod (ratoa_gamecode) Feature Analysis for idTech3 Engine Fork

**Date**: February 26, 2026
**Source**: https://github.com/rdntcntrl/ratoa_gamecode
**License**: GPL-2.0
**Documentation**: https://ratmod.github.io/

---

## Executive Summary

Ratmod (RatArena / RatOA) is an open-source mod for OpenArena (ioquake3-based) with ~1,879 commits of active development. It implements a wide range of competitive gameplay, networking, rendering, and quality-of-life improvements — many of which operate at or near the engine level. This document categorizes the most impactful features that could be implemented in our idTech3 engine fork, organized by engine subsystem.

---

## 1. Network Protocol & Anti-Lag (HIGH PRIORITY)

These features directly modify how the engine handles networked gameplay and are the most impactful for competitive play.

### 1.1 Backward Reconciliation (Unlagged)

**What it does**: Server-side lag compensation for hitscan weapons. When a player fires, the server rewinds all other players to where they were at the time the shooter saw them (based on the command timestamp), performs hit detection against those historical positions, then restores everyone to their true state.

**Key components**:
- `G_StoreHistory()` — stores player position + bounding box each server frame in a circular buffer
- `G_TimeShiftClient()` — interpolates between historical states to reconstruct past positions
- `G_DoTimeShiftFor()` — interface function that determines reconciliation timing
- `G_UnTimeShiftClient()` — restores clients to true state after hit testing
- `cg_unlagged.c` — client-side prediction of instant-hit weapon effects (rail trails, bullet impacts)

**Engine impact**: Requires additions to both server (`g_unlagged.c`) and client (`cg_unlagged.c`) game modules. The core mechanism is server-authoritative and does not break the network protocol, but extends snapshot handling.

**Why implement**: This is the single most impactful competitive feature. It makes hitscan weapons feel responsive regardless of ping (up to ~150ms).

### 1.2 Delagged Projectiles

**What it does**: Extends lag compensation beyond hitscan to projectile weapons (rockets, grenades, plasma). While standard Unlagged only compensates hitscan, Ratmod adds projectile delag by adjusting projectile spawn positions and timing to account for the shooter's latency.

**Engine impact**: Server-side projectile spawning logic needs latency-aware adjustments. This is a Ratmod-specific extension not present in vanilla Unlagged.

**Why implement**: Completes the lag compensation story — without this, rocket/plasma fights still feel laggy even with Unlagged.

### 1.3 Latency Equalizer

**What it does**: An optional server-side system that artificially equalizes effective latency across all players. Low-ping players receive slightly delayed feedback to match higher-ping players, creating a level playing field.

**Engine impact**: Server-side command processing and snapshot delivery timing. Can be toggled via server cvar.

**Why implement**: Controversial but highly valued in competitive settings where ping differences are significant. Should be optional and off by default.

### 1.4 Improved Local Prediction

**What it does**: Enhanced client-side prediction of various game events to reduce perceived latency. Goes beyond standard Quake 3 prediction to cover more edge cases and game states.

**Engine impact**: Client-side prediction code in cgame module.

**Why implement**: Reduces perceived latency and makes the game feel more responsive at all ping levels.

### 1.5 True Ping Calculation

**What it does**: More accurate ping measurement that reflects actual round-trip time, accounting for server processing delays and snapshot timing.

**Engine impact**: Modifications to client/server ping measurement code.

**Why implement**: Accurate ping display helps players and server admins assess connection quality.

---

## 2. Rendering Enhancements (MEDIUM-HIGH PRIORITY)

These features modify the visual rendering pipeline and are relevant to our Vulkan/OpenGL renderer work.

### 2.1 Bright Player Outlines / Brightshells

**What it does**: Renders colored outlines or bright shell overlays on player models for improved visibility. Highly configurable per-player with hue values (0-360 via `color2` cvar). Can be set independently for enemies and teammates.

**Key cvars**:
- `cg_enemyModel` — model override for enemy players (e.g., `smarine/bright`)
- `cg_teamModel` — model override for teammates
- `color2 H<value>` — outline/shell hue (0-360)

**Engine impact**: Requires a post-processing or multi-pass rendering approach:
- Option A: Stencil-buffer outline pass (render model to stencil, expand, draw solid color where stencil differs)
- Option B: Shader-based bright shell (additive pass with scaled-up model using vertex normals)

**Why implement**: Essential for competitive play visibility. This is one of the most requested features in any Quake 3 mod. Our Vulkan renderer is well-positioned to implement this efficiently.

### 2.2 Friend Markers Through Walls

**What it does**: Renders teammate indicators (name, health bar, armor value) that are visible through walls. Shows real-time health/armor status for team coordination.

**Engine impact**: Requires depth-test-disabled rendering pass for 2D overlay elements projected to 3D positions. Needs server-to-client health/armor data for teammates (may require extending snapshot data).

**Why implement**: Major quality-of-life improvement for team modes. Reduces the need for voice communication about teammate status.

### 2.3 Widescreen HUD Support

**What it does**: HUD elements properly scaled and positioned for widescreen (16:9, 21:9) aspect ratios instead of being stretched from 4:3.

**Key cvars**:
- `cg_ratStatusbar 1-5` — multiple statusbar layout options
- `cg_drawHabarDecor` — toggle decorative HUD elements
- `cg_drawHabarBackground` — toggle statusbar background

**Engine impact**: HUD rendering coordinate system changes. Aspect-ratio-aware UI scaling.

**Why implement**: Modern displays are widescreen. Stretched HUD is a constant complaint. This is a fundamental engine-level improvement.

### 2.4 Improved Weapon Visuals

**What it does**: Enhanced visual effects for rail trails, rocket trails, lightning gun beam, and grenade explosions. Configurable trail styles and effects.

**Key cvars**:
- `cg_noprojectileTrail` — disable projectile trails for performance
- Various rail effect settings

**Engine impact**: Particle system and beam rendering improvements. Mostly cgame-level but benefits from renderer support for additive blending and particle batching.

**Why implement**: Visual polish that improves both aesthetics and competitive clarity.

### 2.5 High-Resolution Icons and Font

**What it does**: Replaces low-resolution Quake 3 icons and fonts with higher-resolution versions for modern displays.

**Engine impact**: Asset loading and text rendering. Our ImGui integration may already handle some of this.

**Why implement**: Essential for modern displays where original Q3 assets look pixelated.

---

## 3. Physics & Gameplay Engine (MEDIUM PRIORITY)

These modify the core movement and game physics, sitting at the boundary between engine and game logic.

### 3.1 Missiles Through Teleporters

**What it does**: Projectiles (rockets, grenades, plasma) can pass through teleporters and maintain their trajectory on the other side, properly transformed to match the teleporter's exit orientation.

**Engine impact**: Teleporter entity handling code needs to process non-player entities. Requires proper velocity/direction transformation based on teleporter orientation.

**Why implement**: Adds significant depth to map strategy. Missing in vanilla Q3 and frequently requested.

### 3.2 Grenades Affected by Jumppads

**What it does**: Grenades (and potentially other projectiles) are affected by jumppad physics, getting launched by them like players are.

**Engine impact**: Jumppad trigger code needs to handle projectile entities in addition to player entities.

**Why implement**: Creates interesting gameplay dynamics and is more physically intuitive.

### 3.3 Configurable Movement Physics

**What it does**: Server-configurable physics modes:
- **Additive jump**: Vertical velocity is added to current velocity rather than replacing it
- **Rampjump**: Players gain speed from jumping off ramps (similar to CPMA)
- **Ratmode**: Enhanced air control allowing more direction changes mid-air

**Engine impact**: Player movement code (`PM_*` functions in `bg_pmove.c`). These are typically implemented as physics flags or cvar-controlled branches.

**Why implement**: Movement physics customization is one of the most popular mod features. Supporting multiple physics modes at the engine level makes the engine more versatile.

### 3.4 Pause/Unpause

**What it does**: Ability to pause and unpause the game, freezing all game state including physics, timers, and respawns.

**Engine impact**: Server-side game loop pausing. Requires proper handling of client connections during pause (keep-alive packets, timeout prevention).

**Why implement**: Essential for competitive matches (player disconnects, rule disputes) and LAN events.

---

## 4. Server Infrastructure (MEDIUM PRIORITY)

Server-side systems for managing players, teams, and game flow.

### 4.1 Team Queue System

**What it does**: Enforces equal team sizes by only allowing players to join teams in pairs. When teams become unequal (player disconnect), the last player to join the larger team is moved to a queue. Queued players are displayed on the scoreboard with team-colored text.

**Engine impact**: Server-side team management logic. Extends the `\team` command with a queue state (`\team q`). Requires scoreboard data extensions.

**Why implement**: Solves the perennial problem of unbalanced teams without requiring admin intervention.

### 4.2 Team Balance System

**What it does**: Automatic team balancing that considers player skill/score when assigning teams. Works alongside the queue system.

**Engine impact**: Server-side team assignment logic. May integrate with a basic skill rating system.

**Why implement**: Fair teams without admin micromanagement.

### 4.3 Enhanced Callvote System

**What it does**: Comprehensive voting system with configurable vote options:
- Game type changes (`\cv custom xterm`, `\cv custom th`)
- Map voting with improved end-of-game map vote menu
- Custom game modifiers (queues, physics modes, etc.)

**Engine impact**: Server-side vote handling, client-side vote UI. Extensible vote registration system.

**Why implement**: Empowers players to manage their own game experience. Reduces admin burden.

### 4.4 Multitournament Mode

**What it does**: Multiple simultaneous 1v1 matches on a single server. Automatic pairing based on win/loss ratios. All matches start/stop together with overtime support. Spectators can choose which match to watch (`\specgame [id]`).

**Engine impact**: Significant server-side game state partitioning. Each match needs independent scoring, timing, and spawn management. Client needs match selection UI.

**Why implement**: Excellent for tournament organization and pickup games. Efficient server utilization.

---

## 5. Quality of Life (LOWER PRIORITY but high impact)

Features that improve the overall player experience.

### 5.1 Ping Feature for Team Coordination

**What it does**: Players can "ping" a location on the map visible to teammates, similar to modern battle royale communication systems.

**Engine impact**: Client-side input binding + server-side world-position broadcast to team. Client-side 3D marker rendering.

**Why implement**: Modern communication feature that reduces reliance on voice chat.

### 5.2 New Announcer and Awards/Medals

**What it does**: Additional announcer voice lines and new award/medal types beyond the vanilla Q3 set.

**Engine impact**: Mostly asset-level but requires extending the award tracking and announcement systems in the game module.

**Why implement**: Adds game feel and competitive feedback.

### 5.3 Spectator Improvements

**What it does**:
- `\followauto` — automatic cycling through players
- `\specgame [id]` — spectate specific match in multitournament
- Enhanced spectator HUD with more information

**Engine impact**: Client-side spectator mode code and HUD rendering.

**Why implement**: Better spectator experience benefits streaming and tournament broadcasting.

### 5.4 Configurable Map Vote Menu

**What it does**: Improved end-of-game map selection interface with visual map previews and better UX.

**Engine impact**: Client-side UI rendering. Could leverage our ImGui integration.

**Why implement**: Better than the vanilla text-only map vote.

---

## 6. Game Type Extensions (ENGINE FRAMEWORK)

While these are primarily game logic, they demonstrate the need for engine-level gametype extensibility.

| Game Type | Description | Engine Requirement |
|-----------|-------------|-------------------|
| **Extermination (XTERM)** | Team elimination with escalating respawn delays | Round-based game state management |
| **FreezeTag** | Frozen players can be thawed by teammates | Entity state extensions (frozen flag) |
| **Multitournament** | Multiple simultaneous 1v1s | Game state partitioning |
| **Treasure Hunter** | Hide-and-seek with tokens | New entity types, phase-based rounds |
| **CoinFFA / CoinTDM** | Score-based variants | Extended scoring system |

**Engine impact**: The engine needs a flexible gametype registration system that mods can extend without modifying engine code. This is an architectural concern.

---

## Implementation Priority Matrix

| Feature | Engine Impact | Competitive Value | Implementation Effort | Priority |
|---------|--------------|-------------------|----------------------|----------|
| Backward Reconciliation (Unlagged) | High | Critical | Medium | **P0** |
| Delagged Projectiles | High | High | Medium | **P0** |
| Widescreen HUD | Medium | High | Low-Medium | **P1** |
| Bright Outlines / Brightshells | Medium | High | Medium | **P1** |
| Friend Markers Through Walls | Medium | High | Medium | **P1** |
| Missiles Through Teleporters | Low-Medium | Medium | Low | **P1** |
| Latency Equalizer | Medium | Medium | Medium | **P2** |
| Configurable Movement Physics | Medium | High | Medium | **P2** |
| Pause/Unpause | Low-Medium | High | Low | **P2** |
| Team Queue System | Low | High | Low-Medium | **P2** |
| Enhanced Callvote System | Low | Medium | Medium | **P2** |
| Grenades on Jumppads | Low | Low-Medium | Low | **P3** |
| Improved Local Prediction | Medium | Medium | High | **P3** |
| Map Ping Feature | Low | Medium | Low | **P3** |
| Multitournament Mode | High | Medium | High | **P3** |
| High-Res Icons/Fonts | Low | Medium | Low | **P3** |
| Spectator Improvements | Low | Low-Medium | Low | **P3** |

---

## Architectural Recommendations

### Layer Assignment (per CLAUDE.md Constitution)

1. **Chocolate Layer** (enhancement, zero breaking changes):
   - Unlagged / backward reconciliation
   - Delagged projectiles
   - Latency equalizer
   - Widescreen HUD
   - Bright outlines / brightshells
   - Friend markers
   - Improved weapon visuals
   - Pause/unpause

2. **Layer Cake** (modern architecture):
   - Extensible gametype registration system
   - Plugin-based physics modes
   - Configurable vote system framework
   - Multitournament game state partitioning

### Compatibility Considerations

- All features MUST be toggleable via cvars (per Constitution: "Every new feature must include a toggle and startup log line")
- Network protocol extensions must maintain backward compatibility
- Rendering features must work on both OpenGL and Vulkan paths
- Movement physics changes must default to vanilla Q3 behavior

### Reference Implementation Notes

The Ratmod source code at `github.com/rdntcntrl/ratoa_gamecode` is GPL-2.0 licensed, which is compatible with our GPL-licensed engine. Key files to study:

- `code/game/g_unlagged.c` — server-side lag compensation
- `code/cgame/cg_unlagged.c` — client-side prediction
- `code/game/g_team.c` — team management (queue, balance)
- `code/cgame/cg_draw.c` — HUD rendering, friend markers
- `code/cgame/cg_players.c` — bright outlines/shells rendering
- `code/game/bg_pmove.c` — movement physics modifications
- `code/game/g_active.c` — pause/unpause, gameplay modifications

---

## Sources

1. **GitHub Repository**: https://github.com/rdntcntrl/ratoa_gamecode (GPL-2.0, ~1,879 commits)
2. **Official Documentation**: https://ratmod.github.io/
3. **Config Reference**: https://ratmod.github.io/config-command-reference.html
4. **Game Types**: https://ratmod.github.io/game-types.html
5. **Performance Tweaks**: https://ratmod.github.io/tweaks.html
6. **OpenArena Wiki**: https://openarena.fandom.com/wiki/Mods/Ratmod
7. **Unlagged Technical Reference**: https://ra.is/unlagged/code.html
8. **Unlagged Code Walkthroughs**: https://ra.is/unlagged/walkthroughs.html
9. **Q3 Networking Primer**: https://ra.is/unlagged/network.html
