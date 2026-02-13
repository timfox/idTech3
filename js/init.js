// JavaScript Initialization Script
// This runs automatically when the JS VM starts

console.log("=== JavaScript VM Initialized ===");
console.log("Available APIs:");
console.log("- console.log() - Print to console");
console.log("- console.cmd() - Execute console commands");
console.log("- file.open/save() - File operations");
console.log("- cvar.register/set/int/float/string() - CVar manipulation");
console.log("- openjs.file/folder() - Load JS files/folders");
console.log("- qvm.call() - Call QVM functions");

// Register some test cvars
cvar.register("js_test_counter", "0", 0);
cvar.register("js_test_string", "hello world", 0);

console.log("Test cvars registered!");
console.log("Type 'js.eval \"testBasic()\"' to run basic tests");
console.log("Type 'js.eval \"testFileOps()\"' to test file operations");
console.log("Type 'js.eval \"testCvars()\"' to test cvar operations");