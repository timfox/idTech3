#pragma once

#include <charconv>
#include <cstddef>
#include <string>

namespace PS
{
class StringBuilder
{
public:
	struct Buffer
	{
		char* begin;
		char* end;
	};

	explicit StringBuilder(Buffer) {}

	void Append(char value)
	{
		m_Value.push_back(value);
	}

	void Append(unsigned int value)
	{
		char buffer[32];
		const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
		m_Value.append(buffer, result.ptr);
	}

	void Append(int value)
	{
		char buffer[32];
		const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
		m_Value.append(buffer, result.ptr);
	}

	const std::string& Str() const
	{
		return m_Value;
	}

private:
	std::string m_Value;
};
}

