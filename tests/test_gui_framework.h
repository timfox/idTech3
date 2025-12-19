/*
===============================================================================
GUI Testing Framework for id Tech 3
===============================================================================
*/

#ifndef __TEST_GUI_FRAMEWORK_H__
#define __TEST_GUI_FRAMEWORK_H__

#include "../src/qcommon/q_shared.h"

// GUI test context
typedef struct {
    qboolean initialized;
    int screen_width;
    int screen_height;
    int mouse_x;
    int mouse_y;
    qboolean mouse_left_button;
    qboolean mouse_right_button;
    int key_pressed;
    char text_input[256];
    int text_cursor;
} gui_test_context_t;

// Mock UI functions for testing
extern void UI_Init(void);
extern void UI_Shutdown(void);
extern void UI_Refresh(int time);
extern void UI_KeyEvent(int key, int down);
extern void UI_MouseEvent(int dx, int dy);
extern qboolean UI_ConsoleCommand(int realTime);

// GUI test utilities
void gui_test_init(gui_test_context_t *ctx, int width, int height);
void gui_test_shutdown(gui_test_context_t *ctx);
void gui_test_simulate_mouse_move(gui_test_context_t *ctx, int x, int y);
void gui_test_simulate_mouse_click(gui_test_context_t *ctx, int button, qboolean down);
void gui_test_simulate_key_press(gui_test_context_t *ctx, int key);
void gui_test_simulate_text_input(gui_test_context_t *ctx, const char *text);
void gui_test_update(gui_test_context_t *ctx, int time_delta);

// GUI test assertions
#define ASSERT_GUI_VISIBLE(element) \
    do { \
        test_count++; \
        if (!(element)) { \
            Com_Printf("FAIL: %s:%d: GUI element not visible\n", \
                __func__, __LINE__); \
            test_failed++; \
            return; \
        } \
        test_passed++; \
    } while(0)

#define ASSERT_GUI_FOCUSED(element) \
    do { \
        test_count++; \
        if (!(element)) { \
            Com_Printf("FAIL: %s:%d: GUI element not focused\n", \
                __func__, __LINE__); \
            test_failed++; \
            return; \
        } \
        test_passed++; \
    } while(0)

#define ASSERT_GUI_POSITION(element, expected_x, expected_y) \
    do { \
        test_count++; \
        if (!((element)->x == (expected_x) && (element)->y == (expected_y))) { \
            Com_Printf("FAIL: %s:%d: GUI element position mismatch\n", \
                __func__, __LINE__); \
            test_failed++; \
            return; \
        } \
        test_passed++; \
    } while(0)

#define ASSERT_GUI_TEXT(element, expected_text) \
    do { \
        test_count++; \
        if (strcmp((element)->text, (expected_text)) != 0) { \
            Com_Printf("FAIL: %s:%d: GUI element text mismatch\n", \
                __func__, __LINE__); \
            test_failed++; \
            return; \
        } \
        test_passed++; \
    } while(0)

// Mock menu system structures (simplified for testing)
typedef struct {
    int x, y;
    int width, height;
    char text[64];
    qboolean visible;
    qboolean focused;
    void (*callback)(void *self, int event);
    void *callback_data;
} mock_menu_item_t;

typedef struct {
    char title[64];
    mock_menu_item_t items[16];
    int num_items;
    int cursor;
    qboolean fullscreen;
    qboolean showlogo;
} mock_menu_framework_t;

// GUI test helper functions
mock_menu_framework_t* gui_test_create_menu(const char *title);
void gui_test_destroy_menu(mock_menu_framework_t *menu);
mock_menu_item_t* gui_test_add_menu_item(mock_menu_framework_t *menu,
                                        int x, int y, int width, int height,
                                        const char *text,
                                        void (*callback)(void *self, int event));
void gui_test_simulate_menu_interaction(gui_test_context_t *ctx,
                                       mock_menu_framework_t *menu,
                                       int mouse_x, int mouse_y,
                                       qboolean click);

#endif // __TEST_GUI_FRAMEWORK_H__
