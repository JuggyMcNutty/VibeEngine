#pragma once

#include <cstdint>
#include <string>
#include <string_view>

class StrTools
{
public:
	static bool equals_ignore_case(const std::string_view& str1, const std::string_view& str2);
	static bool startswith(const std::string_view& str, const std::string_view& cmp, bool ignoreCase = false);
	static std::string replace(const std::string& str, const std::string_view& find, const std::string_view& repl, bool ignoreCase = false);
	static std::string int_to_string(const int value, const int minWidth);
	// Finds first occurrence of ANY character given in characters in str. Returns std::string::npos if none of the characters can be found.
	static size_t find_first_of_any(const std::string& str, const std::string& characters);
	// The original's hash of a name, case aside (Core's appStrihash): its
	// CRC table (GCRCTable, polynomial 0x04C11DB7 from the top bit) run over
	// each character's two bytes, a-z in upper case. The flag base's 64
	// buckets and the AI event manager's 256 are chosen by it.
	static uint32_t ue1_strihash(const std::string_view& str);
};
