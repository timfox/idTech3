/*
 * UI blur validation scene: CSS-style filter / backdrop-filter blur.
 *
 * Exercises: one filter: blur() image, backdrop panels over the live 3D scene,
 * nested blurred panels, rounded corners, a transformed (rotated) panel, a
 * partially offscreen panel, and several overlapping blur panels.
 *
 * Load with: js_reload scripts/js/demo_ui_blur.js
 * (usually from demo_ui_blur.cfg in this pk3)
 *
 * Debug: ui_filterDebug 1..6, status: ui_blur_status
 */
(function () {
	if (typeof idtech3 === "undefined" || typeof idtech3.on !== "function") {
		if (typeof print === "function") {
			print("[ui_blur] idtech3 API missing - build with USE_DUKTAPE");
		}
		return;
	}
	if (typeof idtech3.hudBackdropBlur !== "function") {
		idtech3.print("[ui_blur] hudBackdropBlur binding missing - engine too old");
		return;
	}

	var imageShader = 0;
	idtech3.on("map_load", function () {
		imageShader = 0; /* re-register after vid/map restarts */
	});

	var t0 = 0;
	idtech3.on("frame", function () {
		var now = (typeof idtech3.getMilliseconds === "function") ? idtech3.getMilliseconds() : 0;
		if (!t0) { t0 = now; }
		var t = (now - t0) * 0.001;

		if (!imageShader && typeof idtech3.materialRegister === "function") {
			try { imageShader = idtech3.materialRegister("console"); } catch (e) { imageShader = 0; }
		}

		/* 1) filter: blur(8px) image - the element itself is blurred. */
		if (imageShader) {
			idtech3.hudFilterBlurPic(24, 40, 120, 90, imageShader, 8, 6, 1.0);
		}

		/* 2) main backdrop panel - blur(18px), rounded corners, dark tint.
		 * Text drawn afterward stays sharp (composed after the blur pass). */
		idtech3.hudBackdropBlur(180, 60, 280, 160, 18, 12, 0.95, 0.05, 0.06, 0.09, 0.35);
		idtech3.hudDrawText(196, 84, "backdrop-filter: blur(18px)", 10);
		idtech3.hudDrawText(196, 100, "text stays sharp", 8);

		/* 3) nested blurred panel inside the main panel. */
		idtech3.hudBackdropBlur(210, 130, 160, 70, 10, 8, 0.9, 0.10, 0.06, 0.12, 0.30);
		idtech3.hudDrawText(220, 152, "nested blur(10px)", 8);

		/* 4) transformed (rotated) panel - rotation animates slowly. */
		idtech3.hudBackdropBlur(480, 90, 130, 90, 14, 10, 0.9,
			0.06, 0.10, 0.06, 0.30, 0.15 * Math.sin(t * 0.7));

		/* 5) partially offscreen panel (left edge). */
		idtech3.hudBackdropBlur(-40, 280, 160, 90, 16, 0, 0.9, 0.10, 0.05, 0.05, 0.30);

		/* 6) several overlapping panels - all reuse one shared backdrop pyramid. */
		idtech3.hudBackdropBlur(320, 300, 140, 90, 20, 8, 0.85, 0.04, 0.04, 0.10, 0.35);
		idtech3.hudBackdropBlur(390, 330, 140, 90, 20, 8, 0.85, 0.10, 0.04, 0.04, 0.35);
		idtech3.hudBackdropBlur(460, 360, 140, 90, 20, 8, 0.85, 0.04, 0.10, 0.04, 0.35);
		idtech3.hudDrawText(330, 320, "overlapping", 8);
	});

	idtech3.print("[ui_blur] validation scene active - ui_filterDebug 1..6, ui_blur_status");
})();
