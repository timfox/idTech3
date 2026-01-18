/*
===========================================================================
UI2 - Layout Engine
Computes positions and sizes for nodes using block/flex/absolute layout
===========================================================================
*/

#include "ui2_internal.h"
#include "../common/qcommon.h"
#include <algorithm>
#include <climits>

#ifdef __cplusplus

// Apply styles from stylesheet to node
static void ApplyStyles(ui2Context_t *ctx, UiNode *node) {
	if (!ctx || !node) return;
	
	// Try to match by class name first, then tag name
	const Style *matchedStyle = nullptr;
	
	if (node->classNameId > 0) {
		matchedStyle = ctx->styleSheet.findRule(node->classNameId);
	}
	
	if (!matchedStyle && node->tagId > 0) {
		matchedStyle = ctx->styleSheet.findRule(node->tagId);
	}
	
	if (matchedStyle) {
		// Merge matched style into node style
		node->style = *matchedStyle;
	}
}

// Compute content box from layout box and padding
static constexpr void ComputeContentBox(LayoutBox &box, const ComputedStyle &computed) noexcept {
	box.contentX = box.x + computed.padding[3];  // left padding
	box.contentY = box.y + computed.padding[0];  // top padding
	box.contentWidth = box.width - computed.padding[1] - computed.padding[3];  // right + left
	box.contentHeight = box.height - computed.padding[0] - computed.padding[2];  // top + bottom
}

// Compute layout for a single node (block layout)
static void LayoutBlock(ui2Context_t *ctx, int32_t nodeIdx, int32_t parentX, int32_t parentY, 
                        int32_t parentWidth, int32_t parentHeight) {
	if (nodeIdx < 0 || nodeIdx >= (int32_t)ctx->nodeCount) {
		return;
	}
	
	UiNode *node = &ctx->nodes[nodeIdx];
	
	// Skip if display: none
	if (node->style.display == DisplayType::None) {
		return;
	}
	
	// Compute style values
	ComputedStyle &computed = node->computed;
	
	// Width - handle px, percent, and auto
	if (node->style.width.isAuto()) {
		computed.width = parentWidth;
	} else if (node->style.width.isPercent()) {
		computed.width = (parentWidth * node->style.width.value) / 100;
	} else {
		computed.width = node->style.width.value;
		if (computed.width > parentWidth) {
			computed.width = parentWidth;
		}
	}
	if (computed.width < node->style.minWidth) {
		computed.width = node->style.minWidth;
	}
	
	// Height - handle px, percent, and auto
	if (node->style.height.isAuto()) {
		computed.height = 0;  // Will be computed based on content
	} else if (node->style.height.isPercent()) {
		computed.height = (parentHeight * node->style.height.value) / 100;
	} else {
		computed.height = node->style.height.value;
	}
	if (computed.height < node->style.minHeight) {
		computed.height = node->style.minHeight;
	}
	
	// Padding and margin
	std::ranges::copy(node->style.padding, computed.padding.begin());
	std::ranges::copy(node->style.margin, computed.margin.begin());
	
	// Colors
	computed.backgroundColor = node->style.backgroundColor;
	computed.color = node->style.color;
	
	// Position (for relative, offset from parent)
	if (node->style.position == PositionType::Absolute) {
		// Absolute positioning relative to parent
		computed.left = node->style.left == UI2_AUTO ? 0 : node->style.left;
		computed.top = node->style.top == UI2_AUTO ? 0 : node->style.top;
		computed.right = node->style.right == UI2_AUTO ? UI2_AUTO : node->style.right;
		computed.bottom = node->style.bottom == UI2_AUTO ? UI2_AUTO : node->style.bottom;
	} else {
		computed.left = computed.top = 0;
		computed.right = computed.bottom = UI2_AUTO;
	}
	
	// Layout box
	LayoutBox &box = node->layout;
	
	if (node->style.position == PositionType::Absolute) {
		box.x = parentX + computed.left;
		box.y = parentY + computed.top;
		if (computed.right != UI2_AUTO) {
			box.width = (parentX + parentWidth - computed.right) - box.x;
		} else {
			box.width = computed.width;
		}
		if (computed.bottom != UI2_AUTO) {
			box.height = (parentY + parentHeight - computed.bottom) - box.y;
		} else {
			box.height = computed.height;
		}
	} else {
		// Relative positioning: margin affects position
		box.x = parentX + computed.margin[3];  // left margin
		box.y = parentY + computed.margin[0];  // top margin
		box.width = computed.width;
		box.height = computed.height;
	}
	
	// Compute content box
	ComputeContentBox(box, computed);
	
	// Clip rect (for overflow: clip)
	if (node->style.overflow == OverflowType::Clip) {
		box.clipX = box.contentX;
		box.clipY = box.contentY;
		box.clipWidth = box.contentWidth;
		box.clipHeight = box.contentHeight;
	} else {
		// No clipping
		box.clipX = box.clipY = 0;
		box.clipWidth = box.clipHeight = INT_MAX;
	}
	
	// Layout children
	if (node->firstChild >= 0 && node->style.display != DisplayType::None && !node->isText) {
		if (node->style.display == DisplayType::Flex) {
			// Flex layout
			if (node->style.flexDirection == FlexDirection::Row) {
				// Row layout: children arranged horizontally
				int32_t x = box.contentX;
				int32_t y = box.contentY;
				int32_t availableWidth = box.contentWidth;
				int32_t availableHeight = box.contentHeight;
				
				// First pass: compute child sizes and flex properties
				int32_t totalFlexGrow = 0;
				int32_t fixedSizeTotal = 0;
				struct FlexItem {
					int32_t nodeIdx;
					int32_t baseSize;
					int32_t flexGrow;
					int32_t flexShrink;
				};
				struct FlexItem flexItems[64];  // Max 64 flex items
				int32_t flexItemCount = 0;
				
				int32_t childIdx = node->firstChild;
				while (childIdx >= 0 && flexItemCount < 64) {
					UiNode *child = &ctx->nodes[childIdx];
					ApplyStyles(ctx, child);
					
					if (child->style.display != DisplayType::None) {
						FlexItem item;
						item.nodeIdx = childIdx;
						item.flexGrow = child->style.flexGrow;
						item.flexShrink = child->style.flexShrink;
						
						// Compute base size from flex-basis or width
						if (!child->style.flexBasis.isAuto()) {
							if (child->style.flexBasis.isPercent()) {
								item.baseSize = (availableWidth * child->style.flexBasis.value) / 100;
							} else {
								item.baseSize = child->style.flexBasis.value;
							}
						} else if (child->style.width.isAuto()) {
							item.baseSize = 0;  // Will grow
							totalFlexGrow += item.flexGrow;
							flexItems[flexItemCount++] = item;
							childIdx = child->nextSibling;
							continue;
						} else if (child->style.width.isPercent()) {
							item.baseSize = (availableWidth * child->style.width.value) / 100;
						} else {
							item.baseSize = child->style.width.value;
						}
						
						fixedSizeTotal += item.baseSize + child->style.margin[1] + child->style.margin[3];
						if (item.flexGrow > 0) {
							totalFlexGrow += item.flexGrow;
							flexItems[flexItemCount++] = item;
						}
					}
					childIdx = child->nextSibling;
				}
				
				// Distribute remaining space based on flex-grow
				int32_t remainingWidth = availableWidth - fixedSizeTotal;
				if (remainingWidth < 0) remainingWidth = 0;
				
				// Distribute space proportionally to flex-grow
				for (int32_t i = 0; i < flexItemCount; ++i) {
					if (totalFlexGrow > 0) {
						int32_t extra = (remainingWidth * flexItems[i].flexGrow) / totalFlexGrow;
						flexItems[i].baseSize += extra;
					}
				}
				
				// Second pass: layout children
				childIdx = node->firstChild;
				int32_t currentX = x;
				int32_t flexItemIdx = 0;
				while (childIdx >= 0) {
					UiNode *child = &ctx->nodes[childIdx];
					if (child->style.display != DisplayType::None) {
						// Find flex item or use fixed size
						int32_t childWidth = 0;
						if (flexItemIdx < flexItemCount && flexItems[flexItemIdx].nodeIdx == childIdx) {
							childWidth = flexItems[flexItemIdx].baseSize;
							flexItemIdx++;
						} else if (child->style.width.isAuto()) {
							childWidth = 0;  // Will be computed
						} else if (child->style.width.isPercent()) {
							childWidth = (availableWidth * child->style.width.value) / 100;
						} else {
							childWidth = child->style.width.value;
						}
						
						int32_t childHeight = availableHeight;  // Stretch by default
						
						// Apply align-items
						int32_t childY = y;
						if (!child->style.height.isAuto()) {
							if (child->style.height.isPercent()) {
								childHeight = (availableHeight * child->style.height.value) / 100;
							} else {
								childHeight = child->style.height.value;
							}
							if (node->style.alignItems == AlignItems::Center) {
								childY = y + (availableHeight - childHeight) / 2;
							} else if (node->style.alignItems == AlignItems::End) {
								childY = y + availableHeight - childHeight;
							}
						}
						
						LayoutBlock(ctx, childIdx, currentX + child->style.margin[3], 
						           childY + child->style.margin[0], childWidth, childHeight);
						
						currentX += childWidth + child->style.margin[1] + child->style.margin[3];
					}
					childIdx = child->nextSibling;
				}
			} else {
				// Column layout: children arranged vertically
				int32_t x = box.contentX;
				int32_t y = box.contentY;
				int32_t availableWidth = box.contentWidth;
				int32_t availableHeight = box.contentHeight;
				
				// Similar flex logic for column, but simpler
				int32_t childIdx = node->firstChild;
				int32_t currentY = y;
				while (childIdx >= 0) {
					UiNode *child = &ctx->nodes[childIdx];
					ApplyStyles(ctx, child);
					
					if (child->style.display != DisplayType::None) {
						int32_t childWidth = availableWidth;
						int32_t childHeight = 0;
						
						if (child->style.height.isAuto()) {
							childHeight = 0;  // Will be computed
						} else if (child->style.height.isPercent()) {
							childHeight = (availableHeight * child->style.height.value) / 100;
						} else {
							childHeight = child->style.height.value;
						}
						
						LayoutBlock(ctx, childIdx, x + child->style.margin[3], 
						           currentY + child->style.margin[0], childWidth, childHeight);
						
						currentY += child->layout.height + child->style.margin[0] + child->style.margin[2];
					}
					childIdx = child->nextSibling;
				}
			}
		} else {
			// Block layout: children stacked vertically
			int32_t 				childIdx = node->firstChild;
				int32_t currentY = box.contentY;
				
				while (childIdx >= 0) {
					UiNode *child = &ctx->nodes[childIdx];
					ApplyStyles(ctx, child);
					
					if (child->style.display != DisplayType::None) {
					int32_t childWidth = box.contentWidth;
					int32_t childHeight = 0;
					
					if (child->style.height.isAuto()) {
						childHeight = 0;  // Will be computed
					} else if (child->style.height.isPercent()) {
						childHeight = (box.contentHeight * child->style.height.value) / 100;
					} else {
						childHeight = child->style.height.value;
					}
					
					LayoutBlock(ctx, childIdx, box.contentX + child->style.margin[3], 
					           currentY + child->style.margin[0], childWidth, childHeight);
					
					// Update height if auto
					if (node->style.height.isAuto()) {
						box.height = std::max(box.height, 
						                      (int32_t)(child->layout.y + child->layout.height - box.y + child->style.margin[2]));
					}
					
					currentY = child->layout.y + child->layout.height + child->style.margin[2];
				}
				childIdx = child->nextSibling;
			}
			
			// Update content box after children
			ComputeContentBox(box, computed);
		}
	}
}

extern "C" {

// Compute layout for entire tree
void ComputeLayout(ui2Context_t *ctx) {
	if (!ctx || ctx->rootNode < 0) {
		return;
	}
	
	// Apply styles to all nodes
	for (uint32_t i = 0; i < ctx->nodeCount; ++i) {
		ApplyStyles(ctx, &ctx->nodes[i]);
	}
	
	// Layout root node (full screen)
	LayoutBlock(ctx, ctx->rootNode, 0, 0, ctx->screenWidth, ctx->screenHeight);
}

} // extern "C"

#endif // __cplusplus
