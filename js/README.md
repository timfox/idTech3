# JavaScript Scripting in idTech3

This directory contains JavaScript scripts that demonstrate the JavaScript scripting capabilities in your idTech3 engine.

## Getting Started

The JavaScript system uses the Duktape engine and is automatically initialized when the engine starts. Scripts are loaded in this order:

1. `js/init.js` - Basic initialization and API overview
2. `js/framework/*.js` - Framework/utility scripts
3. `js/main.js` - Main scripts with test functions

## Console Commands

- `js.open <filename>` - Load and execute a JavaScript file
- `js.eval "<javascript code>"` - Execute JavaScript code directly
- `js.restart` - Restart the JavaScript VM

## Available APIs

### Console Output
```javascript
console.log("Hello World!");
console.cmd("echo JavaScript says hi");
```

### File Operations
```javascript
// Save content to file
file.save("myfile.txt", "Hello World!");

// Load content from file
var content = file.open("myfile.txt");

// Load JavaScript files
openjs.file("myscript.js");
openjs.folder("scripts/", true); // recursive
```

### Console Variables (CVars)
```javascript
// Register a new cvar
cvar.register("my_cvar", "default_value", 0);

// Set cvar value
cvar.set("my_cvar", "new_value");

// Get cvar values
var intVal = cvar.int("my_cvar");
var floatVal = cvar.float("my_cvar");
var strVal = cvar.string("my_cvar");
```

### QVM Integration
```javascript
// Call QVM functions
var result = qvm.call(qvm.game, functionId, arg1, arg2, ...);

// Available VM constants
qvm.game   // VM_GAME
qvm.cgame  // VM_CGAME
qvm.ui     // VM_UI
```

## Test Scripts

### Basic Tests
```bash
# Run basic functionality test
js.eval "testBasic()"

# Test cvar operations
js.eval "testCvars()"

# Test file operations
js.eval "testFileOps()"

# Run all tests
js.eval "runAllTests()"
```

### Framework Tests
```bash
# Test utility functions
js.eval "testUtils()"
```

### UI Examples
```bash
# Load UI example
js.open "ui_example.js"

# Show main menu
js.eval "showMainMenu()"

# Simulate gameplay
js.eval "simulateGameplay()"

# Create particle system demo
js.eval "createParticleSystem()"

# Initialize UI commands (binds keys)
js.eval "initUICommands()"
```

## Error Handling

JavaScript errors are stored in the `js_error` cvar. Check this if scripts fail:

```bash
cvarlist js_error
```

## Creating Your Own Scripts

1. Create a new `.js` file in the `js/` directory
2. Use `js.open "myscript.js"` to load it
3. Or add it to the automatic loading system

Example script structure:
```javascript
// myscript.js

function myFunction() {
    console.log("My custom function!");
    return "success";
}

// Export functions to global scope
this.myFunction = myFunction;

console.log("My script loaded!");
```

## JSCall Integration

For QVM integration, implement a `JSCall` function:

```javascript
function JSCall(funcId, args) {
    switch (funcId) {
        case 1: return myFunction1(args);
        case 2: return myFunction2(args);
        default: return "Unknown function";
    }
}
```

## Tips

- All scripts run in a shared global scope
- Use `console.log()` for debugging output
- File operations are relative to the game directory
- Scripts are automatically reloaded on `js.restart`
- Use try/catch blocks for error handling

## Examples

See the included test scripts for comprehensive examples of all APIs.