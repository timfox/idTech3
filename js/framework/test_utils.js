// JavaScript Framework Utilities
// This is an example framework script

console.log("Loading test_utils.js framework");

// Utility functions
function clamp(value, min, max) {
    return Math.min(Math.max(value, min), max);
}

function lerp(a, b, t) {
    return a + (b - a) * clamp(t, 0, 1);
}

function randomBetween(min, max) {
    return min + Math.random() * (max - min);
}

function formatTime(ms) {
    var seconds = Math.floor(ms / 1000);
    var minutes = Math.floor(seconds / 60);
    var hours = Math.floor(minutes / 60);

    seconds = seconds % 60;
    minutes = minutes % 60;

    return hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
}

// Vector math utilities
var Vec3 = {
    create: function(x, y, z) {
        return { x: x || 0, y: y || 0, z: z || 0 };
    },

    add: function(a, b) {
        return Vec3.create(a.x + b.x, a.y + b.y, a.z + b.z);
    },

    subtract: function(a, b) {
        return Vec3.create(a.x - b.x, a.y - b.y, a.z - b.z);
    },

    multiply: function(v, scalar) {
        return Vec3.create(v.x * scalar, v.y * scalar, v.z * scalar);
    },

    dot: function(a, b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    },

    length: function(v) {
        return Math.sqrt(Vec3.dot(v, v));
    },

    normalize: function(v) {
        var len = Vec3.length(v);
        if (len > 0) {
            return Vec3.multiply(v, 1 / len);
        }
        return Vec3.create();
    }
};

// Test the utilities
function testUtils() {
    console.log("=== Testing Framework Utilities ===");

    // Test basic utilities
    console.log("clamp(5, 0, 10) = " + clamp(5, 0, 10));
    console.log("clamp(15, 0, 10) = " + clamp(15, 0, 10));
    console.log("lerp(0, 100, 0.5) = " + lerp(0, 100, 0.5));
    console.log("randomBetween(1, 10) = " + randomBetween(1, 10));
    console.log("formatTime(3661000) = " + formatTime(3661000)); // 1h 1m 1s

    // Test vector math
    var v1 = Vec3.create(1, 2, 3);
    var v2 = Vec3.create(4, 5, 6);

    console.log("v1 = (" + v1.x + ", " + v1.y + ", " + v1.z + ")");
    console.log("v2 = (" + v2.x + ", " + v2.y + ", " + v2.z + ")");

    var sum = Vec3.add(v1, v2);
    console.log("v1 + v2 = (" + sum.x + ", " + sum.y + ", " + sum.z + ")");

    var dot = Vec3.dot(v1, v2);
    console.log("v1 · v2 = " + dot);

    var len = Vec3.length(v1);
    console.log("|v1| = " + len);

    var norm = Vec3.normalize(v1);
    console.log("normalized v1 = (" + norm.x + ", " + norm.y + ", " + norm.z + ")");

    return "Framework utilities test completed";
}

// Export functions
this.clamp = clamp;
this.lerp = lerp;
this.randomBetween = randomBetween;
this.formatTime = formatTime;
this.Vec3 = Vec3;
this.testUtils = testUtils;

console.log("test_utils.js framework loaded successfully!");