// Main JavaScript Script
// Contains test functions and utilities

// Basic functionality test
function testBasic() {
    console.log("=== JavaScript Basic Test ===");
    console.log("JavaScript VM is working!");
    console.log("Current time: " + Date.now());
    console.log("Math.PI = " + Math.PI);

    // Test basic math
    var result = Math.sqrt(144) + Math.pow(2, 3);
    console.log("Math test: sqrt(144) + 2^3 = " + result);

    return "Basic test completed successfully";
}

// Console variable tests
function testCvars() {
    console.log("=== CVar Test ===");

    // Test reading cvars
    console.log("js_test_counter = " + cvar.int("js_test_counter"));
    console.log("js_test_string = " + cvar.string("js_test_string"));

    // Test setting cvars
    cvar.set("js_test_counter", "42");
    cvar.set("js_test_string", "modified by JavaScript");

    console.log("After modification:");
    console.log("js_test_counter = " + cvar.int("js_test_counter"));
    console.log("js_test_string = " + cvar.string("js_test_string"));

    return "CVar test completed";
}

// File operations test
function testFileOps() {
    console.log("=== File Operations Test ===");

    try {
        // Test writing to a file
        var testContent = "This file was created by JavaScript!\n";
        testContent += "Timestamp: " + new Date().toISOString() + "\n";
        testContent += "Random number: " + Math.random() + "\n";

        file.save("js_test_output.txt", testContent);
        console.log("Successfully wrote to js_test_output.txt");

        // Test reading a file (if it exists)
        try {
            var content = file.open("js_test_output.txt");
            console.log("File content length: " + content.length + " characters");
        } catch (e) {
            console.log("Could not read file: " + e);
        }

    } catch (e) {
        console.log("File operation failed: " + e);
        return "File test failed: " + e;
    }

    return "File operations test completed";
}

// Console command execution test
function testConsoleCmds() {
    console.log("=== Console Commands Test ===");

    // Execute some console commands
    console.cmd("echo \"JavaScript executed: echo command\"");
    console.cmd("js_test_counter 100");  // This should set the cvar
    console.cmd("cvarlist js_test_*");   // List our test cvars

    console.log("Executed console commands");
    return "Console commands test completed";
}

// Framework loading test
function testFramework() {
    console.log("=== Framework Loading Test ===");

    // Try to load a framework script
    try {
        openjs.file("framework/test_utils.js");
        console.log("Loaded framework/test_utils.js");
    } catch (e) {
        console.log("Failed to load framework script: " + e);
    }

    return "Framework test completed";
}

// QVM call test (if available)
function testQvmCalls() {
    console.log("=== QVM Call Test ===");

    try {
        // This is just a demonstration - actual QVM calls depend on the game
        console.log("QVM constants available:");
        console.log("VM_GAME = " + qvm.game);
        console.log("VM_CGAME = " + qvm.cgame);
        console.log("VM_UI = " + qvm.ui);

        // Example QVM call (this may not work depending on the game)
        // var result = qvm.call(qvm.game, 1, "test argument");

        console.log("QVM call test completed (no actual calls made)");
    } catch (e) {
        console.log("QVM test error: " + e);
        return "QVM test failed: " + e;
    }

    return "QVM test completed";
}

// Master test function
function runAllTests() {
    console.log("=== Running All JavaScript Tests ===");

    var results = [];

    results.push(testBasic());
    results.push(testCvars());
    results.push(testFileOps());
    results.push(testConsoleCmds());
    results.push(testFramework());
    results.push(testQvmCalls());

    console.log("=== Test Results ===");
    for (var i = 0; i < results.length; i++) {
        console.log((i + 1) + ". " + results[i]);
    }

    console.log("=== All Tests Completed ===");
    return "All tests completed. Check console output above.";
}

// JSCall function for QVM integration
function JSCall(funcId, args) {
    console.log("JSCall invoked with funcId: " + funcId);

    // This function can be called from the QVM
    // funcId determines what function to execute
    // args contains the arguments

    switch (funcId) {
        case 1:
            return testBasic();
        case 2:
            return testCvars();
        case 3:
            return testFileOps();
        default:
            return "Unknown function ID: " + funcId;
    }
}

// Export functions to global scope
this.testBasic = testBasic;
this.testCvars = testCvars;
this.testFileOps = testFileOps;
this.testConsoleCmds = testConsoleCmds;
this.testFramework = testFramework;
this.testQvmCalls = testQvmCalls;
this.runAllTests = runAllTests;
this.JSCall = JSCall;