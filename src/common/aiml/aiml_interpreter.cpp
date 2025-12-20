/*
=============================================================================
 AIML 2.1 interpreter core
=============================================================================
*/

#include "aiml_interpreter.h"

#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>

#include "aiml_parser.h"

namespace aiml
{
	namespace
	{
		std::string ToUpper(const std::string& value)
		{
			std::string out = value;
			std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			return out;
		}

		std::string ToLower(const std::string& value)
		{
			std::string out = value;
			std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}
	} // namespace

	AimlInterpreter::AimlInterpreter()
	{
		botProperties_["name"] = "idtech3-bot";
		botProperties_["version"] = "2.1";
		botProperties_["location"] = "localhost";
	}

	bool AimlInterpreter::LoadFromString(const std::string& name, const std::string& aimlText)
	{
		(void)name; // reserved for future differentiation
		AimlParser parser;
		ParsedAiml parsed = parser.Parse(aimlText);

		if (parsed.categories.empty()) {
			return false;
		}

		for (auto& cat : parsed.categories) {
			cat.pattern = ToUpper(Normalize(cat.pattern));
			cat.topic = ToUpper(Normalize(cat.topic.empty() ? "*" : cat.topic));
			cat.that = ToUpper(Normalize(cat.that));
			categories_.push_back(std::move(cat));
		}

		for (const auto& kv : parsed.botProperties) {
			botProperties_[ToLower(kv.first)] = kv.second;
		}

		return true;
	}

	void AimlInterpreter::Clear()
	{
		categories_.clear();
		sessions_.clear();
	}

	void AimlInterpreter::SetBotPredicate(const std::string& key, const std::string& value)
	{
		botProperties_[ToLower(key)] = value;
	}

	std::string AimlInterpreter::GetBotPredicate(const std::string& key) const
	{
		auto it = botProperties_.find(ToLower(key));
		if (it != botProperties_.end()) {
			return it->second;
		}
		return {};
	}

	void AimlInterpreter::ResetSession(const std::string& userId)
	{
		sessions_.erase(userId);
	}

	void AimlInterpreter::ResetAllSessions()
	{
		sessions_.clear();
	}

	std::string AimlInterpreter::CollapseWhitespace(const std::string& text) const
	{
		std::string out;
		out.reserve(text.size());
		bool prevSpace = false;
		for (char c : text) {
			if (std::isspace(static_cast<unsigned char>(c))) {
				if (!prevSpace) {
					out.push_back(' ');
					prevSpace = true;
				}
			} else {
				out.push_back(c);
				prevSpace = false;
			}
		}
		if (!out.empty() && out.front() == ' ') {
			out.erase(out.begin());
		}
		if (!out.empty() && out.back() == ' ') {
			out.pop_back();
		}
		return out;
	}

	std::string AimlInterpreter::Normalize(const std::string& text) const
	{
		std::string collapsed = CollapseWhitespace(text);
		std::string out;
		out.reserve(collapsed.size());
		for (char c : collapsed) {
			if (std::isalnum(static_cast<unsigned char>(c)) || c == '*' || c == '_' || c == ' ') {
				out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
			} else {
				out.push_back(' ');
			}
		}
		return CollapseWhitespace(out);
	}

	std::vector<std::string> AimlInterpreter::Tokenize(const std::string& text) const
	{
		std::vector<std::string> tokens;
		std::string current;
		for (char c : text) {
			if (c == ' ') {
				if (!current.empty()) {
					tokens.push_back(current);
					current.clear();
				}
				continue;
			}

			if (std::isalnum(static_cast<unsigned char>(c)) || c == '*' || c == '_') {
				current.push_back(c);
			}
		}
		if (!current.empty()) {
			tokens.push_back(current);
		}
		return tokens;
	}

	std::string AimlInterpreter::JoinWords(const std::vector<std::string>& words, size_t begin, size_t end) const
	{
		if (begin >= end || begin >= words.size()) {
			return {};
		}
		end = std::min(end, words.size());
		std::ostringstream oss;
		for (size_t i = begin; i < end; ++i) {
			if (i > begin) {
				oss << ' ';
			}
			oss << words[i];
		}
		return oss.str();
	}

	std::string AimlInterpreter::ToCase(const std::string& value, TemplateType type) const
	{
		if (value.empty()) {
			return value;
		}

		if (type == TemplateType::Lowercase) {
			std::string out = value;
			std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return out;
		}

		if (type == TemplateType::Uppercase) {
			return ToUpper(value);
		}

		if (type == TemplateType::Formal) {
			std::string out = ToLower(value);
			bool startWord = true;
			for (char& c : out) {
				if (std::isspace(static_cast<unsigned char>(c))) {
					startWord = true;
				} else if (startWord) {
					c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
					startWord = false;
				}
			}
			return out;
		}

		if (type == TemplateType::Sentence) {
			std::string out = ToLower(value);
			if (!out.empty()) {
				out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
			}
			return out;
		}

		return value;
	}

	bool AimlInterpreter::MatchTokens(const std::vector<std::string>& pattern, const std::vector<std::string>& words, size_t pi, size_t wi, std::vector<std::string>& stars) const
	{
		if (pi == pattern.size()) {
			return wi == words.size();
		}

		const std::string& token = pattern[pi];
		if (token == "*" || token == "_") {
			for (size_t consume = 0; wi + consume <= words.size(); ++consume) {
				std::vector<std::string> tempStars = stars;
				tempStars.push_back(JoinWords(words, wi, wi + consume));
				if (MatchTokens(pattern, words, pi + 1, wi + consume, tempStars)) {
					stars = std::move(tempStars);
					return true;
				}
			}
			return false;
		}

		if (wi >= words.size()) {
			return false;
		}

		if (token != words[wi]) {
			return false;
		}

		return MatchTokens(pattern, words, pi + 1, wi + 1, stars);
	}

	bool AimlInterpreter::MatchCategory(const Category& cat, const std::vector<std::string>& inputTokens, const SessionState& session, MatchContext& outContext, MatchScore& outScore) const
	{
		if (!cat.topic.empty() && cat.topic != "*" && cat.topic != ToUpper(Normalize(session.topic))) {
			return false;
		}

		std::vector<std::string> patternTokens = Tokenize(cat.pattern);
		std::vector<std::string> stars;
		if (!MatchTokens(patternTokens, inputTokens, 0, 0, stars)) {
			return false;
		}

		std::vector<std::string> thatStars;
		if (!cat.that.empty()) {
			std::vector<std::string> thatPattern = Tokenize(cat.that);
			std::vector<std::string> thatInput = Tokenize(Normalize(session.lastResponse));
			if (!MatchTokens(thatPattern, thatInput, 0, 0, thatStars)) {
				return false;
			}
		}

		outContext.stars = std::move(stars);
		outContext.thatStars = std::move(thatStars);
		outContext.topicStars.clear();

		outScore.literalCount = 0;
		outScore.wildcardCount = 0;
		outScore.patternSize = static_cast<int>(patternTokens.size());

		for (const auto& tok : patternTokens) {
			if (tok == "*" || tok == "_") {
				outScore.wildcardCount++;
			} else {
				outScore.literalCount++;
			}
		}

		return true;
	}

	std::string AimlInterpreter::EvaluateTemplate(const TemplateNode& node, SessionState& session, const MatchContext& match, const std::string& userId, int depth)
	{
		auto evalChildren = [&](const std::vector<TemplateNode>& nodes) {
			std::string out;
			for (const auto& child : nodes) {
				out += EvaluateTemplate(child, session, match, userId, depth);
			}
			return out;
		};

		switch (node.type) {
		case TemplateType::Text:
			return node.text;
		case TemplateType::Root:
			return evalChildren(node.children);
		case TemplateType::Random: {
			if (node.children.empty()) {
				return {};
			}
			static std::mt19937 rng{ std::random_device{}() };
			std::uniform_int_distribution<size_t> dist(0, node.children.size() - 1);
			const auto& choice = node.children[dist(rng)];
			return EvaluateTemplate(choice, session, match, userId, depth);
		}
		case TemplateType::Condition: {
			for (const auto& branch : node.branches) {
				if (branch.name.empty()) {
					continue;
				}
				auto it = session.predicates.find(branch.name);
				std::string value = (it != session.predicates.end()) ? ToLower(CollapseWhitespace(it->second)) : "";
				if (branch.value.empty()) {
					continue;
				}
				if (value == ToLower(branch.value)) {
					return evalChildren(branch.children);
				}
			}
			if (!node.children.empty()) {
				return evalChildren(node.children);
			}
			return {};
		}
		case TemplateType::Set: {
			std::string value = evalChildren(node.children);
			if (!node.name.empty()) {
				session.predicates[node.name] = value;
				if (node.name == "topic") {
					session.topic = Normalize(value);
				}
			}
			return value;
		}
		case TemplateType::Get: {
			if (node.name.empty()) {
				return {};
			}
			auto it = session.predicates.find(node.name);
			if (it != session.predicates.end()) {
				return it->second;
			}
			return {};
		}
		case TemplateType::Bot: {
			auto it = botProperties_.find(node.name);
			if (it != botProperties_.end()) {
				return it->second;
			}
			return {};
		}
		case TemplateType::Srai: {
			std::string query = evalChildren(node.children);
			return ReplyInternal(userId, query, depth + 1);
		}
		case TemplateType::Think: {
			(void)evalChildren(node.children);
			return {};
		}
		case TemplateType::Star: {
			int idx = node.index - 1;
			if (idx >= 0 && idx < static_cast<int>(match.stars.size())) {
				return match.stars[idx];
			}
			return {};
		}
		case TemplateType::ThatStar: {
			int idx = node.index - 1;
			if (idx >= 0 && idx < static_cast<int>(match.thatStars.size())) {
				return match.thatStars[idx];
			}
			return {};
		}
		case TemplateType::TopicStar: {
			int idx = node.index - 1;
			if (idx >= 0 && idx < static_cast<int>(match.topicStars.size())) {
				return match.topicStars[idx];
			}
			return {};
		}
		case TemplateType::Lowercase:
		case TemplateType::Uppercase:
		case TemplateType::Formal:
		case TemplateType::Sentence:
			return ToCase(evalChildren(node.children), node.type);
		default:
			return evalChildren(node.children);
		}
	}

	std::string AimlInterpreter::ReplyInternal(const std::string& userId, const std::string& input, int depth)
	{
		if (depth > 5) {
			return {};
		}

		std::string normalized = Normalize(input);
		std::vector<std::string> inputTokens = Tokenize(normalized);

		SessionState& session = sessions_[userId];
		if (session.topic.empty()) {
			session.topic = "*";
		}

		MatchContext bestContext;
		MatchScore bestScore{ -1, 0, 0 };
		const Category* bestCategory = nullptr;

		for (const auto& cat : categories_) {
			MatchContext ctx;
			MatchScore score;
			if (MatchCategory(cat, inputTokens, session, ctx, score)) {
				if (!bestCategory ||
				    score.literalCount > bestScore.literalCount ||
				    (score.literalCount == bestScore.literalCount && score.wildcardCount < bestScore.wildcardCount) ||
				    (score.literalCount == bestScore.literalCount && score.wildcardCount == bestScore.wildcardCount && score.patternSize > bestScore.patternSize)) {
					bestCategory = &cat;
					bestContext = std::move(ctx);
					bestScore = score;
				}
			}
		}

		if (!bestCategory) {
			return {};
		}

		std::string reply = EvaluateTemplate(bestCategory->templ, session, bestContext, userId, depth);
		reply = CollapseWhitespace(reply);
		session.lastInput = normalized;
		session.lastResponse = reply;
		return reply;
	}

	std::string AimlInterpreter::Reply(const std::string& userId, const std::string& input)
	{
		return ReplyInternal(userId, input, 0);
	}
} // namespace aiml

