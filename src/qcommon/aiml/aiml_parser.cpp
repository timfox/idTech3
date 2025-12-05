/*
=============================================================================
 Minimal AIML 2.1 XML parser
=============================================================================
*/

#include "aiml_parser.h"

#include <algorithm>
#include <cctype>
#include <stack>

namespace aiml
{
	namespace
	{
		struct XmlNode
		{
			std::string name;
			std::string text;
			std::unordered_map<std::string, std::string> attributes;
			std::vector<XmlNode> children;
		};

		std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value;
		}

		bool IsWhitespace(char c)
		{
			return std::isspace(static_cast<unsigned char>(c)) != 0;
		}

		std::string Trim(const std::string& value)
		{
			size_t start = 0;
			while (start < value.size() && IsWhitespace(value[start])) {
				++start;
			}

			size_t end = value.size();
			while (end > start && IsWhitespace(value[end - 1])) {
				--end;
			}

			return value.substr(start, end - start);
		}

		std::string CollapseWhitespace(const std::string& value)
		{
			std::string out;
			out.reserve(value.size());
			bool prevSpace = false;
			for (char c : value) {
				if (IsWhitespace(c)) {
					if (!prevSpace) {
						out.push_back(' ');
						prevSpace = true;
					}
				} else {
					out.push_back(c);
					prevSpace = false;
				}
			}
			return Trim(out);
		}

		std::unordered_map<std::string, std::string> ParseAttributes(const std::string& src)
		{
			std::unordered_map<std::string, std::string> attributes;
			size_t pos = 0;
			while (pos < src.size()) {
				while (pos < src.size() && IsWhitespace(src[pos])) {
					++pos;
				}
				if (pos >= src.size()) {
					break;
				}
				size_t keyStart = pos;
				while (pos < src.size() && src[pos] != '=' && !IsWhitespace(src[pos])) {
					++pos;
				}
				if (pos >= src.size() || src[pos] != '=') {
					break;
				}
				std::string key = ToLower(src.substr(keyStart, pos - keyStart));
				++pos; // skip '='
				if (pos < src.size() && src[pos] == '"') {
					++pos; // skip opening quote
					size_t valueStart = pos;
					while (pos < src.size() && src[pos] != '"') {
						++pos;
					}
					std::string value = src.substr(valueStart, pos - valueStart);
					if (pos < src.size()) {
						++pos; // skip closing quote
					}
					attributes[key] = value;
				} else {
					// Unquoted value until whitespace or end
					size_t valueStart = pos;
					while (pos < src.size() && !IsWhitespace(src[pos])) {
						++pos;
					}
					attributes[key] = src.substr(valueStart, pos - valueStart);
				}
			}
			return attributes;
		}

		void ParseXmlInto(XmlNode& parent, const std::string& src, size_t& pos)
		{
			while (pos < src.size()) {
				if (src[pos] == '<') {
					if (pos + 1 < src.size() && src[pos + 1] == '!') {
						// Skip comments and declarations
						size_t end = src.find("-->", pos + 4);
						if (end == std::string::npos) {
							return;
						}
						pos = end + 3;
						continue;
					}

					if (pos + 1 < src.size() && src[pos + 1] == '/') {
						// Closing tag - consume and return to caller
						size_t close = src.find('>', pos);
						if (close == std::string::npos) {
							pos = src.size();
						} else {
							pos = close + 1;
						}
						return;
					}

					size_t end = src.find('>', pos);
					if (end == std::string::npos) {
						return;
					}

					std::string tagContent = src.substr(pos + 1, end - pos - 1);
					bool selfClosing = false;
					if (!tagContent.empty() && tagContent.back() == '/') {
						selfClosing = true;
						tagContent.pop_back();
					}

					// Separate name from attributes
					size_t nameEnd = 0;
					while (nameEnd < tagContent.size() && !IsWhitespace(tagContent[nameEnd])) {
						++nameEnd;
					}
					std::string tagName = ToLower(tagContent.substr(0, nameEnd));
					std::string attrText = (nameEnd < tagContent.size()) ? tagContent.substr(nameEnd) : std::string{};

					XmlNode child;
					child.name = tagName;
					child.attributes = ParseAttributes(attrText);

					pos = end + 1;

					if (!selfClosing) {
						ParseXmlInto(child, src, pos);
					}

					parent.children.push_back(std::move(child));
				} else {
					// Text node
					size_t start = pos;
					while (pos < src.size() && src[pos] != '<') {
						++pos;
					}
					std::string raw = src.substr(start, pos - start);
					std::string collapsed = CollapseWhitespace(raw);
					if (!collapsed.empty()) {
						XmlNode textNode;
						textNode.name = "#text";
						textNode.text = collapsed;
						parent.children.push_back(std::move(textNode));
					}
				}
			}
		}

		std::string ExtractText(const XmlNode& node)
		{
			if (node.name == "#text") {
				return node.text;
			}

			std::string out;
			for (const auto& child : node.children) {
				std::string chunk = ExtractText(child);
				if (!chunk.empty()) {
					if (!out.empty()) {
						out.push_back(' ');
					}
					out += chunk;
				}
			}
			return CollapseWhitespace(out);
		}

		void AppendChildText(TemplateNode& parent, const std::string& text)
		{
			if (text.empty()) {
				return;
			}
			TemplateNode textNode;
			textNode.type = TemplateType::Text;
			textNode.text = text;
			parent.children.push_back(std::move(textNode));
		}

		TemplateNode ParseTemplate(const XmlNode& node)
		{
			if (node.name == "#text") {
				TemplateNode textNode;
				textNode.type = TemplateType::Text;
				textNode.text = node.text;
				return textNode;
			}

			std::string lowerName = ToLower(node.name);
			TemplateNode result;

			if (lowerName == "template" || lowerName == "li" || lowerName == "category") {
				result.type = TemplateType::Root;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "random") {
				result.type = TemplateType::Random;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "condition") {
				result.type = TemplateType::Condition;
				ConditionBranch base;
				auto itName = node.attributes.find("name");
				auto itValue = node.attributes.find("value");
				if (itName != node.attributes.end() && itValue != node.attributes.end()) {
					base.name = ToLower(itName->second);
					base.value = ToLower(itValue->second);
					for (const auto& child : node.children) {
						base.children.push_back(ParseTemplate(child));
					}
					result.branches.push_back(std::move(base));
					return result;
				}

				for (const auto& child : node.children) {
					if (ToLower(child.name) == "li") {
						ConditionBranch branch;
						auto nameIt = child.attributes.find("name");
						auto valIt = child.attributes.find("value");
						if (nameIt != child.attributes.end()) {
							branch.name = ToLower(nameIt->second);
						} else if (itName != node.attributes.end()) {
							branch.name = ToLower(itName->second);
						}
						if (valIt != child.attributes.end()) {
							branch.value = ToLower(valIt->second);
						}
						for (const auto& liChild : child.children) {
							branch.children.push_back(ParseTemplate(liChild));
						}
						result.branches.push_back(std::move(branch));
					} else {
						result.children.push_back(ParseTemplate(child));
					}
				}
				return result;
			}

			if (lowerName == "set") {
				result.type = TemplateType::Set;
				auto it = node.attributes.find("name");
				if (it != node.attributes.end()) {
					result.name = ToLower(it->second);
				}
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "get") {
				result.type = TemplateType::Get;
				auto it = node.attributes.find("name");
				if (it != node.attributes.end()) {
					result.name = ToLower(it->second);
				}
				return result;
			}

			if (lowerName == "bot") {
				result.type = TemplateType::Bot;
				auto it = node.attributes.find("name");
				if (it != node.attributes.end()) {
					result.name = ToLower(it->second);
				}
				return result;
			}

			if (lowerName == "srai") {
				result.type = TemplateType::Srai;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "think") {
				result.type = TemplateType::Think;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "star") {
				result.type = TemplateType::Star;
				auto it = node.attributes.find("index");
				if (it != node.attributes.end()) {
					result.index = std::max(1, std::stoi(it->second));
				}
				return result;
			}

			if (lowerName == "thatstar") {
				result.type = TemplateType::ThatStar;
				auto it = node.attributes.find("index");
				if (it != node.attributes.end()) {
					result.index = std::max(1, std::stoi(it->second));
				}
				return result;
			}

			if (lowerName == "topicstar") {
				result.type = TemplateType::TopicStar;
				auto it = node.attributes.find("index");
				if (it != node.attributes.end()) {
					result.index = std::max(1, std::stoi(it->second));
				}
				return result;
			}

			if (lowerName == "uppercase") {
				result.type = TemplateType::Uppercase;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "lowercase") {
				result.type = TemplateType::Lowercase;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "formal") {
				result.type = TemplateType::Formal;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			if (lowerName == "sentence") {
				result.type = TemplateType::Sentence;
				for (const auto& child : node.children) {
					result.children.push_back(ParseTemplate(child));
				}
				return result;
			}

			// Fallback: treat as text plus children
			result.type = TemplateType::Root;
			if (!node.text.empty()) {
				AppendChildText(result, node.text);
			}
			for (const auto& child : node.children) {
				result.children.push_back(ParseTemplate(child));
			}
			return result;
		}

		Category BuildCategory(const XmlNode& node, const std::string& topic)
		{
			Category cat;
			cat.topic = topic.empty() ? "*" : ToLower(topic);

			for (const auto& child : node.children) {
				std::string name = ToLower(child.name);
				if (name == "pattern") {
					cat.pattern = ToLower(ExtractText(child));
				} else if (name == "that") {
					cat.that = ToLower(ExtractText(child));
				} else if (name == "template") {
					cat.templ = ParseTemplate(child);
				}
			}

			if (cat.pattern.empty()) {
				cat.pattern = "*";
			}
			if (cat.templ.children.empty() && cat.templ.text.empty()) {
				cat.templ.type = TemplateType::Root;
				AppendChildText(cat.templ, ExtractText(node));
			}
			return cat;
		}
	} // namespace

	ParsedAiml AimlParser::Parse(const std::string& source) const
	{
		ParsedAiml out;
		// Some reasonable defaults for bot properties
		out.botProperties["name"] = "idtech3-bot";
		out.botProperties["version"] = "2.1";
		out.botProperties["location"] = "localhost";

		XmlNode root;
		root.name = "#root";
		size_t pos = 0;
		ParseXmlInto(root, source, pos);

		auto processCategory = [&](const XmlNode& catNode, const std::string& topicName) {
			Category cat = BuildCategory(catNode, topicName);
			out.categories.push_back(std::move(cat));
		};

		for (const auto& child : root.children) {
			std::string name = ToLower(child.name);
			if (name == "aiml") {
				for (const auto& aimlChild : child.children) {
					std::string innerName = ToLower(aimlChild.name);
					if (innerName == "topic") {
						std::string topicName = "*";
						auto it = aimlChild.attributes.find("name");
						if (it != aimlChild.attributes.end()) {
							topicName = ToLower(it->second);
						}
						for (const auto& topicChild : aimlChild.children) {
							if (ToLower(topicChild.name) == "category") {
								processCategory(topicChild, topicName);
							}
						}
					} else if (innerName == "category") {
						processCategory(aimlChild, "*");
					} else if (innerName == "bot") {
						auto it = aimlChild.attributes.find("name");
						if (it != aimlChild.attributes.end()) {
							out.botProperties[ToLower(it->second)] = ExtractText(aimlChild);
						}
					}
				}
			} else if (name == "category") {
				processCategory(child, "*");
			} else if (name == "topic") {
				std::string topicName = "*";
				auto it = child.attributes.find("name");
				if (it != child.attributes.end()) {
					topicName = ToLower(it->second);
				}
				for (const auto& topicChild : child.children) {
					if (ToLower(topicChild.name) == "category") {
						processCategory(topicChild, topicName);
					}
				}
			}
		}

		return out;
	}
} // namespace aiml

