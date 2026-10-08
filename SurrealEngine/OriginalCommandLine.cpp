#include "Precomp.h"
#include "OriginalCommandLine.h"
#include <cctype>

namespace
{
	bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

	// appStrfind: a substring in any case, from pos; npos when there is none.
	size_t FindNoCase(const std::string& hay, const std::string& needle, size_t pos)
	{
		if (needle.empty())
			return std::string::npos;
		for (size_t i = pos; i + needle.size() <= hay.size(); i++)
		{
			size_t k = 0;
			while (k < needle.size() && std::tolower((unsigned char)hay[i + k]) == std::tolower((unsigned char)needle[k]))
				k++;
			if (k == needle.size())
				return i;
		}
		return std::string::npos;
	}

	// ParseToken: the next word, or a quoted run; false at the end.
	bool NextToken(const std::string& s, size_t& pos, std::string& token)
	{
		while (pos < s.size() && IsSpace(s[pos]))
			pos++;
		if (pos >= s.size())
			return false;
		token.clear();
		if (s[pos] == '"')
		{
			pos++;
			while (pos < s.size() && s[pos] != '"')
				token += s[pos++];
			if (pos < s.size())
				pos++;
		}
		else
		{
			while (pos < s.size() && !IsSpace(s[pos]))
				token += s[pos++];
		}
		return true;
	}
}

OriginalCommandLine& OriginalCommandLine::Get()
{
	static OriginalCommandLine instance;
	return instance;
}

void OriginalCommandLine::Set(const std::string& value)
{
	line = value;
	given = true;
}

bool OriginalCommandLine::Param(const std::string& name) const
{
	// Past the line's first character, as ParseParam starts its search there.
	// The name ending at a space or the end is the fork's own test, by choice
	// (OriginalCommandLine.h): the original's checks nothing after it.
	for (size_t at = FindNoCase(line, name, 1); at != std::string::npos; at = FindNoCase(line, name, at + 1))
	{
		char before = line[at - 1];
		size_t end = at + name.size();
		if ((before == '-' || before == '/') && (end == line.size() || IsSpace(line[end])))
			return true;
	}
	return false;
}

bool OriginalCommandLine::Value(const std::string& name, std::string& value) const
{
	size_t at = FindNoCase(line, name + "=", 0);
	if (at == std::string::npos)
		return false;
	size_t pos = at + name.size() + 1;
	value.clear();
	if (pos < line.size() && line[pos] == '"')
	{
		pos++;
		while (pos < line.size() && line[pos] != '"')
			value += line[pos++];
	}
	else
	{
		while (pos < line.size() && !IsSpace(line[pos]))
			value += line[pos++];
	}
	return true;
}

std::string OriginalCommandLine::StartURL() const
{
	if (!Param("hax0r") && !Param("server"))
		return {};
	size_t pos = 0;
	std::string token;
	if (!NextToken(line, pos, token))
		return {};
	if (FindNoCase(token, "SERVER", 0) == 0 && token.size() == 6 && !NextToken(line, pos, token))
		return {};
	if (token.empty() || token[0] == '-')
		return {};
	return token;
}
