#pragma once

#include <string>

// The original's command line, as DeusEx.exe hands it to its engine: one
// string, which the fork takes from --cmdline= -- the recreated launcher's
// run-game.sh passes the player's there. Its flags are found as the
// original's code finds them (dx-reverse-info's cli-flags.md) -- Parse's "X="
// anywhere, in any case, its value up to the next space, or in double quotes
// -- but for ParseParam, stricter by choice: "-X" or "/X" anywhere, ending at
// a space or the end, where the original's checks nothing after the name.
// Both take '/' as a switch, and a Linux path is full of them: the original's
// rule would find -server in INI=/srv/server/x.ini. What the fork does with
// each flag is NATIVES.md's "The command line".
class OriginalCommandLine
{
public:
	static OriginalCommandLine& Get();

	void Set(const std::string& line);
	bool Given() const { return given; }
	const std::string& Line() const { return line; }

	bool Param(const std::string& name) const;
	bool Value(const std::string& name, std::string& value) const;

	// UGameEngine::Init's start URL: the first token -- the second when the
	// first is SERVER -- but only with -hax0r or -server, and not a token
	// starting with '-'. Empty for the game's own start (DX.dx).
	std::string StartURL() const;

private:
	std::string line;
	bool given = false;
};
