// JavaScript UI Scripting Example
// Demonstrates UI manipulation and game integration

console.log("Loading UI scripting example...");

// UI state management
var uiState = {
    menuOpen: false,
    selectedOption: 0,
    animationTime: 0,
    playerHealth: 100,
    playerAmmo: 30
};

// UI helper functions
function createMenu(title, options) {
    console.log("=== " + title + " ===");
    for (var i = 0; i < options.length; i++) {
        var marker = (i === uiState.selectedOption) ? ">" : " ";
        console.log(marker + " " + options[i]);
    }
    console.log("====================");
}

function updateUI() {
    // Simulate updating UI state from game
    uiState.animationTime += 0.016; // ~60 FPS
    uiState.playerHealth = Math.max(0, uiState.playerHealth - Math.random() * 0.1);

    // Update cvars with UI state
    cvar.set("ui_health", Math.floor(uiState.playerHealth));
    cvar.set("ui_ammo", Math.floor(uiState.playerAmmo));
    cvar.set("ui_animation", uiState.animationTime.toFixed(2));
}

function showMainMenu() {
    var options = [
        "Start Game",
        "Load Game",
        "Settings",
        "Quit"
    ];

    createMenu("Main Menu", options);

    // Handle input (simulated)
    if (uiState.selectedOption < 0) uiState.selectedOption = options.length - 1;
    if (uiState.selectedOption >= options.length) uiState.selectedOption = 0;
}

function showHUD() {
    console.log("=== PLAYER HUD ===");
    console.log("Health: " + Math.floor(uiState.playerHealth) + "/100");
    console.log("Ammo: " + Math.floor(uiState.playerAmmo) + "/30");
    console.log("Time: " + uiState.animationTime.toFixed(1) + "s");

    // Visual health bar
    var healthBars = Math.floor((uiState.playerHealth / 100) * 20);
    var healthBar = "";
    for (var i = 0; i < 20; i++) {
        healthBar += (i < healthBars) ? "█" : "░";
    }
    console.log("[" + healthBar + "]");
}

function simulateGameplay() {
    console.log("=== GAMEPLAY SIMULATION ===");

    // Simulate taking damage
    var damage = Math.floor(Math.random() * 20);
    uiState.playerHealth -= damage;
    console.log("Took " + damage + " damage!");

    // Simulate ammo usage
    if (Math.random() < 0.3) { // 30% chance to fire
        uiState.playerAmmo = Math.max(0, uiState.playerAmmo - 1);
        console.log("Fired weapon! Ammo: " + uiState.playerAmmo);
    }

    // Regenerate health slowly
    if (uiState.playerHealth < 100) {
        uiState.playerHealth += 0.5;
    }

    // Low health warning
    if (uiState.playerHealth < 25) {
        console.log("WARNING: Health critically low!");
        console.cmd("play sound/player/lowhealth.wav"); // Simulate sound
    }

    updateUI();
    showHUD();
}

// Menu navigation functions
function menuUp() {
    uiState.selectedOption--;
    showMainMenu();
}

function menuDown() {
    uiState.selectedOption++;
    showMainMenu();
}

function menuSelect() {
    var actions = [
        function() { console.log("Starting game..."); simulateGameplay(); },
        function() { console.log("Loading game..."); },
        function() { console.log("Opening settings..."); },
        function() { console.log("Quitting..."); process.exit(0); }
    ];

    if (actions[uiState.selectedOption]) {
        actions[uiState.selectedOption]();
    }
}

// Register UI commands
function initUICommands() {
    console.log("Registering UI commands...");

    // These would normally be bound to actual input events
    console.cmd("bind UPARROW \"js.eval 'menuUp()'\"");
    console.cmd("bind DOWNARROW \"js.eval 'menuDown()'\"");
    console.cmd("bind ENTER \"js.eval 'menuSelect()'\"");
    console.cmd("bind SPACE \"js.eval 'simulateGameplay()'\"");

    console.log("UI commands registered!");
    console.log("Use arrow keys to navigate menu, Enter to select, Space for gameplay");
}

// Advanced UI features
function createParticleSystem() {
    console.log("=== PARTICLE SYSTEM DEMO ===");

    var particles = [];
    var numParticles = 10;

    // Create particles
    for (var i = 0; i < numParticles; i++) {
        particles.push({
            x: randomBetween(-100, 100),
            y: randomBetween(-100, 100),
            z: randomBetween(-100, 100),
            vx: randomBetween(-5, 5),
            vy: randomBetween(-5, 5),
            vz: randomBetween(-5, 5),
            life: 1.0
        });
    }

    // Simulate particle physics
    for (var frame = 0; frame < 5; frame++) {
        console.log("Frame " + (frame + 1) + ":");

        for (var i = 0; i < particles.length; i++) {
            var p = particles[i];

            // Apply gravity
            p.vy -= 0.5;

            // Update position
            p.x += p.vx;
            p.y += p.vy;
            p.z += p.vz;

            // Apply drag
            p.vx *= 0.98;
            p.vy *= 0.98;
            p.vz *= 0.98;

            // Update life
            p.life -= 0.1;

            if (p.life > 0) {
                console.log("  Particle " + i + ": pos(" +
                    p.x.toFixed(1) + ", " +
                    p.y.toFixed(1) + ", " +
                    p.z.toFixed(1) + ") life:" + p.life.toFixed(2));
            }
        }
    }

    console.log("Particle simulation complete");
}

// Save/Load UI state
function saveUIState() {
    var stateData = JSON.stringify(uiState);
    try {
        file.save("ui_state.json", stateData);
        console.log("UI state saved to ui_state.json");
    } catch (e) {
        console.log("Failed to save UI state: " + e);
    }
}

function loadUIState() {
    try {
        var stateData = file.open("ui_state.json");
        uiState = JSON.parse(stateData);
        console.log("UI state loaded from ui_state.json");
    } catch (e) {
        console.log("Failed to load UI state (using defaults): " + e);
    }
}

// Export functions
this.showMainMenu = showMainMenu;
this.showHUD = showHUD;
this.simulateGameplay = simulateGameplay;
this.menuUp = menuUp;
this.menuDown = menuDown;
this.menuSelect = menuSelect;
this.initUICommands = initUICommands;
this.createParticleSystem = createParticleSystem;
this.saveUIState = saveUIState;
this.loadUIState = loadUIState;
this.updateUI = updateUI;

console.log("UI scripting example loaded!");
console.log("Try: js.eval 'showMainMenu()' or js.eval 'simulateGameplay()'");