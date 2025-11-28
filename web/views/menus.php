<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Customizing id Tech 3 Menus</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            line-height: 1.6;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
        }
        .code-block {
            background: #f4f4f4;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
        .note {
            background: #fff3cd;
            padding: 10px;
            border-left: 4px solid #ffc107;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <h1>Customizing id Tech 3 Menus</h1>
    
    <h2>1. Menu Structure Overview</h2>
    <p>The menu system in id Tech 3 consists of several key components:</p>
    <ul>
        <li>Menu framework (menuframework_s)</li>
        <li>Menu items (menucommon_s)</li>
        <li>Menu drawing functions</li>
        <li>Menu event handling</li>
    </ul>

    <h2>2. Basic Menu Item Structure</h2>
    <div class="code-block">
        <pre>
typedef struct {
    int type;           // Item type (MTYPE_*)
    const char *name;   // Item name
    int id;            // Item identifier
    int x, y;          // Position
    int width, height; // Dimensions
    int style;         // UI style flags
    const char *text;  // Display text
    void (*callback)(void); // Click handler
} menucommon_s;</pre>
    </div>

    <h2>3. Creating a Custom Menu</h2>
    <div class="code-block">
        <pre>
typedef struct {
    menuframework_s menu;
    menubitmap_s background;
    menutext_s title;
    menubitmap_s logo;
    menubitmap_s mainMenu;
    menubitmap_s optionsMenu;
    menubitmap_s quitButton;
} mainmenu_t;

static mainmenu_t s_main;

void MainMenu_Init(void) {
    memset(&s_main, 0, sizeof(mainmenu_t));
    
    s_main.menu.wrapAround = qtrue;
    s_main.menu.fullscreen = qtrue;
    
    // Add background
    s_main.background.generic.type = MTYPE_BITMAP;
    s_main.background.generic.name = "menu/art/background";
    s_main.background.generic.flags = QMF_INACTIVE;
    s_main.background.generic.x = 0;
    s_main.background.generic.y = 0;
    s_main.background.width = 640;
    s_main.background.height = 480;
    Menu_AddItem(&s_main.menu, &s_main.background);
    
    // Add title
    s_main.title.generic.type = MTYPE_TEXT;
    s_main.title.generic.flags = QMF_CENTER_JUSTIFY;
    s_main.title.generic.x = 320;
    s_main.title.generic.y = 40;
    s_main.title.string = "GAME TITLE";
    s_main.title.style = UI_CENTER|UI_BIGFONT;
    s_main.title.color = color_main;
    Menu_AddItem(&s_main.menu, &s_main.title);
}</pre>
    </div>

    <h2>4. Customizing Menu Text</h2>
    <p>To modify menu text, you can either:</p>
    <ol>
        <li>Edit the string literals in the code</li>
        <li>Use a string table system</li>
        <li>Load from external files</li>
    </ol>

    <div class="code-block">
        <pre>
// String table approach
typedef struct {
    const char *key;
    const char *value;
} menustring_t;

static menustring_t menuStrings[] = {
    {"MENU_TITLE", "GAME TITLE"},
    {"MENU_NEWGAME", "New Game"},
    {"MENU_LOADGAME", "Load Game"},
    {"MENU_OPTIONS", "Options"},
    {"MENU_QUIT", "Quit"},
    {NULL, NULL}
};

const char *Menu_GetString(const char *key) {
    int i;
    for (i = 0; menuStrings[i].key != NULL; i++) {
        if (!Q_stricmp(menuStrings[i].key, key)) {
            return menuStrings[i].value;
        }
    }
    return key;
}</pre>
    </div>

    <h2>5. Adding Menu Items</h2>
    <div class="code-block">
        <pre>
void MainMenu_AddItems(void) {
    // New Game button
    s_main.newGame.generic.type = MTYPE_BITMAP;
    s_main.newGame.generic.name = "menu/art/newgame";
    s_main.newGame.generic.flags = QMF_HIGHLIGHT_IF_FOCUS;
    s_main.newGame.generic.x = 320;
    s_main.newGame.generic.y = 200;
    s_main.newGame.generic.callback = MainMenu_NewGameEvent;
    s_main.newGame.width = 128;
    s_main.newGame.height = 32;
    Menu_AddItem(&s_main.menu, &s_main.newGame);
    
    // Options button
    s_main.options.generic.type = MTYPE_BITMAP;
    s_main.options.generic.name = "menu/art/options";
    s_main.options.generic.flags = QMF_HIGHLIGHT_IF_FOCUS;
    s_main.options.generic.x = 320;
    s_main.options.generic.y = 250;
    s_main.options.generic.callback = MainMenu_OptionsEvent;
    s_main.options.width = 128;
    s_main.options.height = 32;
    Menu_AddItem(&s_main.menu, &s_main.options);
}</pre>
    </div>

    <div class="note">
        <strong>Important:</strong> When modifying menu text, ensure you maintain proper string length limits and consider localization requirements.
    </div>

    <h2>6. Menu Event Handling</h2>
    <div class="code-block">
        <pre>
static void MainMenu_NewGameEvent(void *ptr, int event) {
    if (event != QM_ACTIVATED) {
        return;
    }
    
    UI_NewGameMenu();
}

static void MainMenu_OptionsEvent(void *ptr, int event) {
    if (event != QM_ACTIVATED) {
        return;
    }
    
    UI_OptionsMenu();
}</pre>
    </div>

    <h2>7. Files to Modify</h2>
    <ul>
        <li><code>ui/menudef.h</code> - Menu structure definitions</li>
        <li><code>ui/menu.c</code> - Core menu functionality</li>
        <li><code>ui/ui_main.c</code> - Main menu implementation</li>
        <li><code>ui/ui_shared.c</code> - Shared UI utilities</li>
        <li><code>ui/ui_style.c</code> - UI style definitions</li>
    </ul>

    <h2>8. Best Practices</h2>
    <ul>
        <li>Use consistent naming conventions for menu items</li>
        <li>Implement proper error handling for missing assets</li>
        <li>Consider screen resolution independence</li>
        <li>Maintain proper menu hierarchy</li>
        <li>Use appropriate UI styles for different menu elements</li>
        <li>Follow font guidelines from <a href="fonts">Font System Documentation</a></li>
    </ul>

    <div class="note">
        <strong>Remember:</strong> Always test menu changes across different resolutions and aspect ratios to ensure proper display. For font-related issues, refer to the <a href="fonts">Font System Documentation</a>.
    </div>
</body>
</html>
