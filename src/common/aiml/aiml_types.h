// Lightweight AIML type definitions shared by the parser and interpreter.
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aiml
{
	enum class TemplateType
	{
		Text,
		Root,
		Random,
		Condition,
		Set,
		Get,
		Bot,
		Srai,
		Think,
		Star,
		ThatStar,
		TopicStar,
		Lowercase,
		Uppercase,
		Formal,
		Sentence
	};

	struct ConditionBranch
	{
		std::string name;
		std::string value;
		std::vector<struct TemplateNode> children;
	};

	struct TemplateNode
	{
		TemplateType type{ TemplateType::Text };
		std::string text;
		std::string name;
		std::string value;
		int index{ 1 };
		std::vector<TemplateNode> children;
		std::vector<ConditionBranch> branches;
	};

	struct Category
	{
		std::string pattern;
		std::string that;
		std::string topic;
		TemplateNode templ;
	};

	struct SessionState
	{
		std::string topic{ "*" };
		std::string lastInput;
		std::string lastResponse;
		std::unordered_map<std::string, std::string> predicates;
	};

	struct ParsedAiml
	{
		std::vector<Category> categories;
		std::unordered_map<std::string, std::string> botProperties;
	};
} // namespace aiml

