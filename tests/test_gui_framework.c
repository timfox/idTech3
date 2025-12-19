/*
===============================================================================
GUI Testing Framework Implementation
===============================================================================
*/

#include "test_gui_framework.h"
#include <stdlib.h>
#include <string.h>

// Global GUI test context
static gui_test_context_t *current_test_ctx = NULL;

// Mock UI functions
void UI_Init(void) {
    // Mock initialization
}

void UI_Shutdown(void) {
    // Mock shutdown
}

void UI_Refresh(int time) {
    (void)time; // Mock refresh
}

void UI_KeyEvent(int key, int down) {
    if (current_test_ctx && down) {
        current_test_ctx->key_pressed = key;
    }
}

void UI_MouseEvent(int dx, int dy) {
    if (current_test_ctx) {
        current_test_ctx->mouse_x += dx;
        current_test_ctx->mouse_y += dy;

        // Clamp to screen bounds
        if (current_test_ctx->mouse_x < 0) current_test_ctx->mouse_x = 0;
        if (current_test_ctx->mouse_y < 0) current_test_ctx->mouse_y = 0;
        if (current_test_ctx->mouse_x >= current_test_ctx->screen_width)
            current_test_ctx->mouse_x = current_test_ctx->screen_width - 1;
        if (current_test_ctx->mouse_y >= current_test_ctx->screen_height)
            current_test_ctx->mouse_y = current_test_ctx->screen_height - 1;
    }
}

qboolean UI_ConsoleCommand(int realTime) {
    (void)realTime;
    return qfalse; // Mock implementation
}

// GUI test utilities implementation
void gui_test_init(gui_test_context_t *ctx, int width, int height) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->initialized = qtrue;
    ctx->screen_width = width;
    ctx->screen_height = height;
    ctx->mouse_x = width / 2;
    ctx->mouse_y = height / 2;
    current_test_ctx = ctx;
}

void gui_test_shutdown(gui_test_context_t *ctx) {
    ctx->initialized = qfalse;
    current_test_ctx = NULL;
}

void gui_test_simulate_mouse_move(gui_test_context_t *ctx, int x, int y) {
    if (!ctx->initialized) return;

    ctx->mouse_x = x;
    ctx->mouse_y = y;

    // Clamp to screen bounds
    if (ctx->mouse_x < 0) ctx->mouse_x = 0;
    if (ctx->mouse_y < 0) ctx->mouse_y = 0;
    if (ctx->mouse_x >= ctx->screen_width) ctx->mouse_x = ctx->screen_width - 1;
    if (ctx->mouse_y >= ctx->screen_height) ctx->mouse_y = ctx->screen_height - 1;

    UI_MouseEvent(0, 0); // Trigger mouse event processing
}

void gui_test_simulate_mouse_click(gui_test_context_t *ctx, int button, qboolean down) {
    if (!ctx->initialized) return;

    if (button == 0) { // Left button
        ctx->mouse_left_button = down;
    } else if (button == 1) { // Right button
        ctx->mouse_right_button = down;
    }
}

void gui_test_simulate_key_press(gui_test_context_t *ctx, int key) {
    if (!ctx->initialized) return;

    UI_KeyEvent(key, 1); // Key down
    UI_KeyEvent(key, 0); // Key up
}

void gui_test_simulate_text_input(gui_test_context_t *ctx, const char *text) {
    if (!ctx->initialized || !text) return;

    size_t len = strlen(text);
    size_t available = sizeof(ctx->text_input) - ctx->text_cursor - 1;

    if (len > available) len = available;

    memcpy(ctx->text_input + ctx->text_cursor, text, len);
    ctx->text_cursor += len;
    ctx->text_input[ctx->text_cursor] = '\0';
}

void gui_test_update(gui_test_context_t *ctx, int time_delta) {
    if (!ctx->initialized) return;

    UI_Refresh(time_delta);
}

// Mock menu system implementation
mock_menu_framework_t* gui_test_create_menu(const char *title) {
    mock_menu_framework_t *menu = malloc(sizeof(mock_menu_framework_t));
    if (!menu) return NULL;

    memset(menu, 0, sizeof(*menu));
    Q_strncpyz(menu->title, title, sizeof(menu->title));
    menu->cursor = -1;
    menu->fullscreen = qfalse;
    menu->showlogo = qfalse;

    return menu;
}

void gui_test_destroy_menu(mock_menu_framework_t *menu) {
    if (menu) {
        free(menu);
    }
}

mock_menu_item_t* gui_test_add_menu_item(mock_menu_framework_t *menu,
                                        int x, int y, int width, int height,
                                        const char *text,
                                        void (*callback)(void *self, int event)) {
    if (!menu || menu->num_items >= 16) return NULL;

    mock_menu_item_t *item = &menu->items[menu->num_items++];
    item->x = x;
    item->y = y;
    item->width = width;
    item->height = height;
    Q_strncpyz(item->text, text, sizeof(item->text));
    item->visible = qtrue;
    item->focused = qfalse;
    item->callback = callback;
    item->callback_data = item;

    return item;
}

void gui_test_simulate_menu_interaction(gui_test_context_t *ctx,
                                       mock_menu_framework_t *menu,
                                       int mouse_x, int mouse_y,
                                       qboolean click) {
    if (!ctx->initialized || !menu) return;

    // Move mouse to position
    gui_test_simulate_mouse_move(ctx, mouse_x, mouse_y);

    // Find item under mouse
    for (int i = 0; i < menu->num_items; i++) {
        mock_menu_item_t *item = &menu->items[i];
        if (item->visible &&
            mouse_x >= item->x && mouse_x < item->x + item->width &&
            mouse_y >= item->y && mouse_y < item->y + item->height) {

            // Focus the item
            item->focused = qtrue;
            menu->cursor = i;

            // Click if requested
            if (click && item->callback) {
                gui_test_simulate_mouse_click(ctx, 0, qtrue);  // Mouse down
                item->callback(item->callback_data, 3); // QM_ACTIVATED
                gui_test_simulate_mouse_click(ctx, 0, qfalse); // Mouse up
            }
            break;
        }
    }
}
