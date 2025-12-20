// AIML 2.1 interpreter core written in C++23.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "aiml_types.h"

namespace aiml
{
	class AimlInterpreter
	{
	public:
		AimlInterpreter();

		bool LoadFromString(const std::string& name, const std::string& aimlText);
		void Clear();

		std::string Reply(const std::string& userId, const std::string& input);

		void SetBotPredicate(const std::string& key, const std::string& value);
		std::string GetBotPredicate(const std::string& key) const;

		void ResetSession(const std::string& userId);
		void ResetAllSessions();

		bool HasCategories() const { return !categories_.empty(); }

	private:
		struct MatchContext
		{
			std::vector<std::string> stars;
			std::vector<std::string> thatStars;
			std::vector<std::string> topicStars;
		};

		struct MatchScore
		{
			int literalCount{ 0 };
			int wildcardCount{ 0 };
			int patternSize{ 0 };
		};

		std::string ReplyInternal(const std::string& userId, const std::string& input, int depth);

		bool MatchCategory(const Category& cat, const std::vector<std::string>& inputTokens, const SessionState& session, MatchContext& outContext, MatchScore& outScore) const;
		bool MatchTokens(const std::vector<std::string>& pattern, const std::vector<std::string>& words, size_t pi, size_t wi, std::vector<std::string>& stars) const;

		std::string EvaluateTemplate(const TemplateNode& node, SessionState& session, const MatchContext& match, const std::string& userId, int depth);

		std::vector<std::string> Tokenize(const std::string& text) const;
		std::string Normalize(const std::string& text) const;
		std::string CollapseWhitespace(const std::string& text) const;
		std::string JoinWords(const std::vector<std::string>& words, size_t begin, size_t end) const;
		std::string ToCase(const std::string& value, TemplateType type) const;

		std::unordered_map<std::string, SessionState> sessions_;
		std::vector<Category> categories_;
		std::unordered_map<std::string, std::string> botProperties_;
	};
} // namespace aiml

