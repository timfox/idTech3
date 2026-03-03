# ui_open_tab: Opening Credits, Audio, and Gameplay from Console

## Overview

When the engine receives `open credits`, `open audio`, or `open gameplay` and no direct menu or UI handler exists, it sets the cvar `ui_open_tab` and opens the main menu. The UI must read this cvar on main menu open and navigate to the requested screen.

## Engine Behavior

- **Cvar**: `ui_open_tab` (CVAR_ARCHIVE_ND)
- **Set by**: `CL_Open_f` in `src/client/cl_main.c` when target is `credits`, `audio`, or `gameplay`
- **Flow**: Engine sets `ui_open_tab` to the target string, then calls `CL_SetActiveMenuByName("main")`
- **UI responsibility**: On main menu open, read `ui_open_tab`, open the appropriate sub-menu, and clear the cvar

## Implementation for Mods

### 1. Add `handleOpenTab` to `ui/options_tabs.js`

If you already have `options_tabs.js` with `switchTab`, add `handleOpenTab` and export it:

```javascript
var UI_OPEN_TAB_CVAR = 'ui_open_tab';

function handleOpenTab() {
  var tab = (idtech3.cvarGet(UI_OPEN_TAB_CVAR) || '').toLowerCase();
  if (!tab) {
    return;
  }

  var cmd = '';
  if (tab === 'credits') {
    cmd = 'close main ; open credits_menu';
  } else if (tab === 'audio') {
    cmd = 'close main ; open options_audio_menu';
  } else if (tab === 'gameplay') {
    cmd = 'close main ; open options_gameplay_menu';
  }

  if (cmd) {
    idtech3.exec(cmd + ' ; seta ' + UI_OPEN_TAB_CVAR + ' ""');
  }
}

module.exports = {
  switchTab: switchTab,        // existing
  handleOpenTab: handleOpenTab
};
```

A full reference implementation is in `docs/samples/ui/options_tabs.js`.

### 2. Add `onOpen` to the main menu

In your main menu `menuDef`, add an `onOpen` handler that calls `handleOpenTab`:

```c
menuDef {
  name "main"
  fullScreen 1
  rect 0 0 640 480
  visible 1
  focusColor 0.98 1.00 1.00 1.00
  onOpen { exec "js_exec idtech3.require('ui/options_tabs').handleOpenTab()" }
  onESC { close main ; open quit_confirm }
  soundLoop "music/etherealmadness.mp3"
  // ... rest of menu items
}
```

### 3. Menu name mapping

Ensure your menu names match the `handleOpenTab` logic:

| ui_open_tab | Menu to open        |
|-------------|---------------------|
| credits     | credits_menu        |
| audio       | options_audio_menu  |
| gameplay    | options_gameplay_menu |

If your mod uses different menu names, adjust the `cmd` strings in `handleOpenTab` accordingly.

## Testing

1. In console: `open credits` → should show credits screen
2. In console: `open audio` → should show audio settings
3. In console: `open gameplay` → should show gameplay settings
4. From main menu, Options → Audio tab should still work as before

## Notes

- `handleOpenTab` runs every time the main menu opens; it returns immediately if `ui_open_tab` is empty
- The cvar is cleared after navigation so subsequent main menu opens behave normally
- If `idtech3.exec` is restricted by `js_allowExec`, ensure it allows the required commands
