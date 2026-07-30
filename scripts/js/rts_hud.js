(function () {
	"use strict";

	var api = idtech3;
	var loadedAt = api.getMilliseconds ? api.getMilliseconds() : 0;

	function drawPanel(x, y, w, h) {
		api.hudSetColor(0.03, 0.04, 0.045, 0.82);
		api.hudDrawRect(x, y, w, h);
		api.hudSetColor(0.46, 0.34, 0.12, 0.95);
		api.hudDrawRect(x, y, w, 1);
		api.hudResetColor();
	}

	function drawText(x, y, text, size) {
		api.hudSetColor(0.92, 0.89, 0.74, 1.0);
		api.hudDrawText(x, y, text, size || 10);
		api.hudResetColor();
	}

	function drawRtsHud() {
		var state;

		if (!api.rts || !api.rts.state) {
			return;
		}
		state = api.rts.state();
		if (!state || !state.entityCount) {
			return;
		}

		drawPanel(372, 8, 260, 42);
		drawText(384, 18, "JS RTS  turn " + state.turn + "  units " + state.entityCount + "  res " + state.resources, 10);

		drawPanel(372, 426, 260, 46);
		if (state.selectedCount > 0) {
			drawText(384, 438, "Selected " + state.selectedCount + "  entity " + state.primarySelection + "  HP " + state.primaryHitpoints, 10);
			drawText(384, 454, "JS API: idtech3.rts.moveSelected(player,x,y)", 8);
		} else {
			drawText(384, 444, "No RTS selection. Try rts_gui_select_all.", 10);
		}
	}

	api.on("frame", drawRtsHud);
	api.print("RTS JavaScript HUD loaded at " + loadedAt + " ms");
}());
