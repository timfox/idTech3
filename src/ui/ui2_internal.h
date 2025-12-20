/*
===========================================================================
UI2 - Internal C++ Types and Structures
===========================================================================
*/

#ifndef __UI2_INTERNAL_H__
#define __UI2_INTERNAL_H__

#include "ui2_public.h"
#include "../common/q_shared.h"
#include <cstdint>
#include <cstring>

#ifdef __cplusplus

// Forward declarations
struct UiNode;
struct Style;
struct ComputedStyle;
struct LayoutBox;

// Display type
enum class DisplayType : uint8_t {
	None = 0,
	Block = 1,
	Flex = 2
};

// Flex direction
enum class FlexDirection : uint8_t {
	Row = 0,
	Column = 1
};

// Justify content
enum class JustifyContent : uint8_t {
	Start = 0,
	Center = 1,
	End = 2,
	SpaceBetween = 3
};

// Align items
enum class AlignItems : uint8_t {
	Start = 0,
	Center = 1,
	End = 2,
	Stretch = 3
};

// Position type
enum class PositionType : uint8_t {
	Relative = 0,
	Absolute = 1
};

// Overflow type
enum class OverflowType : uint8_t {
	Visible = 0,
	Clip = 1
};

// Special value for auto sizing
constexpr int32_t UI2_AUTO = -1;

// RGBA color (0-255 per channel)
struct Color {
	uint8_t r, g, b, a;
	
	Color() : r(255), g(255), b(255), a(255) {}
	Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_) 
		: r(r_), g(g_), b(b_), a(a_) {}
	
	static Color fromRGBA(uint32_t rgba) {
		return Color(
			(uint8_t)((rgba >> 24) & 0xFF),
			(uint8_t)((rgba >> 16) & 0xFF),
			(uint8_t)((rgba >> 8) & 0xFF),
			(uint8_t)(rgba & 0xFF)
		);
	}
	
	uint32_t toRGBA() const {
		return ((uint32_t)r << 24) | ((uint32_t)g << 16) | 
		       ((uint32_t)b << 8) | (uint32_t)a;
	}
};

// Unit type for sizing
enum class SizeUnit : uint8_t {
	Px = 0,      // pixels
	Percent = 1, // percentage of parent
	Auto = 2     // auto (UI2_AUTO)
};

// Size value with unit
struct SizeValue {
	int32_t value = UI2_AUTO;
	SizeUnit unit = SizeUnit::Auto;
	
	SizeValue() = default;
	SizeValue(int32_t v, SizeUnit u) : value(v), unit(u) {}
	
	bool isAuto() const { return unit == SizeUnit::Auto || value == UI2_AUTO; }
	bool isPercent() const { return unit == SizeUnit::Percent; }
	bool isPx() const { return unit == SizeUnit::Px; }
};

// Specified style values (from CSS)
struct Style {
	DisplayType display = DisplayType::Block;
	FlexDirection flexDirection = FlexDirection::Row;
	JustifyContent justifyContent = JustifyContent::Start;
	AlignItems alignItems = AlignItems::Stretch;
	
	// Flex properties
	int32_t flexGrow = 0;      // 0 = don't grow
	int32_t flexShrink = 1;    // 1 = can shrink
	SizeValue flexBasis;       // default size before growing/shrinking
	
	SizeValue width;
	SizeValue height;
	int32_t minWidth = 0;
	int32_t minHeight = 0;
	
	int32_t padding[4] = {0, 0, 0, 0};  // top, right, bottom, left
	int32_t margin[4] = {0, 0, 0, 0};   // top, right, bottom, left
	
	Color backgroundColor = Color(0, 0, 0, 0);  // transparent
	Color color = Color(255, 255, 255, 255);    // white
	
	int32_t borderWidth = 0;
	Color borderColor = Color(0, 0, 0, 0);
	int32_t borderRadius[4] = {0, 0, 0, 0};  // top-left, top-right, bottom-right, bottom-left
	
	PositionType position = PositionType::Relative;
	int32_t left = UI2_AUTO;
	int32_t top = UI2_AUTO;
	int32_t right = UI2_AUTO;
	int32_t bottom = UI2_AUTO;
	
	OverflowType overflow = OverflowType::Visible;
	
	const char *font = "default";
	float fontSize = 16.0f;  // Default font size
};

// Computed style (resolved values)
struct ComputedStyle {
	int32_t width = 0;
	int32_t height = 0;
	int32_t padding[4] = {0, 0, 0, 0};
	int32_t margin[4] = {0, 0, 0, 0};
	Color backgroundColor;
	Color color;
	int32_t borderWidth = 0;
	Color borderColor;
	int32_t left = 0;
	int32_t top = 0;
	int32_t right = 0;
	int32_t bottom = 0;
};

// Layout box (computed position and size)
struct LayoutBox {
	int32_t x = 0;
	int32_t y = 0;
	int32_t width = 0;
	int32_t height = 0;
	
	// Content box (inside padding)
	int32_t contentX = 0;
	int32_t contentY = 0;
	int32_t contentWidth = 0;
	int32_t contentHeight = 0;
	
	// Clip rect (for overflow: clip)
	int32_t clipX = 0;
	int32_t clipY = 0;
	int32_t clipWidth = 0;
	int32_t clipHeight = 0;
};

// UI Node (tree structure)
struct UiNode {
	// Tree structure
	int32_t parent = -1;
	int32_t firstChild = -1;
	int32_t nextSibling = -1;
	int32_t childCount = 0;
	
	// Tag and class name (string IDs from string table)
	uint32_t tagId = 0;
	uint32_t classNameId = 0;
	
	// Text content (if any)
	const char *text = nullptr;
	
	// Style
	Style style;
	ComputedStyle computed;
	LayoutBox layout;
	
	// Node type
	bool isText = false;
};

// String table (for tag/class names)
struct StringTable {
	static constexpr size_t MAX_STRINGS = 1024;
	static constexpr size_t MAX_STRING_LEN = 64;
	
	char strings[MAX_STRINGS][MAX_STRING_LEN];
	uint32_t count = 0;
	
	uint32_t intern(const char *str) {
		if (!str || !*str) return 0;
		
		// Check if already exists
		for (uint32_t i = 1; i < count; ++i) {
			if (std::strcmp(strings[i], str) == 0) {
				return i;
			}
		}
		
		// Add new string
		if (count >= MAX_STRINGS - 1) {
			return 0;  // Table full
		}
		
		size_t len = std::strlen(str);
		if (len >= MAX_STRING_LEN) {
			len = MAX_STRING_LEN - 1;
		}
		
		std::memcpy(strings[count], str, len);
		strings[count][len] = '\0';
		return count++;
	}
	
	const char *get(uint32_t id) const {
		if (id >= count) return nullptr;
		return strings[id];
	}
	
	void reset() {
		count = 1;  // Keep ID 0 as empty string
		strings[0][0] = '\0';
	}
};

// Style rule (selector -> style)
struct StyleRule {
	uint32_t selectorId;  // Class name or tag name
	Style style;
};

// Style sheet (collection of rules)
struct StyleSheet {
	static constexpr size_t MAX_RULES = 256;
	StyleRule rules[MAX_RULES];
	uint32_t ruleCount = 0;
	
	void addRule(uint32_t selectorId, const Style &style) {
		if (ruleCount >= MAX_RULES) return;
		rules[ruleCount].selectorId = selectorId;
		rules[ruleCount].style = style;
		ruleCount++;
	}
	
	const Style *findRule(uint32_t selectorId) const {
		for (uint32_t i = 0; i < ruleCount; ++i) {
			if (rules[i].selectorId == selectorId) {
				return &rules[i].style;
			}
		}
		return nullptr;
	}
	
	void reset() {
		ruleCount = 0;
	}
};

// UI Context (main state)
struct ui2Context_s {
	// Renderer callbacks
	ui2Renderer_t renderer;
	
	// Screen dimensions
	int screenWidth = 0;
	int screenHeight = 0;
	
	// Node pool
	static constexpr size_t MAX_NODES = 1024;
	UiNode nodes[MAX_NODES];
	uint32_t nodeCount = 0;
	int32_t rootNode = -1;
	int32_t currentNode = -1;  // For building tree
	
	// String table
	StringTable stringTable;
	
	// Style sheet
	StyleSheet styleSheet;
	
	// Arena allocator (for temporary allocations)
	void *arenaBase = nullptr;
	size_t arenaSize = 0;
	size_t arenaUsed = 0;
	
	// Default font (cached)
	fontInfo_t *defaultFont = nullptr;
	
	// Frame state
	bool inFrame = false;
};

#endif // __cplusplus

#endif // __UI2_INTERNAL_H__
