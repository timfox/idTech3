/*
===============================================================================
Basic GUI Testing - Menu interaction and UI component testing
===============================================================================
*/

#include "test_framework.h"
#include "test_gui_framework.h"
#include <stdlib.h>

// Mock Com_Error for testing
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	exit(1);
}

// Minimal Com_Printf stub for the test framework
void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

// Test callback counters
static int button_click_count = 0;
static int last_clicked_item = -1;

static void test_button_callback(void *self, int event) {
    mock_menu_item_t *item = (mock_menu_item_t *)self;
    if (event == 3) { // QM_ACTIVATED
        button_click_count++;
        last_clicked_item = (int)(item - item); // Simplified indexing
    }
}

TEST(gui_context_initialization) {
    gui_test_context_t ctx;

    // Test initialization
    gui_test_init(&ctx, 800, 600);
    ASSERT_TRUE(ctx.initialized);
    ASSERT_EQ(ctx.screen_width, 800);
    ASSERT_EQ(ctx.screen_height, 600);
    ASSERT_EQ(ctx.mouse_x, 400); // Should center
    ASSERT_EQ(ctx.mouse_y, 300);

    // Test shutdown
    gui_test_shutdown(&ctx);
    ASSERT_FALSE(ctx.initialized);
}

TEST(gui_mouse_interaction) {
    gui_test_context_t ctx;
    gui_test_init(&ctx, 800, 600);

    // Test mouse movement
    gui_test_simulate_mouse_move(&ctx, 100, 200);
    ASSERT_EQ(ctx.mouse_x, 100);
    ASSERT_EQ(ctx.mouse_y, 200);

    // Test mouse movement clamping
    gui_test_simulate_mouse_move(&ctx, -50, 700);
    ASSERT_EQ(ctx.mouse_x, 0);   // Clamped to 0
    ASSERT_EQ(ctx.mouse_y, 599); // Clamped to screen height - 1

    // Test mouse clicks
    gui_test_simulate_mouse_click(&ctx, 0, qtrue);  // Left button down
    ASSERT_TRUE(ctx.mouse_left_button);

    gui_test_simulate_mouse_click(&ctx, 0, qfalse); // Left button up
    ASSERT_FALSE(ctx.mouse_left_button);

    gui_test_simulate_mouse_click(&ctx, 1, qtrue);  // Right button down
    ASSERT_TRUE(ctx.mouse_right_button);

    gui_test_shutdown(&ctx);
}

TEST(gui_menu_creation_and_interaction) {
    gui_test_context_t ctx;
    gui_test_init(&ctx, 800, 600);

    // Create a test menu
    mock_menu_framework_t *menu = gui_test_create_menu("Test Menu");
    ASSERT_NOT_NULL(menu);
    ASSERT_STR_EQ(menu->title, "Test Menu");
    ASSERT_EQ(menu->num_items, 0);
    ASSERT_EQ(menu->cursor, -1);

    // Add menu items
    mock_menu_item_t *button1 = gui_test_add_menu_item(menu, 100, 100, 200, 50,
                                                      "Button 1", test_button_callback);
    ASSERT_NOT_NULL(button1);
    ASSERT_STR_EQ(button1->text, "Button 1");
    ASSERT_EQ(button1->x, 100);
    ASSERT_EQ(button1->y, 100);
    ASSERT_EQ(button1->width, 200);
    ASSERT_EQ(button1->height, 50);
    ASSERT_TRUE(button1->visible);
    ASSERT_FALSE(button1->focused);

    mock_menu_item_t *button2 = gui_test_add_menu_item(menu, 100, 160, 200, 50,
                                                      "Button 2", test_button_callback);
    ASSERT_NOT_NULL(button2);
    ASSERT_EQ(menu->num_items, 2);

    // Reset click counters
    button_click_count = 0;
    last_clicked_item = -1;

    // Test mouse interaction with button 1
    gui_test_simulate_menu_interaction(&ctx, menu, 150, 125, qtrue); // Click on button 1
    ASSERT_EQ(button_click_count, 1);
    ASSERT_TRUE(button1->focused);
    ASSERT_FALSE(button2->focused);
    ASSERT_EQ(menu->cursor, 0);

    // Reset and test button 2
    button_click_count = 0;
    button1->focused = qfalse;
    menu->cursor = -1;

    gui_test_simulate_menu_interaction(&ctx, menu, 150, 185, qtrue); // Click on button 2
    ASSERT_EQ(button_click_count, 1);
    ASSERT_FALSE(button1->focused);
    ASSERT_TRUE(button2->focused);
    ASSERT_EQ(menu->cursor, 1);

    // Test clicking outside buttons
    button_click_count = 0;
    button2->focused = qfalse;
    menu->cursor = -1;

    gui_test_simulate_menu_interaction(&ctx, menu, 50, 50, qtrue); // Click outside
    ASSERT_EQ(button_click_count, 0);
    ASSERT_EQ(menu->cursor, -1);
    ASSERT_FALSE(button1->focused);
    ASSERT_FALSE(button2->focused);

    // Cleanup
    gui_test_destroy_menu(menu);
    gui_test_shutdown(&ctx);
}

TEST(gui_text_input_simulation) {
    gui_test_context_t ctx;
    gui_test_init(&ctx, 800, 600);

    // Test text input
    gui_test_simulate_text_input(&ctx, "hello");
    ASSERT_STR_EQ(ctx.text_input, "hello");
    ASSERT_EQ(ctx.text_cursor, 5);

    gui_test_simulate_text_input(&ctx, " world");
    ASSERT_STR_EQ(ctx.text_input, "hello world");
    ASSERT_EQ(ctx.text_cursor, 11);

    // Test buffer limits (simulate reaching limit)
    memset(ctx.text_input, 'a', sizeof(ctx.text_input) - 2);
    ctx.text_cursor = sizeof(ctx.text_input) - 2;
    ctx.text_input[ctx.text_cursor] = '\0';

    // Try to add more text (should be truncated)
    gui_test_simulate_text_input(&ctx, "moretext");
    // Should still be null-terminated and not overflow
    ASSERT_TRUE(strlen(ctx.text_input) < sizeof(ctx.text_input));

    gui_test_shutdown(&ctx);
}

TEST(gui_key_event_simulation) {
    gui_test_context_t ctx;
    gui_test_init(&ctx, 800, 600);

    // Test key press simulation
    gui_test_simulate_key_press(&ctx, 'a');
    ASSERT_EQ(ctx.key_pressed, 'a');

    gui_test_simulate_key_press(&ctx, 'A');
    ASSERT_EQ(ctx.key_pressed, 'A');

    gui_test_simulate_key_press(&ctx, 13); // Enter key
    ASSERT_EQ(ctx.key_pressed, 13);

    gui_test_shutdown(&ctx);
}

TEST(gui_menu_bounds_checking) {
    gui_test_context_t ctx;
    gui_test_init(&ctx, 800, 600);

    mock_menu_framework_t *menu = gui_test_create_menu("Bounds Test Menu");
    ASSERT_NOT_NULL(menu);

    // Add items at screen edges
    mock_menu_item_t *top_left = gui_test_add_menu_item(menu, 0, 0, 100, 50,
                                                       "Top Left", NULL);
    mock_menu_item_t *bottom_right = gui_test_add_menu_item(menu, 700, 550, 100, 50,
                                                           "Bottom Right", NULL);

    ASSERT_NOT_NULL(top_left);
    ASSERT_NOT_NULL(bottom_right);
    ASSERT_EQ(menu->num_items, 2);

    // Test interaction at boundaries
    gui_test_simulate_menu_interaction(&ctx, menu, 50, 25, qfalse); // Inside top-left
    ASSERT_TRUE(top_left->focused);
    ASSERT_EQ(menu->cursor, 0);

    top_left->focused = qfalse;
    menu->cursor = -1;

    gui_test_simulate_menu_interaction(&ctx, menu, 750, 575, qfalse); // Inside bottom-right
    ASSERT_TRUE(bottom_right->focused);
    ASSERT_EQ(menu->cursor, 1);

    // Test just outside boundaries
    bottom_right->focused = qfalse;
    menu->cursor = -1;

    gui_test_simulate_menu_interaction(&ctx, menu, 810, 575, qfalse); // Outside screen
    ASSERT_FALSE(bottom_right->focused);
    ASSERT_EQ(menu->cursor, -1);

    gui_test_destroy_menu(menu);
    gui_test_shutdown(&ctx);
}

int main(void) {
	Com_Printf("Running GUI tests...\n\n");

	RUN_TEST(gui_context_initialization);
	RUN_TEST(gui_mouse_interaction);
	RUN_TEST(gui_menu_creation_and_interaction);
	RUN_TEST(gui_text_input_simulation);
	RUN_TEST(gui_key_event_simulation);
	RUN_TEST(gui_menu_bounds_checking);

	PRINT_TEST_SUMMARY();

	Com_Printf("\nGUI tests completed.\n");
	Com_Printf("These tests validate basic GUI component interaction.\n");

	return (test_failed > 0) ? 1 : 0;
}
