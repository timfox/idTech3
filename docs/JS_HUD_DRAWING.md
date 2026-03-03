# JavaScript HUD Drawing

## Overview

The idTech3 JavaScript (Duktape) runtime exposes HUD drawing functions for custom overlays. These must be called from the **frame** event callback so they execute during the correct render phase.

## When to Call

**Critical:** `hudDrawPic`, `hudDrawText`, `hudSetColor`, `hudDrawRect`, and `hudResetColor` must be invoked from an `idtech3.on("frame", ...)` callback. The frame event fires once per rendered frame, before the game HUD is drawn. Drawing outside this context will not appear or may cause undefined behavior.

## Example: Frame-Based HUD

```javascript
idtech3.on("frame", function() {
  var size = idtech3.getScreenSize();
  idtech3.hudSetColor(1, 1, 0, 0.8);
  idtech3.hudDrawText(10, 10, "Custom HUD", 16);
  idtech3.hudResetColor();
});
```

## Example: Conditional HUD (e.g. in Menu)

```javascript
idtech3.on("frame", function() {
  var menu = idtech3.cvarGet("ui_open");
  if (menu && menu !== "0") {
    idtech3.hudDrawText(320, 240, "Menu Open", 24);
  }
});
```

## API Reference

| Function | Arguments | Description |
|----------|-----------|-------------|
| `hudDrawPic(x, y, w, h, shaderOrName)` | float x, y, w, h; int or string shader | Draw a stretched pic. Pass shader handle (from textureLoad) or texture name. |
| `hudDrawText(x, y, text, size?)` | int x, y; string text; float size (default 8) | Draw text at position. |
| `hudSetColor(r, g, b, a?)` | float r, g, b, a (default 1) | Set draw color for subsequent primitives. |
| `hudDrawRect(x, y, w, h)` | float x, y, w, h | Draw a filled rectangle. |
| `hudResetColor()` | none | Reset color to white. |
| `getScreenSize()` | none | Returns `{width, height}`. |

## Texture Loading

Use `idtech3.textureLoad(path)` to get a shader handle for `hudDrawPic`:

```javascript
var shader = idtech3.textureLoad("gfx/2d/crosshair");
idtech3.on("frame", function() {
  idtech3.hudDrawPic(312, 232, 16, 16, shader);
});
```

## Performance

- Keep frame callbacks lightweight. Heavy work will impact FPS.
- Use `js_frameCallbackBudgetMs` to limit callback time.
- Prefer `textureLoad` once and reuse the handle.

## See Also

- `docs/samples/ui/options_tabs.js` – UI integration example
- `docs/UI_OPEN_TAB.md` – Menu and `ui_open_tab` flow
