/*
===========================================================================
UI2 - Renderer Bridge
Draws rectangles and text using Quake3e renderer
===========================================================================
*/

#include "ui2_internal.h"
#include "../common/q_shared.h"
#include <algorithm>

#ifdef __cplusplus

// Convert Color to float array [0.0-1.0]
static void ColorToFloat(const Color &c, float *rgba) {
	rgba[0] = c.r / 255.0f;
	rgba[1] = c.g / 255.0f;
	rgba[2] = c.b / 255.0f;
	rgba[3] = c.a / 255.0f;
}

// Draw solid rectangle
static void DrawRect(ui2Context_t *ctx, int32_t x, int32_t y, int32_t w, int32_t h, const Color &color) {
	if (!ctx || w <= 0 || h <= 0) {
		return;
	}
	
	// Set color
	float rgba[4];
	ColorToFloat(color, rgba);
	ctx->renderer.SetColor(rgba);
	
	// Draw stretched pic (1x1 white texture, stretched to rect size)
	// Use shader handle 0 (white solid)
	ctx->renderer.DrawStretchPic((float)x, (float)y, (float)w, (float)h, 
	                             0.0f, 0.0f, 1.0f, 1.0f, 0);
}

// Draw text using renderer's TextPaint callback
static void DrawText(ui2Context_t *ctx, int32_t x, int32_t y, const char *text, 
                    const Color &color, float fontSize, fontInfo_t *font) {
	if (!ctx || !text || !*text) {
		return;
	}
	
	// Convert color to float array
	float rgba[4];
	ColorToFloat(color, rgba);
	
	// Use renderer's TextPaint if available
	if (ctx->renderer.TextPaint && font) {
		ctx->renderer.TextPaint((float)x, (float)y, fontSize / 16.0f, rgba, text, font);
	} else {
		// Fallback: draw placeholder rectangles
		int32_t charWidth = (int32_t)(fontSize * 0.5f);  // Approximate width
		int32_t charHeight = (int32_t)fontSize;
		
		const char *p = text;
		int32_t currentX = x;
		while (*p && *p != '\0') {
			DrawRect(ctx, currentX, y, charWidth, charHeight, color);
			currentX += charWidth;
			p++;
		}
	}
}

// Draw rounded rectangle (simplified - draws as regular rect for now)
// TODO: Implement proper rounded corners using multiple quads or shader
static void DrawRoundedRect(ui2Context_t *ctx, int32_t x, int32_t y, int32_t w, int32_t h, 
                           const Color &color, const int32_t *borderRadius) {
	// For now, just draw as regular rectangle
	// In a full implementation, this would use a shader or multiple quads
	(void)borderRadius;  // Suppress unused parameter warning
	DrawRect(ctx, x, y, w, h, color);
}

// Render a single node
static void RenderNode(ui2Context_t *ctx, int32_t nodeIdx) {
	if (nodeIdx < 0 || nodeIdx >= (int32_t)ctx->nodeCount) {
		return;
	}
	
	UiNode *node = &ctx->nodes[nodeIdx];
	
	// Skip if display: none
	if (node->style.display == DisplayType::None) {
		return;
	}
	
	LayoutBox &box = node->layout;
	
	// Apply scissor/clip if overflow: clip
	if (node->style.overflow == OverflowType::Clip && 
	    box.clipWidth > 0 && box.clipHeight > 0) {
		if (ctx->renderer.Scissor) {
			ctx->renderer.Scissor(box.clipX, box.clipY, box.clipWidth, box.clipHeight);
		}
	}
	
	// Draw background (with border-radius if specified)
	if (node->computed.backgroundColor.a > 0) {
		bool hasRadius = false;
		for (int i = 0; i < 4; ++i) {
			if (node->style.borderRadius[i] > 0) {
				hasRadius = true;
				break;
			}
		}
		
		if (hasRadius) {
			DrawRoundedRect(ctx, box.x, box.y, box.width, box.height, 
			               node->computed.backgroundColor, node->style.borderRadius);
		} else {
			DrawRect(ctx, box.x, box.y, box.width, box.height, node->computed.backgroundColor);
		}
	}
	
	// Draw border (if borderWidth > 0)
	if (node->style.borderWidth > 0 && node->style.borderColor.a > 0) {
		// Top border
		DrawRect(ctx, box.x, box.y, box.width, node->style.borderWidth, node->style.borderColor);
		// Bottom border
		DrawRect(ctx, box.x, box.y + box.height - node->style.borderWidth, 
		        box.width, node->style.borderWidth, node->style.borderColor);
		// Left border
		DrawRect(ctx, box.x, box.y, node->style.borderWidth, box.height, node->style.borderColor);
		// Right border
		DrawRect(ctx, box.x + box.width - node->style.borderWidth, box.y, 
		        node->style.borderWidth, box.height, node->style.borderColor);
	}
	
	// Draw text if this is a text node
	if (node->isText && node->text) {
		// Get font (use default if available)
		fontInfo_t *font = ctx->defaultFont;
		if (!font && ctx->renderer.GetDefaultFont) {
			font = ctx->renderer.GetDefaultFont();
			ctx->defaultFont = font;  // Cache it
		}
		
		DrawText(ctx, box.contentX, box.contentY, node->text, node->computed.color, 
		        node->style.fontSize, font);
	}
	
	// Render children
	if (node->firstChild >= 0) {
		int32_t childIdx = node->firstChild;
		while (childIdx >= 0) {
			RenderNode(ctx, childIdx);
			childIdx = ctx->nodes[childIdx].nextSibling;
		}
	}
	
	// Reset scissor if we set it
	if (node->style.overflow == OverflowType::Clip && ctx->renderer.Scissor) {
		ctx->renderer.Scissor(0, 0, ctx->screenWidth, ctx->screenHeight);
	}
}

extern "C" {

// Render all nodes
void RenderNodes(ui2Context_t *ctx) {
	if (!ctx || ctx->rootNode < 0) {
		return;
	}
	
	// Render from root
	RenderNode(ctx, ctx->rootNode);
}

} // extern "C"

#endif // __cplusplus
