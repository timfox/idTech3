#pragma once

#include <string>

class CStr8 : public std::string
{
public:
	using std::string::string;
	CStr8() = default;
	CStr8(const std::string& value) : std::string(value) {}
	CStr8(std::string&& value) : std::string(std::move(value)) {}
};

using CStr = CStr8;

class CStrW
{
public:
	CStrW() = default;
	CStrW(const wchar_t* value) : m_Value(value ? value : L"") {}
	CStrW(const std::wstring& value) : m_Value(value) {}

	CStr8 ToUTF8() const
	{
		std::string out;
		out.reserve(m_Value.size());
		for (wchar_t ch : m_Value)
			out.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
		return CStr8(out);
	}

private:
	std::wstring m_Value;
};

