// Minimal AIML 2.1 XML parser that produces normalized categories.
#pragma once

#include "aiml_types.h"

namespace aiml
{
	class AimlParser
	{
	public:
		ParsedAiml Parse(const std::string& source) const;
	};
} // namespace aiml

