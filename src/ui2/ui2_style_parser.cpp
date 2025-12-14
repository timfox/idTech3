/*
===========================================================================
UI2 - CSS Style Parser
Tokenizes and parses CSS-like syntax into style rules
===========================================================================
*/

#include "ui2_internal.h"
#include "../qcommon/qcommon.h"
#include <cctype>
#include <cstdlib>
#include <string>

#ifdef __cplusplus

// Token types
enum class TokenType : uint8_t {
	EndOfFile = 0,
	Identifier,
	Number,
	Hash,        // #rrggbbaa
	String,
	Colon,       // :
	Semicolon,   // ;
	LeftBrace,   // {
	RightBrace,  // }
	Comma,       // ,
	Invalid
};

// Token structure
struct Token {
	TokenType type = TokenType::EndOfFile;
	const char *start = nullptr;
	size_t length = 0;
	int32_t intValue = 0;
	
	Token() = default;
	Token(TokenType t, const char *s, size_t len) 
		: type(t), start(s), length(len) {}
};

// Tokenizer state
struct Tokenizer {
	const char *input = nullptr;
	const char *current = nullptr;
	const char *end = nullptr;
	
	void init(const char *text) {
		input = text;
		current = text;
		if (text) {
			end = text + std::strlen(text);
		} else {
			end = text;
		}
	}
	
	char peek() const {
		if (current >= end) return '\0';
		return *current;
	}
	
	char advance() {
		if (current >= end) return '\0';
		return *current++;
	}
	
	void skipWhitespace() {
		while (current < end && std::isspace((unsigned char)*current)) {
			current++;
		}
	}
	
	bool isAtEnd() const {
		return current >= end;
	}
};

// Tokenize CSS text
static Token nextToken(Tokenizer &tok) {
	tok.skipWhitespace();
	
	if (tok.isAtEnd()) {
		return Token(TokenType::EndOfFile, nullptr, 0);
	}
	
	const char *start = tok.current;
	char c = tok.advance();
	
	switch (c) {
		case ':':
			return Token(TokenType::Colon, start, 1);
		case ';':
			return Token(TokenType::Semicolon, start, 1);
		case '{':
			return Token(TokenType::LeftBrace, start, 1);
		case '}':
			return Token(TokenType::RightBrace, start, 1);
		case ',':
			return Token(TokenType::Comma, start, 1);
		case '#': {
			// Hash color: #rrggbbaa
			const char *hashStart = start;
			size_t len = 1;
			while (tok.current < tok.end && 
			       (std::isxdigit((unsigned char)tok.peek()) || tok.peek() == 'x')) {
				tok.advance();
				len++;
			}
			Token t(TokenType::Hash, hashStart, len);
			return t;
		}
		case '"':
		case '\'': {
			// String literal
			char quote = c;
			const char *strStart = start + 1;
			while (tok.current < tok.end && tok.peek() != quote) {
				if (tok.peek() == '\\') {
					tok.advance();  // Skip escape
				}
				tok.advance();
			}
			size_t len = tok.current - strStart;
			if (tok.peek() == quote) {
				tok.advance();  // Skip closing quote
			}
			return Token(TokenType::String, strStart, len);
		}
		default:
			if (std::isdigit((unsigned char)c) || c == '-') {
				// Number
				bool negative = (c == '-');
				if (negative) {
					c = tok.advance();
					if (tok.isAtEnd() || !std::isdigit((unsigned char)c)) {
						// Might be identifier starting with '-'
						tok.current = start;
						c = tok.advance();
						goto identifier;
					}
				}
				
				int32_t value = c - '0';
				while (tok.current < tok.end && std::isdigit((unsigned char)tok.peek())) {
					value = value * 10 + (tok.advance() - '0');
				}
				
				Token t(TokenType::Number, start, tok.current - start);
				t.intValue = negative ? -value : value;
				return t;
			} else if (std::isalpha((unsigned char)c) || c == '_' || c == '-') {
				// Identifier
			identifier:
				while (tok.current < tok.end && 
				       (std::isalnum((unsigned char)tok.peek()) || 
				        tok.peek() == '_' || tok.peek() == '-')) {
					tok.advance();
				}
				return Token(TokenType::Identifier, start, tok.current - start);
			}
			return Token(TokenType::Invalid, start, 1);
	}
}

// Parse RGBA color from hash token (#rrggbbaa or #rrggbb)
static Color parseColor(const Token &token) {
	if (token.type != TokenType::Hash || token.length < 4) {
		return Color(255, 255, 255, 255);  // Default white
	}
	
	const char *str = token.start + 1;  // Skip '#'
	size_t len = token.length - 1;
	
	uint32_t rgba = 0;
	
	// Parse hex digits
	for (size_t i = 0; i < len && i < 8; ++i) {
		char c = str[i];
		uint32_t digit = 0;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = 10 + (c - 'a');
		} else if (c >= 'A' && c <= 'F') {
			digit = 10 + (c - 'A');
		}
		rgba = (rgba << 4) | digit;
	}
	
	// Handle #rrggbb (6 digits) -> add alpha 255
	if (len == 6) {
		rgba = (rgba << 8) | 0xFF;
	}
	
	// Extract components (RGBA order in hash)
	uint8_t r = (uint8_t)((rgba >> 24) & 0xFF);
	uint8_t g = (uint8_t)((rgba >> 16) & 0xFF);
	uint8_t b = (uint8_t)((rgba >> 8) & 0xFF);
	uint8_t a = (uint8_t)(rgba & 0xFF);
	
	return Color(r, g, b, a);
}

// Parse property value
static bool parsePropertyValue(Tokenizer &tok, Token &token, Style &style, const char *propName) {
	// Skip colon if present
	if (token.type == TokenType::Colon) {
		token = nextToken(tok);
	}
	
	// Parse based on property name
	if (std::strcmp(propName, "display") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "none") style.display = DisplayType::None;
			else if (val == "block") style.display = DisplayType::Block;
			else if (val == "flex") style.display = DisplayType::Flex;
		}
	} else if (std::strcmp(propName, "flex-direction") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "row") style.flexDirection = FlexDirection::Row;
			else if (val == "column") style.flexDirection = FlexDirection::Column;
		}
	} else if (std::strcmp(propName, "justify-content") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "start") style.justifyContent = JustifyContent::Start;
			else if (val == "center") style.justifyContent = JustifyContent::Center;
			else if (val == "end") style.justifyContent = JustifyContent::End;
			else if (val == "space-between") style.justifyContent = JustifyContent::SpaceBetween;
		}
	} else if (std::strcmp(propName, "align-items") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "start") style.alignItems = AlignItems::Start;
			else if (val == "center") style.alignItems = AlignItems::Center;
			else if (val == "end") style.alignItems = AlignItems::End;
			else if (val == "stretch") style.alignItems = AlignItems::Stretch;
		}
	} else if (std::strcmp(propName, "width") == 0 || std::strcmp(propName, "height") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "auto") {
				if (std::strcmp(propName, "width") == 0) {
					style.width.value = UI2_AUTO;
					style.width.unit = SizeUnit::Auto;
				} else {
					style.height.value = UI2_AUTO;
					style.height.unit = SizeUnit::Auto;
				}
			}
		} else if (token.type == TokenType::Number) {
			int32_t value = token.intValue;
			SizeUnit unit = SizeUnit::Px;
			
			// Check for unit
			Token next = nextToken(tok);
			if (next.type == TokenType::Identifier) {
				std::string unitStr(next.start, next.length);
				if (unitStr == "px") {
					unit = SizeUnit::Px;
				} else if (unitStr == "%") {
					unit = SizeUnit::Percent;
				} else {
					tok.current = next.start;  // Put it back
				}
			} else {
				tok.current = next.start;  // Put it back
			}
			
			if (std::strcmp(propName, "width") == 0) {
				style.width.value = value;
				style.width.unit = unit;
			} else {
				style.height.value = value;
				style.height.unit = unit;
			}
		}
	} else if (std::strcmp(propName, "min-width") == 0 || std::strcmp(propName, "min-height") == 0) {
		if (token.type == TokenType::Number) {
			int32_t value = token.intValue;
			Token next = nextToken(tok);
			if (next.type == TokenType::Identifier) {
				std::string unit(next.start, next.length);
				if (unit != "px") {
					tok.current = next.start;
				}
			} else {
				tok.current = next.start;
			}
			if (std::strcmp(propName, "min-width") == 0) style.minWidth = value;
			else style.minHeight = value;
		}
	} else if (std::strcmp(propName, "padding") == 0 || std::strcmp(propName, "margin") == 0) {
		int32_t *target = (std::strcmp(propName, "padding") == 0) ? style.padding : style.margin;
		
		if (token.type == TokenType::Number) {
			int32_t value = token.intValue;
			Token next = nextToken(tok);
			if (next.type == TokenType::Identifier) {
				std::string unit(next.start, next.length);
				if (unit != "px") {
					tok.current = next.start;
				}
			} else {
				tok.current = next.start;
			}
			
			// Check for shorthand (1, 2, or 4 values)
			next = nextToken(tok);
			if (next.type == TokenType::Number) {
				// 2 or 4 values
				int32_t value2 = next.intValue;
				Token next2 = nextToken(tok);
				if (next2.type == TokenType::Identifier && std::string(next2.start, next2.length) == "px") {
					next2 = nextToken(tok);
				}
				if (next2.type == TokenType::Number) {
					// 4 values: top right bottom left
					int32_t value3 = next2.intValue;
					Token next3 = nextToken(tok);
					if (next3.type == TokenType::Identifier && std::string(next3.start, next3.length) == "px") {
						next3 = nextToken(tok);
					}
					if (next3.type == TokenType::Number) {
						int32_t value4 = next3.intValue;
						target[0] = value;   // top
						target[1] = value2;   // right
						target[2] = value3;   // bottom
						target[3] = value4;   // left
						token = next3;
						return true;
					}
				}
				// 2 values: vertical horizontal
				target[0] = value;   // top
				target[1] = value2;  // right
				target[2] = value;    // bottom
				target[3] = value2;   // left
				token = next2;
				return true;
			}
			// 1 value: all sides
			target[0] = target[1] = target[2] = target[3] = value;
			token = next;
			return true;
		}
		return false;
	} else if (std::strcmp(propName, "background-color") == 0 || std::strcmp(propName, "color") == 0) {
		if (token.type == TokenType::Hash) {
			Color c = parseColor(token);
			if (std::strcmp(propName, "background-color") == 0) {
				style.backgroundColor = c;
			} else {
				style.color = c;
			}
		}
	} else if (std::strcmp(propName, "position") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "relative") style.position = PositionType::Relative;
			else if (val == "absolute") style.position = PositionType::Absolute;
		}
	} else if (std::strcmp(propName, "left") == 0 || std::strcmp(propName, "top") == 0 ||
	           std::strcmp(propName, "right") == 0 || std::strcmp(propName, "bottom") == 0) {
		if (token.type == TokenType::Number) {
			int32_t value = token.intValue;
			Token next = nextToken(tok);
			if (next.type == TokenType::Identifier && std::string(next.start, next.length) == "px") {
				next = nextToken(tok);
			} else {
				tok.current = next.start;
			}
			if (std::strcmp(propName, "left") == 0) style.left = value;
			else if (std::strcmp(propName, "top") == 0) style.top = value;
			else if (std::strcmp(propName, "right") == 0) style.right = value;
			else if (std::strcmp(propName, "bottom") == 0) style.bottom = value;
			token = next;
			return true;
		}
		return false;
	} else if (std::strcmp(propName, "overflow") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "visible") style.overflow = OverflowType::Visible;
			else if (val == "clip") style.overflow = OverflowType::Clip;
		}
	} else if (std::strcmp(propName, "font") == 0) {
		if (token.type == TokenType::String || token.type == TokenType::Identifier) {
			// Store font name (for now just "default")
			style.font = "default";
		}
	} else if (std::strcmp(propName, "font-size") == 0) {
		if (token.type == TokenType::Number) {
			float value = (float)token.intValue;
			Token next = nextToken(tok);
			if (next.type == TokenType::Identifier) {
				std::string unit(next.start, next.length);
				if (unit == "px") {
					style.fontSize = value;
				} else {
					tok.current = next.start;
				}
			} else {
				tok.current = next.start;
				style.fontSize = value;  // Default to px
			}
		}
	} else if (std::strcmp(propName, "flex-grow") == 0) {
		if (token.type == TokenType::Number) {
			style.flexGrow = token.intValue;
			if (style.flexGrow < 0) style.flexGrow = 0;
		}
	} else if (std::strcmp(propName, "flex-shrink") == 0) {
		if (token.type == TokenType::Number) {
			style.flexShrink = token.intValue;
			if (style.flexShrink < 0) style.flexShrink = 0;
		}
	} else if (std::strcmp(propName, "flex-basis") == 0) {
		if (token.type == TokenType::Identifier) {
			std::string val(token.start, token.length);
			if (val == "auto") {
				style.flexBasis.value = UI2_AUTO;
				style.flexBasis.unit = SizeUnit::Auto;
			}
		} else if (token.type == TokenType::Number) {
			int32_t value = token.intValue;
			SizeUnit unit = SizeUnit::Px;
			Token next = nextToken(tok);
			if (next.type == TokenType::Identifier) {
				std::string unitStr(next.start, next.length);
				if (unitStr == "px") {
					unit = SizeUnit::Px;
				} else if (unitStr == "%") {
					unit = SizeUnit::Percent;
				} else {
					tok.current = next.start;
				}
			} else {
				tok.current = next.start;
			}
			style.flexBasis.value = value;
			style.flexBasis.unit = unit;
		}
	} else if (std::strcmp(propName, "border-radius") == 0) {
		if (token.type == TokenType::Number) {
			int32_t value = token.intValue;
			Token next = nextToken(tok);
			if (next.type == TokenType::Identifier && std::string(next.start, next.length) == "px") {
				next = nextToken(tok);
			} else {
				tok.current = next.start;
			}
			
			// Check for shorthand (1, 2, or 4 values)
			if (next.type == TokenType::Number) {
				int32_t value2 = next.intValue;
				Token next2 = nextToken(tok);
				if (next2.type == TokenType::Identifier && std::string(next2.start, next2.length) == "px") {
					next2 = nextToken(tok);
				}
				if (next2.type == TokenType::Number) {
					// 4 values: top-left top-right bottom-right bottom-left
					int32_t value3 = next2.intValue;
					Token next3 = nextToken(tok);
					if (next3.type == TokenType::Identifier && std::string(next3.start, next3.length) == "px") {
						next3 = nextToken(tok);
					}
					if (next3.type == TokenType::Number) {
						int32_t value4 = next3.intValue;
						style.borderRadius[0] = value;   // top-left
						style.borderRadius[1] = value2;  // top-right
						style.borderRadius[2] = value3;  // bottom-right
						style.borderRadius[3] = value4;  // bottom-left
						token = next3;
						return true;
					}
				}
				// 2 values: vertical horizontal
				style.borderRadius[0] = style.borderRadius[1] = value;   // top
				style.borderRadius[2] = style.borderRadius[3] = value2;  // bottom
				token = next2;
				return true;
			}
			// 1 value: all corners
			style.borderRadius[0] = style.borderRadius[1] = 
			style.borderRadius[2] = style.borderRadius[3] = value;
			token = next;
			return true;
		}
		return false;
	
	// Consume token
	token = nextToken(tok);
	return true;
}

extern "C" {

// Parse CSS stylesheet
qboolean UI2_LoadStylesheet(ui2Context_t *ctx, const char *cssText) {
	if (!ctx || !cssText) {
		return qfalse;
	}
	
	Tokenizer tok;
	tok.init(cssText);
	
	ctx->styleSheet.reset();
	
	Token token = nextToken(tok);
	
	while (token.type != TokenType::EndOfFile) {
		// Parse selector (tag or class name)
		if (token.type != TokenType::Identifier) {
			token = nextToken(tok);
			continue;
		}
		
		std::string selector(token.start, token.length);
		uint32_t selectorId = ctx->stringTable.intern(selector.c_str());
		
		// Expect '{'
		token = nextToken(tok);
		if (token.type != TokenType::LeftBrace) {
			Com_Printf("UI2: Expected '{' after selector '%s'\n", selector.c_str());
			token = nextToken(tok);
			continue;
		}
		
		// Parse properties
		Style style;
		token = nextToken(tok);
		
		while (token.type != TokenType::RightBrace && token.type != TokenType::EndOfFile) {
			if (token.type == TokenType::Identifier) {
				std::string propName(token.start, token.length);
				token = nextToken(tok);
				
				parsePropertyValue(tok, token, style, propName.c_str());
				
				// Expect ';' or end of block
				if (token.type == TokenType::Semicolon) {
					token = nextToken(tok);
				}
			} else {
				token = nextToken(tok);
			}
		}
		
		// Add rule
		ctx->styleSheet.addRule(selectorId, style);
		
		// Skip '}'
		if (token.type == TokenType::RightBrace) {
			token = nextToken(tok);
		}
	}
	
	return qtrue;
}

} // extern "C"

#endif // __cplusplus
