/*
 * idtech3_demo - minimal "game code" hooks using the engine's idtech3 JS API.
 * Not a product; demonstrates map_load + frame callbacks and optional HUD text.
 *
 * Load with: js_reload scripts/js/demo_hooks.js
 * (usually from demo_js.cfg in this pk3)
 */
(function () {
	if (typeof idtech3 === "undefined" || typeof idtech3.on !== "function") {
		if (typeof print === "function") {
			print("[idtech3_demo] idtech3 API missing - build with USE_DUKTAPE");
		}
		return;
	}

	idtech3.on("map_load", function (ev) {
		var m = ev && (ev.map || ev.s0) ? (ev.map || ev.s0) : "?";
		idtech3.print("[idtech3_demo] map_load: " + m);
	});

	var frames = 0;
	idtech3.on("frame", function () {
		frames++;
		/* Light HUD watermark every ~2s at 60fps - proves frame callbacks + HUD bindings */
		if (frames % 120 === 0 && typeof idtech3.hudDrawText === "function") {
			try {
				idtech3.hudDrawText(8, 24, "idtech3_demo (JS)", 10);
			} catch (e0) {
				/* ignore */
			}
		}
		if (frames % 600 === 0) {
			idtech3.print("[idtech3_demo] frame " + frames + " - director/physics/nav tick in engine (see cl_* cvars)");
		}
	});

	if (typeof print === "function") {
		print("[idtech3_demo] demo_hooks.js: registered map_load + frame");
	}
})();
